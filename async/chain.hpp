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

      template<typename T> struct remove_rvalue_reference      {typedef T type;};
      template<typename T> struct remove_rvalue_reference<T&&> {typedef T type;};
      template<typename T> using remove_rvalue_reference_t = typename remove_rvalue_reference<T>::type;

      /// \brief Probably the only easy way to avoid all the complexity with locks
      /// This lock is shared with the state and the chain. If the state is transfered to another chain, 
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
        shared_lock_t() :state(shared_lock_state_t::create()) {}
        shared_lock_t(std::nullptr_t) :state(nullptr) {}
        ~shared_lock_t() { if (state) { state->release(); } state = nullptr; }
        shared_lock_t(const shared_lock_t& o) : state(o.state) { if (state) state->acquire(); }
        shared_lock_t(shared_lock_t&& o) : state(o.state) { o.state = nullptr; }
        shared_lock_t& operator = (const shared_lock_t& o)
        {
          if (&o == this) return *this;
          if (state) state->release();
          state = o.state;
          if (state) state->acquire();
          return *this;
        }
        shared_lock_t& operator = (shared_lock_t&& o)
        {
          if (&o == this) return *this;
          state = o.state;
          o.state = nullptr;
          return *this;
        }

        void lock() { if (state) state->lock.lock(); }
        void unlock() { if (state) state->lock.unlock(); }
        void trylock() { if (state) state->lock.trylock(); }

        // lock must be held:
        void drop() { if (state) state->release(); state = nullptr; }

        shared_lock_state_t* state = nullptr;
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
            std::lock_guard<shared_lock_t> _lg(lock);

            if (canceled)
              return;
            if (on_completed)
            {
#if N_ASYNC_USE_TASK_MANAGER
              if (tm != nullptr)
              {
                // small gymnastic to still forward everything and not loose stuff around.
                tm->get_task(group_id, [...args = std::forward<Args>(args), on_completed = std::move(on_completed)]() mutable
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
              on_completed = nullptr;
              return;
            }
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
            std::lock_guard<shared_lock_t> _lg(lock);
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

            {
              // release current state:
              std::lock_guard<shared_lock_t> _lg(lock);
              if (link)
              {
                link->link = nullptr;
              }
            }

            lock = o.lock;
            std::lock_guard<shared_lock_t> _lg(lock);
            o.lock.drop();

            canceled = o.canceled;
            link = o.link;
            if (link)
            {
              link->link = this;
            }
            o.link = nullptr;

            on_completed = std::move(o.on_completed);
#if N_ASYNC_USE_TASK_MANAGER
            tm = o.tm;
            group_id = o.group_id;
#endif

            return *this;
          }

          bool is_canceled() const { return canceled; }

#if N_ASYNC_USE_TASK_MANAGER
          void set_default_deferred_info(threading::task_manager* _tm, threading::group_t _group_id)
          {
            std::lock_guard<shared_lock_t> _lg(lock);
            if (tm == nullptr)
              set_deferred_info(_tm, _group_id);
          }
#endif

        private:
          explicit state(chain& _link) : link(&_link), lock(_link.lock) {}
#if N_ASYNC_USE_TASK_MANAGER
          void set_deferred_info(threading::task_manager* _tm, threading::group_t _group_id)
          {
            tm = _tm;
            group_id = _group_id;
          }
#endif

          chain* link = nullptr;
//           std::move_only_function
          fu2::unique_function<void(Args...)> on_completed;

          bool canceled = false;

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
        using ret_type = typename std::result_of<Func(Args...)>::type;
        static_assert(std::is_same_v<ret_type, void>, "return type is not void");

        std::lock_guard<shared_lock_t> _lg(lock);
        if (completed_args)
        {
#if N_ASYNC_USE_TASK_MANAGER
          call_with_completed_args(tm, group_id, cb);
#else
          call_with_completed_args(cb);
#endif
          completed_args.reset();
          return;
        }
        if (link != nullptr)
        {
          link->on_completed = std::move(cb);
#if N_ASYNC_USE_TASK_MANAGER
          link->set_deferred_info(tm, group_id);
#endif
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

      /// \brief Chain from a potentially not-contructed state
      /// This allows for flat (co_await style) code:
      ///   some_async_function(...)
      ///     .then([](...) { return some_other_async_function(...); })
      ///     .then([](...) { return some_other_other_async_function(...); })
      ///     ...;
      ///  \note the chain types can be different allowing for more flexibility
      ///  \note valid return types are:
      ///   - void: same as calling then_void()
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
#if N_ASYNC_USE_TASK_MANAGER
#define N_ASYNC_FWD_PARAMS tm, group_id,
#else
#define N_ASYNC_FWD_PARAMS
#endif
        using ret_type = typename std::result_of<Func(Args...)>::type;
        if constexpr (std::is_same_v<ret_type, void>)
        {
          return then_void(N_ASYNC_FWD_PARAMS std::forward<Func>(cb));
        }
        else if constexpr (is_chain<ret_type>::value)
        {
          // Do all the gymnastic internally.
          // The state is kept alive as a lambda capture
          ret_type ret;
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            // link the states together/chains:
            // FIXME: there was a use_state that would re-link states and chains here, but that was... problematic
            //        with the multi-threaded test (memory corruption)
            // This do work, but has a bit more overhead
            cb(std::forward<Args>(args)...).then(std::move(state));
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
        std::lock_guard<shared_lock_t> _lg(lock);
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
        {
          // release the state:
          std::lock_guard<shared_lock_t> _lg(lock);
          if (link)
          {
            link->link = nullptr;
          }
        }
        lock = o.lock;
        std::lock_guard<shared_lock_t> _lg(lock);
        o.lock.drop();
        link = o.link;
        if (link)
        {
          link->link = this;
          o.link = nullptr;
        }

        completed_args = std::move(o.completed_args);
        return *this;
      }

      void cancel()
      {
        std::lock_guard<shared_lock_t> _lg(lock);
        if (link != nullptr)
        {
          link->canceled = true;
        }
      }

    private:
      template<typename Func>
      void call_with_completed_args(
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        Func&& func)
      {
        [=, this]<size_t... Indices>(Func&& func, std::index_sequence<Indices...>)
        {
#if N_ASYNC_USE_TASK_MANAGER
          if (tm != nullptr)
          {
            tm->get_task(group_id, [completed_args = std::move(completed_args), func = std::move(func)]() mutable
            {
              func(std::forward<Args>(std::get<Indices>(*completed_args))...);
            });
          }
          else
#endif
          {
            func(std::forward<Args>(std::get<Indices>(*completed_args))...);
          }
        }(std::forward<Func>(func), std::make_index_sequence<sizeof...(Args)>{});
      }

    private:
      explicit chain(state& _link) : link(&_link), lock(_link.lock) {}

      std::optional<std::tuple<remove_rvalue_reference_t<Args>...>> completed_args; // Just in case of completion before completed is ever set
      state* link = nullptr;

      shared_lock_t lock;
  };
}
