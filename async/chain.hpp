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

// waiting for std::move_only_function to be availlable, here is a ~drop-in replacement
#include "internal_do_not_use_cpp23_replacement/function2.hpp"

namespace neam::async
{
  /// \brief Represent a list of chainable actions for asynchronous execution. A bit like a promise, but without the name.
  /// \note It has strict ownership rules and a two component part. The chain (which is to be returned) and the state (which is to be stored and allows to trigger the action)
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
    public:
      class state
      {
        public:
          state() = default;

          chain create_chain()
          {
            chain ret(*this);
            link = &ret;
            return ret;
          }

          template<typename... ActualArgs>
          void complete(Args... args)
          {
            if (canceled)
              return;
            if (on_completed)
            {
              on_completed(std::forward<Args>(args)...);
              on_completed = nullptr;
              return;
            }
            if (link)
            {
              link->completed_args.emplace(std::forward<Args>(args)...);
              if (link) link->link = nullptr;
              link = nullptr;
              return;
            }

            if (link) link->link = nullptr;
            link = nullptr;

            // It's not an error to not set the callback
//             check::debug::n_assert(false, "Unable to complete as the chain has be destructed without a callback ever being set");
          }
          void operator()(Args... args) { complete(std::forward<Args>(args)...); }

          ~state()
          {
            if (link)
            {
              link->link = nullptr;
              link = nullptr;
            }
          }

          state(state&& o)
           : link(o.link)
           , on_completed(std::move(o.on_completed))
           , canceled(o.canceled)
          {
            o.link = nullptr;
            if (link)
              link->link = this;
          }

          state& operator = (state&& o)
          {
            if (&o == this) return *this;
            if (link) link->link = nullptr;

            canceled = o.canceled;
            link = o.link;
            if (link) link->link = this;
            o.link = nullptr;

            on_completed = std::move(o.on_completed);
            return *this;
          }

          bool is_canceled() const { return canceled; }

        private:
          explicit state(chain& _link) : link(&_link) {}

          chain* link = nullptr;
//           std::move_only_function
          fu2::unique_function<void(Args...)> on_completed;

          bool canceled = false;

          friend class chain;
      };

      chain() = default;

      state create_state()
      {
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
      void then_void(Func&& cb)
      {
        using ret_type = typename std::result_of<Func(Args...)>::type;
        static_assert(std::is_same_v<ret_type, void>, "return type is not void");

        if (completed_args)
        {
          call_with_completed_args(cb);
          completed_args.reset();
          return;
        }
        if (link != nullptr)
        {
          link->on_completed = std::move(cb);
          return;
        }

        check::debug::n_assert(false, "Unable to call the callback as the state has been destructed without being completed");
      }

      void use_state(state&& other)
      {
        if (&other == link)
          return;

        // we are already completed, simply complete the state
        if (completed_args)
        {
          // this handles the case where the other chain is alive or the callback has been set
          call_with_completed_args(other);
          completed_args.reset();
          return;
        }
        else // we are not completed
        {
          check::debug::n_assert(link != nullptr, "Unable to chain the states as the state has been destructed without being completed");
          check::debug::n_assert(link->on_completed == nullptr, "cannot call both then() and use_state()");

          // other state/chain are stall, no need to do anything
          if (!other.on_completed && other.link == nullptr)
            return;

          // move the on_completed callback around, if it exists
          if (other.on_completed)
          {
            link->on_completed = std::move(other.on_completed);
            other.on_completed = nullptr;
          }
          // we go from chain<->state{other} -> chain{this}<->state
          //    to      chain        <->         state
          // and left chain{this} and state{other} be stale and un-completable
          link->link = other.link;
          if (link->link)
            link->link->link = link;

          // unlink ourselves (and other)
          other.link = nullptr;
          link = nullptr;
        }
      }

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
      auto then(Func&& cb) -> auto
      {
        using ret_type = typename std::result_of<Func(Args...)>::type;
        if constexpr (std::is_same_v<ret_type, void>)
        {
          return then_void(std::forward<Func>(cb));
        }
        else if constexpr (is_chain<ret_type>::value)
        {
          // Do all the gymnastic internally.
          // The state is kept alive as a lambda capture
          ret_type ret;
          then_void([cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            // link the states together/chains:
            cb(std::forward<Args>(args)...).use_state(std::move(state));
          });
          return ret;
        }
        else // return a simple chain where the only argument is a r-value of the return type
        {
          using chain_type = std::conditional_t<std::is_trivially_copyable_v<ret_type>, ret_type, ret_type&&>;
          chain<chain_type> ret;
          then_void([cb = std::move(cb), state = ret.create_state()] (Args... args) mutable
          {
            // link the states together/chains:
            state.complete(cb(std::forward<Args>(args)...));
          });
          return ret;
        }
      }

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
        if (link != nullptr)
        {
          link->link = nullptr;
          link = nullptr;
        }
      }

      chain(chain&& o)
        : completed_args(std::move(o.completed_args))
        , link(o.link)
      {
        o.link = nullptr;
        if (link)
          link->link = this;
      }

      chain& operator = (chain&&o)
      {
        if (&o == this) return *this;
        if (link) link->link = nullptr;
        link = o.link;
        if (link) link->link = this;
        o.link = nullptr;
        completed_args = std::move(o.completed_args);
        return *this;
      }

      void cancel()
      {
        if (link != nullptr)
          link->canceled = true;
      }

    private:
      template<typename Func>
      void call_with_completed_args(Func&& func)
      {
        [this]<size_t... Indices>(Func&& func, std::index_sequence<Indices...>)
        {
          func(std::forward<Args>(std::get<Indices>(*completed_args))...);
        }(std::forward<Func>(func), std::make_index_sequence<sizeof...(Args)>{});
      }

    private:
      explicit chain(state& _link) : link(&_link) {}

      std::optional<std::tuple<remove_rvalue_reference_t<Args>...>> completed_args; // Just in case of completion before completed is ever set
      state* link = nullptr;
  };

  /// \brief Simple chain variant that is simply "the previous operation has completed"
  using continuation_chain = chain<>;

  /// \brief return a chain that will be called when all the argument chains have completed.
  ///        Argument chains must be continuation_chain that have no callback
  /// \note multi-chain needs dynamic allocation for the state
  /// \todo put that in a pool, as the state is the same size for each multi-chain
  template<typename Container>
  continuation_chain multi_chain(Container&& c)
  {
    struct state_t
    {
      uint64_t count;
      typename continuation_chain::state state;
    };

    if (!c.size())
      return continuation_chain::create_and_complete();

    continuation_chain ret;
    state_t* state = new state_t {c.size(), ret.create_state() };

    // create the lambda:
    const auto lbd = [state]()
    {
      --state->count;
      if (state->count == 0)
      {
        state->state.complete();
        delete state;
      }
    };

    // register in the chains:
    for (auto& it : c)
    {
      it.then(lbd);
    }

    return ret;
  };

  /// \brief return a chain that will be called when all the argument chains have completed.
  /// \note multi-chain needs dynamic allocation for the state
  /// \todo put that in a pool, as the state is the same size for each multi-chain
  template<typename CbState, typename Container, typename Fnc>
  chain<CbState> multi_chain(CbState&& initial_state, Container&& c, Fnc&& fnc)
  {
    struct state_t
    {
      uint64_t count;
      typename chain<CbState>::state state;
      CbState data;
    };

    if (!c.size())
      return chain<CbState>::create_and_complete(std::move(initial_state));

    chain<CbState> ret;
    state_t* state = new state_t {c.size(), ret.create_state(), std::move(initial_state) };

    // create the lambda:
    const auto lbd = [state, fnc = std::move(fnc)](CbState current)
    {
      fnc(state->data, current);
      --state->count;
      if (state->count == 0)
      {
        state->state.complete(std::move(state->data));
        delete state;
      }
    };

    // register in the chains:
    for (auto& it : c)
    {
      it.then(lbd);
    }

    return ret;
  };
}
