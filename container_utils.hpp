//
// created by : Timothée Feuillet
// date: 2022-5-21
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

#include <tuple>

namespace neam::cr
{
  /// \brief Construct a container from a list of move-only objects
  /// usage: \code construct<std::vector<int>>(a, b, c); \endcode
  template<typename Cont, typename... Types>
  auto construct(Types &&... v)
  {
    Cont r;
    (r.push_back(std::forward<Types>(v)), ...);
    return r;
  }

  /// \brief Construct a container from a list of move-only objects
  /// usage: \code construct<std::vector>(a, b, c); \endcode
  template<template<typename...> typename Cont, typename Type, typename... Types>
  auto construct(Type&& v0, Types &&... v)
  {
    Cont<std::remove_const_t<std::remove_reference_t<Type>>> r;
    r.push_back(std::forward<Type>(v0));
    (r.push_back(std::forward<Types>(v)), ...);
    return r;
  }

  template<typename Fnc, typename... Args>
  void for_each(std::tuple<Args...>& t, Fnc&& fnc)
  {
    std::apply([&](auto&&... args)
    {
      (fnc(std::forward<Args>(args)), ...);
    }, t);
  }
  template<typename Fnc, typename... Args>
  void for_each(const std::tuple<Args...>& t, Fnc&& fnc)
  {
    std::apply([&](auto&&... args)
    {
      (fnc(args), ...);
    }, t);
  }
}
