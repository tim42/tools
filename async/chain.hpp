//
// created by : Timothée Feuillet
// date: 2021-11-28
//
//
// Copyright (c) 2021 Timothée Feuillet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include <functional>
#include <optional>
#include <tuple>

#include "../debug/assert.hpp"

#ifndef N_ASYNC_USE_TASK_MANAGER
  #define N_ASYNC_USE_TASK_MANAGER true
#endif // N_ASYNC_USE_TASK_MANAGER

#if N_ASYNC_USE_TASK_MANAGER
#include "../threading/task_manager.hpp"
#endif

// waiting for std::move_only_function to be availlable, here is a ~drop-in replacement
#include "internal_do_not_use_cpp23_replacement/function2.hpp"

#if N_ASYNC_USE_TASK_MANAGER
#define N_ASYNC_FWD_PARAMS tm, group_id,
#else
#define N_ASYNC_FWD_PARAMS
#endif

namespace neam::async
{
  /// \brief Represent a list of chainable actions for asynchronous execution. A bit like a promise, but without the name.
  /// \note It has strict ownership rules and a two component part.
  ///       The chain (which is to be returned) and the state (which is to be stored and allows to trigger the action)
  ///
  /// \warning If using the task manager, references might become dangling (r-value refs will not, but l-value might).
  ///          Mind the scope.
  ///
  /// \todo Maybe make it compatible with the needlessly complex C++ corountines? If that makes the code easier?
  template<typename... Args>
  class chain
  {
    private: // template utilities:
      template<typename X> struct is_chain : public std::false_type {};
      template<typename... X> struct is_chain<chain<X...>> : public std::true_type {};

      template<typename T> struct remove_rvalue_reference      {using type = T;};
      template<typename T> struct remove_rvalue_reference<T&&> {using type = T;};
      template<typename T> using remove_rvalue_reference_t = typename remove_rvalue_reference<T>::type;

      template<typename T> struct wrap_arg { using type = T; };
      template<typename T> struct wrap_arg<T&> { using type = std::reference_wrapper<T>; };
      template<typename T> struct wrap_arg<const T&> { using type = std::reference_wrapper<const T>; };
      template<typename T> using wrap_arg_t = typename wrap_arg<T>::type;

      using completed_args_t = std::optional<std::tuple<wrap_arg_t<remove_rvalue_reference_t<Args>>...>>;

      /// \brief Probably the only easy way to avoid all the complexity with locks
      /// This lock is shared with the state and the chain.
      struct shared_lock_state_t
      {
        spinlock lock;
        std::atomic<unsigned> counter = 0;

        static shared_lock_state_t* create() { return new shared_lock_state_t{{}, 1}; }
        void acquire() { if (counter++ == 0) check::debug::n_assert(false, "invalid state detected"); }
        void release() { if (--counter == 0) {delete this;} }
      };
      struct shared_lock_t
      {
        shared_lock_t() : state(shared_lock_state_t::create()) {}
        shared_lock_t(std::nullptr_t) : state(nullptr) {}
        ~shared_lock_t() { if (state) { state.load(std::memory_order::relaxed)->release(); } state.store(nullptr, std::memory_order::relaxed); }
        shared_lock_t(const shared_lock_t& o) : state(o.state.load(std::memory_order::relaxed))
        {
          if (state) state.load(std::memory_order::relaxed)->acquire();
        }
        shared_lock_t(shared_lock_t&& o) : state(o.state) { o.state.store(nullptr, std::memory_order::relaxed); }
        shared_lock_t& operator = (const shared_lock_t& o)
        {
          if (&o == this) return *this;
          if (state) state.load(std::memory_order::relaxed)->release();
          state.store(o.state.load(std::memory_order::relaxed), std::memory_order::relaxed);
          if (state) state.load(std::memory_order::relaxed)->acquire();
          return *this;
        }
        shared_lock_t& operator = (shared_lock_t&& o)
        {
          if (&o == this) return *this;
          state.store(o.state.load(std::memory_order::relaxed), std::memory_order::relaxed);
          o.state.store(nullptr, std::memory_order::relaxed);
          return *this;
        }

        // lock must be held
        void drop() { if (state) state.load(std::memory_order::relaxed)->release(); state.store(nullptr, std::memory_order::relaxed); }

        // because the value of state can change during the waiting-for-lock period
        // we must have a specific way to lock the lock
        void lock()
        {
          while (true)
          {
            if (try_lock())
              return;
            while (state.load(std::memory_order::relaxed)->lock._relaxed_test());
          }
        }

        void unlock()
        {
          check::debug::n_assert(locked_state != nullptr, "cannot unlock an already unlocked state");
          locked_state->lock.unlock();
          locked_state = nullptr;
        }

        bool try_lock()
        {
          check::debug::n_assert(locked_state == nullptr, "cannot lock an already locked state");

          shared_lock_state_t* state_cpy = state.load(std::memory_order::relaxed);
          if (state_cpy->lock.try_lock())
          {
            locked_state = state_cpy;
            return true;
          }
          return false;
        }

        operator bool() const { return state != nullptr; }

        std::atomic<shared_lock_state_t*> state = nullptr;
        shared_lock_state_t* locked_state = nullptr;
      };

    public:
      class state
      {
        public:
          state() = default;

          chain create_chain()
          {
            check::debug::n_assert(link == nullptr, "create_chain called when a chain is already in use");
            chain ret(*this);
            link = &ret;
            return ret;
          }

          void complete(Args... args)
          {
            lock.lock();
            check::debug::n_assert(is_completed == false, "double state completion detected");
            is_completed = true;

            if (on_completed)
            {
              lock.unlock();
#if N_ASYNC_USE_TASK_MANAGER
              if (tm != nullptr)
              {
                // small gymnastic to still forward everything and not loose stuff around.
                tm->get_task(group_id, [...args = std::forward<wrap_arg_t<Args>>(args), on_completed = std::move(on_completed)]() mutable
                {
                  on_completed(std::forward<Args>(args)...);
                });
              }
              else
#endif
              {
                // we call the function in the same context
                on_completed(std::forward<Args>(args)...);
              }
              return;
            }
            std::lock_guard _lg(lock, std::adopt_lock);

            if (link)
            {
              link->completed_args.emplace(std::forward<Args>(args)...);
              link->link = nullptr;
              link = nullptr;
              return;
            }

            // It's not an error to not set the callback
//             check::debug::n_assert(false, "Unable to complete as the chain has be destructed without a callback ever being set");
          }
          void operator()(Args... args) { complete(std::forward<Args>(args)...); }

          ~state()
          {
            if (!lock) return;
            std::lock_guard _lg(lock);
            if (link)
            {
              link->link = nullptr;
              link = nullptr;
            }
          }

          state(state&& o) : lock(nullptr)
          {
            *this = std::move(o);
          }

          state& operator = (state&& o)
          {
            if (&o == this) return *this;

            if (lock)
            {
              // release current state:
              std::lock_guard _lg(lock);
              if (link)
              {
                link->link = nullptr;
              }
            }

            lock = o.lock;
            if (!lock) return *this;

            std::lock_guard _lg(lock);

            canceled = o.canceled;
            on_cancel_cb = std::move(o.on_cancel_cb);
            is_completed = o.is_completed;
            link = o.link;
            if (link)
              link->link = this;
            o.link = nullptr;

            on_completed = std::move(o.on_completed);
#if N_ASYNC_USE_TASK_MANAGER
            tm = o.tm;
            group_id = o.group_id;
#endif

            return *this;
          }

          bool is_canceled() const { return canceled; }

          template<typename Func>
          void on_cancel(Func&& func)
          {
            std::lock_guard _lg(lock);
            if (canceled)
            {
              func();
              return;
            }
            on_cancel_cb = std::move(func);
          }

#if N_ASYNC_USE_TASK_MANAGER
          void set_default_deferred_info(threading::task_manager* _tm, threading::group_t _group_id)
          {
            std::lock_guard _lg(lock);
            set_deferred_info(_tm, _group_id);
          }
#endif

        private:
          explicit state(chain& _link) : link(&_link), lock(_link.lock) {}
#if N_ASYNC_USE_TASK_MANAGER
          void set_deferred_info(threading::task_manager* _tm, threading::group_t _group_id)
          {
            if (_tm != nullptr)
            {
              tm = _tm;
              group_id = _group_id;
            }
          }
#endif

          void cancel()
          {
            // lock must be held
            if (!canceled)
            {
              canceled = true;
              if (on_cancel_cb)
              {
                on_cancel_cb();
              }
            }
          }

          chain* link = nullptr;
//           std::move_only_function
          fu2::unique_function<void(Args...)> on_completed;
          fu2::unique_function<void()> on_cancel_cb;

          bool canceled = false;
          bool is_completed = false;

          shared_lock_t lock;

#if N_ASYNC_USE_TASK_MANAGER
          threading::task_manager* tm = nullptr;
          threading::group_t group_id = threading::k_invalid_task_group;
#endif

          friend class chain;
      };

      chain() = default;

      state create_state()
      {
        check::debug::n_assert(link == nullptr, "create_state called when a state is already in use");
        state ret(*this);
        link = &ret;
        return ret;
      }

      static chain create_and_complete(Args... args)
      {
        chain ret;
        ret.create_state().complete(std::forward<Args>(args)...);
        return ret;
      }

      template<typename Func>
      void then_void(
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        Func&& cb)
      {
        // use_state is faster than then, as is re-links the chain and state
        if constexpr (std::is_same_v<Func, state>)
          return use_state(N_ASYNC_FWD_PARAMS std::move(cb));


        using ret_type = typename std::result_of<Func(Args...)>::type;
        static_assert(std::is_same_v<ret_type, void>, "return type is not void");

        lock.lock();

        check::debug::n_assert(has_completion_set == false, "Double call to then/then_void/use_state detected");
        has_completion_set = true;

        if (completed_args)
        {
          if (link) link->link = nullptr;
          lock.unlock(); // call without the lock held
#if N_ASYNC_USE_TASK_MANAGER
          call_with_completed_args(completed_args, tm, group_id, cb);
#else
          call_with_completed_args(completed_args, cb);
#endif
          return;
        }
        std::lock_guard _lg(lock, std::adopt_lock);
        if (link != nullptr)
        {
#if N_ASYNC_USE_TASK_MANAGER
          if (tm != nullptr)
            link->set_deferred_info(tm, group_id);
#endif
          link->on_completed = std::move(cb);
          return;
        }

        check::debug::n_assert(false, "Unable to call the callback as the state has been destructed without being completed");
      }
#if N_ASYNC_USE_TASK_MANAGER
      template<typename Func>
      void then_void(Func&& cb)
      {
        return then_void(nullptr, threading::k_invalid_task_group, std::forward<Func>(cb));
      }
      template<typename Func>
      void then_void(threading::task_manager& tm, threading::group_t group_id, Func&& cb)
      {
        return then_void(&tm, group_id, std::forward<Func>(cb));
      }
      template<typename Func>
      void then_void(threading::task_manager& tm, Func&& cb)
      {
        return then_void(&tm, tm.get_current_group(), std::forward<Func>(cb));
      }
#endif

      /// \brief complete the provided state on completion.
      /// re-link state and chain so that the original state will trigger the final chain (no recursion in there)
      void use_state(
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        state& other)
      {
        if (&other == link)
          return;

        lock.lock();

        check::debug::n_assert(has_completion_set == false, "Double call to then/then_void/use_state detected");
        has_completion_set = true;

        {
          // we are already completed, simply complete the state
          if (completed_args)
          {
            if (link) link->link = nullptr; // unlink
            lock.unlock(); // call without the lock held
            // this handles the case where the other chain is alive or the callback has been set
            // move around the args and call without the lock held
#if N_ASYNC_USE_TASK_MANAGER
            call_with_completed_args(completed_args, tm, group_id, other);
#else
            call_with_completed_args(completed_args, other);
#endif
            return;
          }
        }

        std::lock_guard _lg(lock, std::adopt_lock);

        // else, we are not completed
        {
          check::debug::n_assert(link != nullptr, "Unable to chain the states as the state has been destructed without being completed");
          check::debug::n_assert(link->is_completed == false, "use_state: current state is already completed");
          check::debug::n_assert(link->on_completed == nullptr, "cannot call both then() and use_state()");
          check::debug::n_assert(other.lock, "invalid state to use");

          {
            std::lock_guard _olg(other.lock);
            check::debug::n_assert(other.is_completed == false, "use_state: cannot use already completed state");

            // other state/chain are stale, no need to do anything
            if (!other.on_completed && other.link == nullptr)
              return;

            state* target_state = link;
            chain* target_chain = other.link;

            // move the on_completed callback around, if it exists
            if (other.on_completed)
            {
              target_state->on_completed = std::move(other.on_completed);
#if N_ASYNC_USE_TASK_MANAGER
              if (other.tm != nullptr)
                target_state->set_deferred_info(other.tm, other.group_id);
#endif
              other.on_completed = nullptr;
              return;
            }
            // we go from chain{target}<->state{other} -> chain{this}<->state{target}
            //       to   chain{target}               <->               state{target}
            // and left chain{this} and state{other} be stale and un-completable

            target_state->link = target_chain;
            if (target_chain)
            {
              target_chain->link = target_state;
//               target_chain->lock = target_state->lock;
              target_state->lock = target_chain->lock;
            }

            // unlink ourselves (and other)
            other.link = nullptr;
            link = nullptr;
          }
        }
      }
#if N_ASYNC_USE_TASK_MANAGER
      void use_state(state& other)
      {
        return use_state(nullptr, threading::k_invalid_task_group, other);
      }
      void use_state(threading::task_manager& tm, threading::group_t group_id, state& other)
      {
        return use_state(&tm, group_id, other);
      }
      void use_state(threading::task_manager& tm, state& other)
      {
        return use_state(&tm, tm.get_current_group(), other);
      }
#endif


      /// \brief Chain from a potentially not-contructed state
      /// This allows for flat (co_await style) code:
      ///   some_async_function(...)
      ///     .then([](...) { return some_other_async_function(...); })
      ///     .then([](...) { return some_other_other_async_function(...); })
      ///     ...;
      ///  \note the chain types can be different allowing for more flexibility
      ///  \note valid return types are:
      ///   - void: same as calling then() with a continuation chain (chain<>)
      ///   - chain< ... >, allow chaining from the return value as if the callback was called in the current scope
      ///   - any_other_type_t: return a chain< any_other_type_t&& > / chain< any_other_type_t > 
      ///     If any_other_type_t is a std::is_trivially_copyable<> or don't have a move constructor, it's a plain type, otherwise it's a &&
      template<typename Func>
      auto then(
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        Func&& cb) -> auto
      {
        using ret_type = std::invoke_result_t<Func, Args...>;
        if constexpr (std::is_same_v<ret_type, void>)
        {
          chain<> ret;
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            cb(std::forward<Args>(args)...);
            state.complete();
          });
          return ret;

//           return then_void(N_ASYNC_FWD_PARAMS std::forward<Func>(cb));
        }
        else if constexpr (is_chain<ret_type>::value)
        {
          // Do all the gymnastic internally.
          // The state is kept alive as a lambda capture
          ret_type ret;
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            // link the states together/chains:
            // we could use then() but the draw-back of then() is recursion.
            // use_state does not perform any recursion and will instead flatten any recursive chains automatically
            cb(std::forward<Args>(args)...).use_state(/*std::move*/(state));
//             cb(std::forward<Args>(args)...).then(std::move(state));
          });
          return ret;
        }
        else // return a simple chain where the only argument is a r-value of the return type
        {
          using chain_type = std::conditional_t<std::is_trivially_copyable_v<ret_type>, ret_type, ret_type&&>;
          chain<chain_type> ret;
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            // link the states together/chains:
            state.complete(cb(std::forward<Args>(args)...));
          });
          return ret;
        }
#undef N_ASYNC_FWD_PARAMS
      }

#if N_ASYNC_USE_TASK_MANAGER
      template<typename Func>
      auto then(Func&& cb) -> auto
      {
        return then(nullptr, threading::k_invalid_task_group, std::forward<Func>(cb));
      }
      template<typename Func>
      auto then(threading::task_manager& tm, threading::group_t group_id, Func&& cb) -> auto
      {
        return then(&tm, group_id, std::forward<Func>(cb));
      }
      template<typename Func>
      auto then(threading::task_manager& tm, Func&& cb) -> auto
      {
        return then(&tm, tm.get_current_group(), std::forward<Func>(cb));
      }
#endif

      /// \brief return a continuation_chain from the current chain
      chain<> to_continuation()
      {
        return then([](Args...)
        {
          return chain<>::create_and_complete();
        });
      }

      ~chain()
      {
        if (!lock.state) return;

        std::lock_guard _lg(lock);
        if (link != nullptr)
        {
          link->link = nullptr;
          link = nullptr;
        }
      }

      chain(chain&& o) : lock(nullptr)
      {
        *this = std::move(o);
      }

      chain& operator = (chain&&o)
      {
        if (&o == this) return *this;

        if (lock)
        {
          // release the state:
          std::lock_guard _lg(lock);
          if (link)
          {
            link->link = nullptr;
          }
        }

        lock = o.lock;
        if (!lock) return *this;
        std::lock_guard _lg(lock);
        has_completion_set = o.has_completion_set;
        link = o.link;
        if (link)
          link->link = this;
        o.link = nullptr;

        completed_args = std::move(o.completed_args);
        o.completed_args.reset();
        return *this;
      }

      /// \brief cancel the current chain.
      /// \warning cancel does not bubble up, but instead cancel the current.
      ///          (it would require a separate chain just to handle cancels, chain that would also have to handle cancelations...)
      ///          chain<  > c = do_async_stuff()
      ///          chain<  > d = c.then( ... );
      ///          d.cancel() // does only cancel d, not c.
      ///          c.cancel() // cancel c, (d might still be completed/called, depending on how the state is handled)
      ///
      /// How cancelation is handled depend on how the state is handled.
      /// The code handling the state might not be able to cancel the thing, or decide to still (or not) call the completion callback.
      void cancel()
      {
        std::lock_guard<shared_lock_t> _lg(lock);
        if (link != nullptr)
        {
          link->cancel();
        }
      }

    private:
      template<typename Func>
      static void call_with_completed_args(
        completed_args_t& args,
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        Func&& func)
      {
        [=, args = std::move(args)]<size_t... Indices>(Func&& func, std::index_sequence<Indices...>) mutable
        {
#if N_ASYNC_USE_TASK_MANAGER
          if (tm != nullptr)
          {
            tm->get_task(group_id, [args = std::move(args), func = std::move(func)]() mutable
            {
              func(std::forward<Args>(std::get<Indices>(*args))...);
            });
          }
          else
#endif
          {
            func(std::forward<Args>(std::get<Indices>(*args))...);
          }
        }(std::forward<Func>(func), std::make_index_sequence<sizeof...(Args)>{});
      }

    private:
      explicit chain(state& _link) : link(&_link), lock(_link.lock) {}

      completed_args_t completed_args; // Just in case of completion before completed is ever set
      state* link = nullptr;

      shared_lock_t lock;

      bool has_completion_set = false;
  };
}
