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

namespace neam::async
{
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
      std::atomic<uint64_t> count;
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
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  ///        Argument chains must be continuation_chain that have no callback
  /// \note multi-chain needs dynamic allocation for the state
  /// \todo put that in a pool, as the state is the same size for each multi-chain
  template<typename... Chains>
  continuation_chain multi_chain_simple(Chains&&... chains)
  {
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename continuation_chain::state state;
    };

    if constexpr (sizeof...(Chains) == 0)
      return continuation_chain::create_and_complete();
    if constexpr (sizeof...(Chains) == 1)
      return (chains,...);

    continuation_chain ret;
    state_t* state = new state_t { sizeof...(Chains), ret.create_state() };

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
    (chains.then(lbd), ...);

    return ret;
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  /// \note multi-chain needs dynamic allocation for the state
  /// \todo put that in a pool, as the state is the same size for each multi-chain
  template<typename CbState, typename Container, typename Fnc>
  chain<CbState> multi_chain(CbState&& initial_state, Container&& c, Fnc&& fnc)
  {
    using cb_state_t = std::remove_reference_t<CbState>;
    struct state_t
    {
      std::atomic<uint64_t> count;
      typename chain<CbState>::state state;
      cb_state_t data;
    };

    if (!c.size())
      return chain<CbState>::create_and_complete(std::move(initial_state));

    chain<CbState> ret;
    state_t* state = new state_t {c.size(), ret.create_state(), std::move(initial_state) };

    // create the lambda:
    const auto lbd = [state, fnc = std::move(fnc)]<typename... Args>(Args&&... args)
    {
      fnc(state->data, std::forward<Args>(args)...);
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
  }

  /// \brief return a chain that will be called when all the argument chains have completed.
  /// \note multi-chain needs dynamic allocation for the state
  /// \todo put that in a pool, as the state is the same size for each multi-chain
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
    };

    if (!sizeof...(Chains))
      return chain<CbState>::create_and_complete(std::move(initial_state));

    chain<CbState> ret;
    state_t* state = new state_t {sizeof...(Chains), ret.create_state(), std::move(initial_state) };

    // create the lambda:
    const auto lbd = [state, fnc = std::move(fnc)]<typename... Args>(Args&&... args)
    {
      fnc(state->data, std::forward<Args>(args)...);
      --state->count;
      if (state->count == 0)
      {
        state->state.complete(std::move(state->data));
        delete state;
      }
    };

    // register in the chains:
    (chains.then(lbd), ...);

    return ret;
  }
}

