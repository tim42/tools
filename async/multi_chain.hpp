//
// created by : Timothée Feuillet
// date: 2022-2-18
//
//
// Copyright (c) 2022 Timothée Feuillet
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

#include "chain.hpp"
#include "../mt_check/mt_check_base.hpp"

namespace neam::async
{
  /// \brief Simple chain variant that is simply "the previous operation has completed"
  using continuation_chain = chain<>;

  /// \brief return a chain that will be called when all the argument chains have completed.
  ///        Argument chains must be continuation_chain that have no callback
  /// \note multi-chain needs dynamic allocation for the state
  /// \note the returned can be cancelled and will forward the cancellation to the parent chains
  template<typename Container>
  continuation_chain multi_chain(Container&& c)
  {
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename continuation_chain::state state;
      std::remove_reference_t<Container> container;

      cr::mt_checker<state_t> checker;
    };

    if (!c.size())
      return continuation_chain::create_and_complete();

    continuation_chain ret;
    std::shared_ptr<state_t> state = std::make_shared<state_t>(c.size(), ret.create_state(), std::move(c));

    // create the lambda:
    const auto lbd = [state]()
    {
      const uint64_t count = state->count.fetch_sub(1, std::memory_order_acq_rel);
      if (count == 1)
      {
        {
          // std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
          state->state.complete();
        }
      }
    };

    state->state.on_cancel([state]()
    {
      {
        // std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
        const uint32_t size = state->container.size();
        for (uint32_t i = 0; i < size; ++i)
        {
          state->container[i].cancel();
        }
      }
    });

    // register in the chains:
    const uint32_t size = state->container.size();
    for (uint32_t i = 0; i < size; ++i)
    {
      state->container[i].then(lbd);
    }

    return ret;
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  ///        Argument chains must be continuation_chain that have no callback
  /// \note multi-chain needs dynamic allocation for the state
  /// \note the returned can be cancelled and will forward the cancellation to the parent chains
  template<template<typename X> class ResultContainer, template<typename X> class Container, typename T>
  chain<ResultContainer<T>> multi_chain(Container<chain<T>>&& c)
  {
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename chain<ResultContainer<T>>::state state;
      std::remove_reference_t<Container<chain<T>>> container;

      spinlock results_lock;
      ResultContainer<std::optional<T>> results;
      cr::mt_checker<state_t> checker;
    };

    if (!c.size())
      return chain<ResultContainer<T>>::create_and_complete({});

    chain<ResultContainer<T>> ret;
    std::shared_ptr<state_t> state = std::make_shared<state_t>(c.size(), ret.create_state(), std::move(c));
    state->results.resize(state->container.size());

    // create the lambda:
    const auto lbd = [state](uint32_t idx, T&& x)
    {
      {
        std::lock_guard lg{state->results_lock};
        state->results[idx].emplace(std::move(x));
      }
      const uint64_t count = state->count.fetch_sub(1, std::memory_order_acq_rel);
      if (count == 1)
      {
        {
          // std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
          ResultContainer<T> results;
          for (auto& it : state->results)
            results.emplace_back(std::move(*it));
          state->state.complete(std::move(results));
        }
      }
    };

    state->state.on_cancel([state]()
    {
      {
        // std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
        const uint32_t size = state->container.size();
        for (uint32_t i = 0; i < size; ++i)
        {
          state->container[i].cancel();
        }
      }
    });

    // register in the chains:
    const uint32_t size = state->container.size();
    for (uint32_t i = 0; i < size; ++i)
    {
      state->container[i].then([i, lbd](T&&x) { lbd(i, std::move(x)); });
    }

    return ret;
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  ///        Argument chains must be continuation_chain that have no callback
  /// \note multi-chain needs dynamic allocation for the state
  /// \note the returned can be cancelled and will forward the cancellation to the parent chains
  template<typename... Chains>
  continuation_chain multi_chain_simple(Chains&&... chains)
  {
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename continuation_chain::state state;
      std::tuple<std::remove_reference_t<Chains>...> chains;
      cr::mt_checker<state_t> checker;
    };

    if constexpr (sizeof...(Chains) == 0)
      return continuation_chain::create_and_complete();
    if constexpr (sizeof...(Chains) == 1)
      return (chains,...);

    continuation_chain ret;
    std::shared_ptr<state_t> state = std::make_shared<state_t>(sizeof...(Chains), ret.create_state(), std::tuple<std::remove_reference_t<Chains>...> { std::move(chains)... });

    // create the lambda:
    const auto lbd = [state]()
    {
      const uint64_t count = state->count.fetch_sub(1, std::memory_order_acq_rel);
      if (count == 1)
      {
        {
          std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
          state->state.complete();
        }
      }
    };

    // allow the chains to be mass cancelled:
    state->state.on_cancel([state]()
    {
      {
        std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
        std::apply([](auto&&... chn) { (chn.cancel(), ...); }, state->chains);
      }
    });

    // register in the chains:
    // WARNING: referencing state after this is incorrect
    std::apply([&lbd](auto&& ... chn)
    {
      (chn.then(lbd), ...);
    }, state->chains);

    return ret;
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  /// \note multi-chain needs dynamic allocation for the state
  /// \note the returned can be cancelled and will forward the cancellation to the parent chains
  template<typename CbState, typename Container, typename Fnc>
  chain<CbState> multi_chain(CbState&& initial_state, Container&& c, Fnc&& fnc)
  {
    using cb_state_t = std::remove_reference_t<CbState>;
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename chain<CbState>::state state;
      cb_state_t data;
      std::remove_reference_t<Container> container;
      cr::mt_checker<state_t> checker;
    };

    if (!c.size())
      return chain<CbState>::create_and_complete(std::move(initial_state));

    chain<CbState> ret;
    std::shared_ptr<state_t> state = std::make_shared<state_t>(c.size(), ret.create_state(), std::move(initial_state), std::move(c));

    // create the lambda:
    const auto lbd = [state, fnc = std::move(fnc)]<typename... Args>(Args&&... args)
    {
      fnc(state->data, std::forward<Args>(args)...);
      const uint64_t count = state->count.fetch_sub(1, std::memory_order_acq_rel);
      if (count == 1)
      {
        {
          std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
          state->state.complete(std::move(state->data));
        }
      }
    };

    state->state.on_cancel([state]()
    {
      {
        std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
        const uint32_t size = state->container.size();
        for (uint32_t i = 0; i < size; ++i)
        {
          state->container[i].cancel();
        }
      }
    });

    // register in the chains:
    const uint32_t size = state->container.size();
    for (uint32_t i = 0; i < size; ++i)
    {
      state->container[i].then(lbd);
    }

    return ret;
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  /// \note multi-chain needs dynamic allocation for the state
  /// \note the returned can be cancelled and will forward the cancellation to the parent chains
  ///
  /// Accepts chains of different types.
  /// If the chains are different, the provided function should be declared this way:
  ///  [/*...*/](CbState& state, auto&&... args) { /*...*/ };
  template<typename CbState, typename Fnc, typename... Chains>
  chain<CbState> multi_chain(CbState&& initial_state, Fnc&& fnc, Chains&&... chains)
  {
    using cb_state_t = std::remove_reference_t<CbState>;
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename chain<CbState>::state state;
      cb_state_t data;
      std::tuple<std::remove_reference_t<Chains>...> chains;
      cr::mt_checker<state_t> checker;
    };

    if (!sizeof...(Chains))
      return chain<CbState>::create_and_complete(std::move(initial_state));

    chain<CbState> ret;
    std::shared_ptr<state_t> state = std::make_shared<state_t>(sizeof...(Chains), ret.create_state(), std::move(initial_state), std::tuple<std::remove_reference_t<Chains>...>{ std::move(chains)... });

    // create the lambda:
    const auto lbd = [state, fnc = std::move(fnc)]<typename... Args>(Args&&... args)
    {
      fnc(state->data, std::forward<Args>(args)...);
      const uint64_t count = state->count.fetch_sub(1, std::memory_order_acq_rel);
      if (count == 1)
      {
        {
          std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
          state->state.complete(std::move(state->data));
        }
      }
    };

    // allow the chains to be mass cancelled:
    state->state.on_cancel([state]()
    {
      {
        std::lock_guard _lg { neam::cr::mt_checker_write_guard_adapter::adapt(state->checker) };
        std::apply([](auto&&... chn) { (chn.cancel(), ...); }, state->chains);
      }
    });

    // register in the chains:
    // WARNING: referencing state after this is incorrect
    std::apply([&lbd](auto&& ... chn)
    {
      (chn.then(lbd), ...);
    }, state->chains);


    return ret;
  }
}

