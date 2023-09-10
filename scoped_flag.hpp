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
        value = ref;
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

      Type get_value() const { return value; }

    private:
      Type& ref;
      Type value;
  };

  /// \brief Atomic variant of scoped_counte, so the add/sub operation is atomic
  template<typename Type>
  class scoped_counter<std::atomic<Type>>
  {
    public:
      scoped_counter(std::atomic<Type>& _ref, Type _step = 1)
       : ref(_ref), step(_step)
      {
        value = ref.fetch_add(step, std::memory_order_acq_rel);
      }
      scoped_counter(std::atomic<Type>& _ref, scoped_counter_adopt_t)
       : ref(_ref)
      {
      }

      ~scoped_counter()
      {
        ref.fetch_sub(step, std::memory_order_release);
      }

      Type get_value() const { return value; }
    private:
      std::atomic<Type>& ref;
      Type step;
      Type value;
  };

  class scoped_ordered_list
  {
    public:
      scoped_ordered_list(std::atomic<uint64_t>& _state, uint8_t _index)
       : state(_state), index(_index)
      {
        if (index < 64)
          state.fetch_or(uint64_t(1) << index, std::memory_order_release);
      }

      ~scoped_ordered_list()
      {
        if (index < 64)
          state.fetch_xor(uint64_t(1) << index, std::memory_order_release);
      }

      uint32_t count_entries_before() const
      {
        if (index == 0 || index >= 64) return 0;

        const uint64_t current_state = state.load(std::memory_order_acquire);
        return __builtin_popcountl(current_state & ((uint64_t(1) << index) - 1));
      }

    private:
      std::atomic<uint64_t>& state;
      uint8_t index;
  };

  // deduction guides for various types:
  template<typename Type> scoped_flag(std::atomic<Type>&, Type, scoped_flag_adopt_t) -> scoped_flag<std::atomic<Type>, Type>;
  template<typename Type, typename StepType> scoped_counter(std::atomic<Type>&, StepType) -> scoped_counter<std::atomic<Type>>;
  template<typename Type> scoped_counter(std::atomic<Type>&, scoped_counter_adopt_t) -> scoped_counter<std::atomic<Type>>;
}

