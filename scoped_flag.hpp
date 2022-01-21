//
// created by : Timothée Feuillet
// date: 2021-11-27
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

#include <atomic>

namespace neam::cr
{
  static constexpr struct scoped_flag_adopt_t {} scoped_flag_adopt;
  static constexpr struct scoped_counter_adopt_t {} scoped_counter_adopt;

  /// \brief scoped flag, set and restore the value of a flag/variable
  template<typename Type, typename ValueType = Type>
  class scoped_flag
  {
    public:
      scoped_flag(Type& _ref, ValueType _set)
       : ref(_ref), unset(_ref)
      {
        ref = _set;
      }
      scoped_flag(Type& _ref, ValueType _unset, scoped_flag_adopt_t)
       : ref(_ref), unset(_unset)
      {
      }
      ~scoped_flag()
      {
        ref = unset;
      }

    private:
      Type& ref;
      ValueType unset;
  };

  /// \brief Atomic variant of scoped_flag, so the set/unset operation is atomic
  template<typename Type>
  class scoped_flag<std::atomic<Type>, Type>
  {
    public:
      scoped_flag(std::atomic<Type>& _ref, Type _set)
       : ref(_ref)
      {
        unset = ref.exchange(_set);
      }
      scoped_flag(std::atomic<Type>& _ref, Type _unset, scoped_flag_adopt_t)
       : ref(_ref), unset(_unset)
      {
      }

      ~scoped_flag()
      {
        ref = unset;
      }

    private:
      std::atomic<Type>& ref;
      Type unset;
  };

    /// \brief scoped counter, increment/decrement the value of a flag/variable
  template<typename Type>
  class scoped_counter
  {
    public:
      scoped_counter(Type& _ref)
       : ref(_ref)
      {
        ref += 1;
      }
      scoped_counter(Type& _ref, scoped_counter_adopt_t)
       : ref(_ref)
      {
      }
      ~scoped_counter()
      {
        ref -= 1;
      }

    private:
      Type& ref;
  };

  /// \brief Atomic variant of scoped_counte, so the add/sub operation is atomic
  template<typename Type>
  class scoped_counter<std::atomic<Type>>
  {
    public:
      scoped_counter(std::atomic<Type>& _ref)
       : ref(_ref)
      {
        ref.fetch_add(1, std::memory_order_release);
      }
      scoped_counter(std::atomic<Type>& _ref, scoped_flag_adopt_t)
       : ref(_ref)
      {
      }

      ~scoped_counter()
      {
        ref.fetch_sub(1, std::memory_order_release);
      }

    private:
      std::atomic<Type>& ref;
  };

  // deduction guides for various types:
  template<typename Type> scoped_flag(std::atomic<Type>&, Type, Type) -> scoped_flag<std::atomic<Type>, Type>;
}

