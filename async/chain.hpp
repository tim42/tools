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
#include <memory>
#include <optional>
#include <tuple>

#include "../tracy.hpp"
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
  /// \brief When called inside of a then(), provide information about the cancellation of the current chain
  /// \note Main usage is to avoid doing extensive computation when the results will be ignored.
  bool is_current_chain_canceled();

  namespace internal
  {
    struct state_t
    {
      bool has_active_chain_call = false;
      bool canceled = false;
    };
    state_t& get_thread_state();

    // place outside chain, so type doesn't depend on the template params and can be referenced by all chains/states
    struct shared_cancellation_state_t : std::enable_shared_from_this<shared_cancellation_state_t>
    {
      spinlock lock;
      fu2::unique_function<void()> on_cancel_cb;

      // for bubbling cancel and stuff.
      std::weak_ptr<shared_cancellation_state_t> prev_state;

#if N_ASYNC_USE_TASK_MANAGER
      threading::task_manager* tm = nullptr;
      threading::group_t group_id = threading::k_invalid_task_group;
#endif

      bool canceled : 1 = false;
      bool is_completed : 1 = false;
      bool can_complete : 1 = true;
      bool multi_completable : 1 = false;
      bool has_completion_set : 1 = false;
    };
  }

  /// \brief Represent a list of chainable actions for asynchronous execution. A bit like a promise, but without the name.
  /// \note It has strict ownership rules and a two component part.
  ///       The chain (which is to be returned) and the state (which is to be stored and allows to trigger/complete the action)
  ///
  /// \note While cancellation is fully supported (and will "bubble up"), it's up to the code handling the state to properly support it.
  ///       Depending on the behavior of code handling the state part of the chain, it is possible to receive a completion after a call to cancel.
  ///
  /// \note While multi-completion is supported, it has further restrictions:
  ///       (must be fully setup before the first completion, must not directly or indirectly manipulated the state or the (direct) chain,
  ///        cancel (at any part of the full chain) during the completion or use the indirect completion using the task manager)
  ///
  /// \warning On thread safety. The state and chain are independent and can be each manipulated by a different threads concurrently
  ///          (Thread A can complete the state while Thread B setups the chain for example. Each object its own thread).
  ///          Having two threads handling a single object is not supported and should require external synchronization.
  ///          (Thread A cancel the chain and Thread B register a callback to the chain, for example (but it's the same for the state), REQUIRES a mutex).
  ///
  /// \warning If using the task manager, references might become dangling (r-value refs will not, but l-value might).
  ///          Mind the scope.
  ///
  /// \note Any allocated memory will be freed when both the chain and the state are destructed
  ///
  /// \todo Maybe make it compatible with the C++ coroutines? If that makes the code easier?
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

    private:
      struct indirection_t;

      // Internal data for the chain/state combo.
      struct shared_state_t : internal::shared_cancellation_state_t
      {
        fu2::unique_function<void(Args...)> on_completed;
        completed_args_t completed_args; // Just in case of completion before completed is ever set

        std::weak_ptr<indirection_t> chain_indirection;

        static std::shared_ptr<shared_state_t> create() { return std::make_shared<shared_state_t>(); }
        std::shared_ptr<shared_state_t> get_ptr() { return std::static_pointer_cast<shared_state_t>(this->shared_from_this()); }
      };

      // We need an indirection on chains for use_state
      struct indirection_t : std::enable_shared_from_this<indirection_t>
      {
        spinlock lock;
        std::shared_ptr<shared_state_t> internal_state;

        static std::shared_ptr<indirection_t> create(std::shared_ptr<shared_state_t>&& internal_state)
        {
          auto ret = std::make_shared<indirection_t>();
          check::debug::n_assert(!internal_state->chain_indirection.lock(), "indirection_t::create() internal_state is already linked / has already a chain_indirection set");
          internal_state->chain_indirection = ret;
          ret->internal_state = std::move(internal_state);
          return ret;
        }
        std::shared_ptr<indirection_t> get_ptr() { return this->shared_from_this(); }
      };

    public:
      /// \brief The part of the chain that the "provider" uses (the one providing data for the completion / support for cancellation)
      class state
      {
        public:
          state() = default;
          ~state() = default;
          state(state&& o) = default;
          state& operator = (state&& o) = default;

          chain create_chain()
          {
            check::debug::n_assert(!internal_state, "state::create_chain() called when state is already linked to a chain");
            internal_state = shared_state_t::create();
            return { internal_state->get_ptr() };
          }

          void complete(Args... args)
          {
            fu2::unique_function<void(Args...)> local_on_completed;
#if N_ASYNC_USE_TASK_MANAGER
            threading::task_manager* local_tm = nullptr;
            threading::group_t local_group_id = threading::k_invalid_task_group;
#endif

            bool clear_internal_state = false;
            bool is_canceled = false;
            {
              check::debug::n_assert(!!internal_state, "state::complete() called when state has never been linked to a chain");
              std::lock_guard _lg(internal_state->lock);
              check::debug::n_assert(internal_state->is_completed == false || internal_state->multi_completable, "state::complete(): double state completion detected");

              if (!internal_state->can_complete)
              {
                reset();
                return;
              }
              internal_state->is_completed = true;
              is_canceled = internal_state->canceled;

              if (internal_state->on_completed)
              {
                if (internal_state->multi_completable)
                {
                  // For multi-completable states, we have to call them with the lock held (and cannot use the task manager).
#if N_ASYNC_USE_TASK_MANAGER
                  check::debug::n_assert(internal_state->tm == nullptr, "state::complete(): multi-completable states cannot use the task manager");
#endif
                  TRACY_SCOPED_ZONE;
                  const internal::state_t old_state = internal::get_thread_state();
                  internal::get_thread_state() =
                  {
                    .has_active_chain_call = true,
                    .canceled = is_canceled,
                  };

                  internal_state->on_completed(std::forward<Args>(args)...);
                  internal_state->on_completed = {};

                  internal::get_thread_state() = old_state;
                }
                else
                {
#if N_ASYNC_USE_TASK_MANAGER
                  local_tm = internal_state->tm;
                  local_group_id = internal_state->group_id;
#endif
                  local_on_completed = std::move(internal_state->on_completed);
                  clear_internal_state = true;
                }
              }
              else // no on_completed set, use completed_args
              {
                check::debug::n_assert(!internal_state->multi_completable, "state::complete() multicompletable chain doesn't have a completion set and has already been completed.");
                internal_state->completed_args.emplace(std::forward<Args>(args)...);
                clear_internal_state = true;
              }
            }

            // lock is not held:
            if (local_on_completed)
            {
#if N_ASYNC_USE_TASK_MANAGER
              if (local_tm != nullptr)
              {
                // small gymnastic to still forward everything and not loose stuff around.
                local_tm->get_task(local_group_id, [...args = std::forward<wrap_arg_t<Args>>(args), on_completed = std::move(local_on_completed), is_canceled]() mutable
                {
                  TRACY_SCOPED_ZONE;
                  const internal::state_t old_state = internal::get_thread_state();
                  internal::get_thread_state() =
                  {
                    .has_active_chain_call = true,
                    .canceled = is_canceled,
                  };

                  on_completed(std::forward<Args>(args)...);

                  internal::get_thread_state() = old_state;
                });
              }
              else
#endif
              {
                TRACY_SCOPED_ZONE;

                const internal::state_t old_state = internal::get_thread_state();
                internal::get_thread_state() =
                {
                  .has_active_chain_call = true,
                  .canceled = is_canceled,
                };

                // we call the function in the same context
                local_on_completed(std::forward<Args>(args)...);

                internal::get_thread_state() = old_state;
              }
            }

            // clear the internal state as it's not used anymore
            if (clear_internal_state)
              internal_state.reset();
          }

          void operator()(Args... args) { complete(std::forward<Args>(args)...); }

          explicit operator bool() const { return !!internal_state; }

          bool is_canceled() const
          {
            if (!internal_state)
              return true; // might not be 100% correct, but in those cases where chain is incorrect, prevent work from being done
            std::lock_guard _lg(internal_state->lock);
            return internal_state->canceled;
          }

          template<typename Func>
          void on_cancel(Func&& func)
          {
            if (is_canceled())
            {
              func();
              return;
            }
            std::lock_guard _lg(internal_state->lock);
            internal_state->on_cancel_cb = std::move(func);
          }

#if N_ASYNC_USE_TASK_MANAGER
          void set_default_deferred_info(threading::task_manager* _tm, threading::group_t _group_id)
          {
            if (_tm == nullptr)
              return;
            check::debug::n_assert(!!internal_state, "state::set_default_deferred_info() called when state has never been linked to a chain");
            std::lock_guard _lg(internal_state->lock);
            if (internal_state->tm == nullptr)
            {
              internal_state->tm = _tm;
              internal_state->group_id = _group_id;
            }
          }
#endif

          /// \brief Support multi-completion events
          /// \note In this mode, the state _must_ have the on_completed function setup.
          void support_multi_completion(bool support)
          {
            check::debug::n_assert(!!internal_state, "state::support_multi_completion() called when state has never been linked to a chain");
            internal_state->multi_completable = support;
          }

          void reset() { internal_state.reset(); }

        private: // internal
          state(std::shared_ptr<shared_state_t>&& internal_state_ptr) : internal_state(std::move(internal_state_ptr)) {}

        private:
          std::shared_ptr<shared_state_t> internal_state;

          template<typename... XArgs>
          friend class chain;
      };

    public: // chain stuff:
      chain() = default;
      chain(chain&& o) = default;
      chain& operator = (chain&& o) = default;

      state create_state(bool multi_completable = false)
      {
        check::debug::n_assert(!indirection, "chain::create_state() called when chain has already been linked to a state");

        auto internal_state = shared_state_t::create();
        indirection = indirection_t::create(internal_state->get_ptr());

        state ret{std::move(internal_state)};
        ret.support_multi_completion(multi_completable);
        return ret;
      }

      static chain create_and_complete(Args... args)
      {
        // avoid the create of a state:
        std::shared_ptr<shared_state_t> internal_state = shared_state_t::create();
        internal_state->completed_args.emplace(std::forward<Args>(args)...);

        chain ret { std::move(internal_state) };
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

        bool call_with_local_completed_args = false;
        completed_args_t local_completed_args;

        std::shared_ptr<shared_state_t> internal_state;
        {
          check::debug::n_assert(!!indirection, "chain::then_void() called when chain has never been linked to a state");
          std::lock_guard _indrdlg { indirection->lock };

          // we got a lock on the indirection, we can keep a ref to the internal state (as we don't do anything funky)
          internal_state = indirection->internal_state;

          std::lock_guard _lg(internal_state->lock);
          check::debug::n_assert(internal_state->has_completion_set == false, "chain::then_void(): Double call to then/then_void/use_state detected");
          internal_state->has_completion_set = true;

          if (internal_state->completed_args)
          {
            call_with_local_completed_args = true;
            local_completed_args = std::move(internal_state->completed_args);

#if N_ASYNC_USE_TASK_MANAGER
            if (tm == nullptr)
            {
              tm = internal_state->tm = tm;
              group_id = internal_state->group_id;
            }
#endif
          }
          else
          {
#if N_ASYNC_USE_TASK_MANAGER
            if (tm != nullptr)
            {
              internal_state->tm = tm;
              internal_state->group_id = group_id;
            }
#endif
            internal_state->on_completed = std::move(cb);
          }
        }

        if (call_with_local_completed_args)
        {
          TRACY_SCOPED_ZONE;
#if N_ASYNC_USE_TASK_MANAGER
          call_with_completed_args(local_completed_args, tm, group_id, cb, internal_state);
#else
          call_with_completed_args(local_completed_args, cb, internal_state);
#endif
        }
        reset();
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
        // We got a few cases to handle:
        //  - the other state is canceled. We forward the cancellation to our state and check the other cases.
        //  - our state is already completed, so we don't do anything but forward the completion
        //  - the other state has no chain_indirection. This means the chain is destroyed. We move on_completed to our state.
        //  - the other state has a chain_indirection. We swap the internal state for ours. We release our indirection.

        // what we want to do is to transform this:
        //      state -> <this>           other -> other_chain
        // into this:
        //      state              ->              other_chain

        check::debug::n_assert(!!indirection, "chain::use_state() called when chain has never been linked to a state");
        check::debug::n_assert(!!other.internal_state, "chain::use_state() called with a state that has never been linked to a chain");

        bool call_with_local_completed_args = false;
        completed_args_t local_completed_args;

        std::shared_ptr<shared_state_t> internal_state;
        {
          // we need exclusive lock, as we'll need to potentially swap stuff around
          std::lock_guard _indrdlg { indirection->lock };
          std::lock_guard _ilg { indirection->internal_state->lock };
          internal_state = indirection->internal_state;

          check::debug::n_assert(internal_state->has_completion_set == false, "chain::use_state(): Double call to then/then_void/use_state detected");
          // we do not set has_completion_set to our state, as it will


          if (internal_state->completed_args)
          {
            {
              // handle canceled state
              // but because we might still complete even if canceled, we will still continue
              std::lock_guard _oilg { other.internal_state->lock };
              if (other.internal_state->canceled)
                bubble_cancel_unlocked(internal_state);

              other.internal_state->prev_state = indirection->internal_state;
            }
            // our state is already completed, we forward the completion to the other state without relinking
            call_with_local_completed_args = true;
            local_completed_args = std::move(internal_state->completed_args);
#if N_ASYNC_USE_TASK_MANAGER
            if (tm == nullptr)
            {
              tm = internal_state->tm = tm;
              group_id = internal_state->group_id;
            }
#endif
          }
          else
          {
            // handle the case where we're not completed yet
            check::debug::n_assert(internal_state->is_completed == false, "chain::use_state(): current state is already completed");
            check::debug::n_assert(internal_state->on_completed == nullptr, "chain::use_state(): cannot call both then() and use_state()");

            // try to grab the indirection
            bool no_other_indirection = false;
            while (true)
            {
              std::lock_guard _oilg { other.internal_state->lock };
              std::shared_ptr<indirection_t> other_indirection = other.internal_state->chain_indirection.lock();
              if (!other_indirection)
              {
                no_other_indirection = true;
                break;
              }
              if (!other_indirection->lock.try_lock())
                continue;
              // swap the indirections (lock in the correct order, indirection -> internal state):
              std::lock_guard _oindrdlg { other_indirection->lock, std::adopt_lock };


              internal_state->chain_indirection = other_indirection;
              other_indirection->internal_state = internal_state;


              if (other.internal_state->canceled)
                bubble_cancel_unlocked(internal_state);

              check::debug::n_assert(other.internal_state->is_completed == false, "chain::use_state(): other state is already completed");
              other.internal_state->prev_state = indirection->internal_state;

              // just in case they were set
              internal_state->on_completed = std::move(other.internal_state->on_completed);
              internal_state->has_completion_set = other.internal_state->has_completion_set;
              break;
            }
            if (no_other_indirection)
            {
              std::lock_guard _oilg { other.internal_state->lock };

              // there's not other indirection, simply move the data to our state:
              internal_state->on_completed = std::move(other.internal_state->on_completed);
              internal_state->has_completion_set = true;
            }
          }
        }

        // remove refs to the indirection
        reset();

        if (call_with_local_completed_args)
        {
          TRACY_SCOPED_ZONE;
#if N_ASYNC_USE_TASK_MANAGER
            call_with_completed_args(local_completed_args, tm, group_id, other, internal_state);
#else
            call_with_completed_args(local_completed_args, other, internal_state);
#endif
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
        check::debug::n_assert(!!indirection, "chain::then() called when chain has never been linked to a state");

        // use_state is faster than then, as is re-links the chain and state
        if constexpr (std::is_same_v<Func, state>)
          return use_state(N_ASYNC_FWD_PARAMS std::move(cb));

        bool is_state_multi_completable;
        {
          std::lock_guard _indlg(indirection->lock);
          check::debug::n_assert(!!indirection->internal_state, "chain::then() corrupted state");
          std::lock_guard _lg(indirection->internal_state->lock);
          is_state_multi_completable = indirection->internal_state->multi_completable;
        }

        using ret_type = std::invoke_result_t<Func, Args...>;
        if constexpr (std::is_same_v<ret_type, void>)
        {
          chain<> ret;
          auto state = ret.create_state(is_state_multi_completable);
          {
            std::lock_guard _lg(state.internal_state->lock);
            std::lock_guard _indlg(indirection->lock);
            state.internal_state->prev_state = indirection->internal_state;
          }
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = std::move(state)] (Args... args) mutable
          {
            TRACY_SCOPED_ZONE;
            cb(std::forward<Args>(args)...);
            state.complete();
          });
          return ret;
        }
        else if constexpr (is_chain<ret_type>::value)
        {
          // Do all the gymnastic internally.
          // The state is kept alive as a lambda capture
          ret_type ret;
          auto state = ret.create_state(is_state_multi_completable);

          // NOTE: Cancellation is tricky there. (use_state will change the prev_state)
          {
            std::lock_guard _lg(state.internal_state->lock);
            std::lock_guard _indlg(indirection->lock);
            state.internal_state->prev_state = indirection->internal_state;
          }
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = std::move(state)] (Args... args) mutable
          {
            TRACY_SCOPED_ZONE;
            // link the states together/chains:
            // we could use then() but the draw-back of then() is recursion.
            // use_state does not perform any recursion and will instead flatten any recursive chains automatically
            cb(std::forward<Args>(args)...).use_state(state);
//             cb(std::forward<Args>(args)...).then(std::move(state));
          });
          return ret;
        }
        else // return a simple chain where the only argument is a r-value of the return type
        {
          using chain_type = std::conditional_t<std::is_trivially_copyable_v<ret_type>, ret_type, ret_type&&>;
          chain<chain_type> ret;
          auto state = ret.create_state(is_state_multi_completable);
          {
            std::lock_guard _lg(state.internal_state->lock);
            std::lock_guard _indlg(indirection->lock);
            state.internal_state->prev_state = indirection->internal_state;
          }
          then_void(N_ASYNC_FWD_PARAMS [cb = std::move(cb), state = std::move(state)] (Args... args) mutable
          {
            TRACY_SCOPED_ZONE;
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

      /// \brief cancel the current chain.
      ///
      /// How cancellation is handled depend on how the state is handled.
      /// The code handling the state might not be able to cancel the operation, or decide to still (or not) call the completion callback.
      void cancel()
      {
        if (!indirection) return;

        {
          std::lock_guard _indrdlg { indirection->lock };
          bubble_cancel(indirection->internal_state);
        }
        reset();
      }

      explicit operator bool() const
      {
        return !!indirection;
      }

      void reset() { indirection.reset(); }

    private: // internal chain/state related stuff:
      chain(std::shared_ptr<shared_state_t>&& internal_state_ptr)
      {
        indirection = indirection_t::create(std::move(internal_state_ptr));
      }

    private:
      // shared_state_t& get_internal_state()
      // {
      //   check::debug::n_assert(!!indirection, "chain::get_internal_state(): called when chain has never been linked to a state");
      //   check::debug::n_assert(!!indirection->internal_state, "chain::get_internal_state(): corrupted state?");
      //
      //   return *indirection->internal_state;
      // }

      static void bubble_cancel(std::shared_ptr<internal::shared_cancellation_state_t> internal_state)
      {
        if (!internal_state)
          return;
        std::shared_ptr<internal::shared_cancellation_state_t> next_internal_state;
        {
          std::lock_guard _lg(internal_state->lock);
          bool was_canceled = internal_state->canceled;
          internal_state->canceled = true; // we might elect to not call the CB, but we should still track the canceled status
          if (was_canceled || (internal_state->is_completed && !internal_state->multi_completable))
            return;
          if (internal_state->on_cancel_cb)
          {
            auto cb = std::move(internal_state->on_cancel_cb);
            cb();
          }
          next_internal_state = internal_state->prev_state.lock();
        }
        // should be tail recursion:
        return bubble_cancel(std::move(next_internal_state));
      }
      static void bubble_cancel_unlocked(std::shared_ptr<internal::shared_cancellation_state_t> internal_state)
      {
        if (!internal_state)
          return;
        std::shared_ptr<internal::shared_cancellation_state_t> next_internal_state;
        {
          bool was_canceled = internal_state->canceled;
          internal_state->canceled = true; // we might elect to not call the CB, but we should still track the canceled status
          if (was_canceled || (internal_state->is_completed && !internal_state->multi_completable))
            return;
          if (internal_state->on_cancel_cb)
          {
            TRACY_SCOPED_ZONE;
            auto cb = std::move(internal_state->on_cancel_cb);
            cb();
          }
          next_internal_state = internal_state->prev_state.lock();
        }
        // should be tail recursion:
        return bubble_cancel(std::move(next_internal_state));
      }

      template<typename Func>
      static void call_with_completed_args(
        completed_args_t& args,
#if N_ASYNC_USE_TASK_MANAGER
        threading::task_manager* tm, threading::group_t group_id,
#endif
        Func&& func,
        std::shared_ptr<internal::shared_cancellation_state_t> internal_state)
      {
        [=, args = std::move(args)]<size_t... Indices>(Func&& func, std::index_sequence<Indices...>) mutable
        {
#if N_ASYNC_USE_TASK_MANAGER
          if (tm != nullptr)
          {
            tm->get_task(group_id, [args = std::move(args), func = std::move(func), internal_state = std::move(internal_state)]() mutable
            {
              const internal::state_t old_state = internal::get_thread_state();
              internal::get_thread_state() =
              {
                .has_active_chain_call = true,
                .canceled = internal_state->canceled,
              };

              func(std::forward<Args>(std::get<Indices>(*args))...);

              internal::get_thread_state() = old_state;
            });
          }
          else
#endif
          {
            const internal::state_t old_state = internal::get_thread_state();
            internal::get_thread_state() =
            {
              .has_active_chain_call = true,
              .canceled = internal_state->canceled,
            };

            func(std::forward<Args>(std::get<Indices>(*args))...);

            internal::get_thread_state() = old_state;
          }
        }(std::forward<Func>(func), std::make_index_sequence<sizeof...(Args)>{});
      }

    private:
      std::shared_ptr<indirection_t> indirection;
  };
}
