//
// created by : Timothée Feuillet
// date: 2022-1-16
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

#include <cstddef>
#include <cstdint>
#include "spinlock.hpp"

namespace neam::cr
{
  /// \brief thread safe (w/ lock) ring buffer
  /// \note very very simple
  template<typename Type, size_t Size>
  class ring_buffer
  {
    private:
      // TODO: proper type management
      static_assert(std::is_trivially_constructible_v<Type>, "Type must be trivially constructible");
      static_assert(std::is_trivially_destructible_v<Type>, "Type must be trivially destructible");
      static_assert(std::is_trivially_copy_assignable_v<Type>, "Type must be trivially copiable");

      static_assert(Size && ((Size & (Size - 1)) == 0), "Size must be a power of two");
      static_assert(Size > 2, "Size must be greater than 2");

    public:
      bool push_back(Type t)
      {
        std::lock_guard<spinlock> _lg(lock);
        uint32_t rh = get_head(read_head);
        const uint32_t wh = get_head(write_head);

        // just to normalize the check below
        if (rh < wh)
          rh += Size;

        // no data: if we have one space left, we cannot store data
        // as rh == wh is the empty buffer
        if (rh - wh == 1 || entry_count == (Size - 2))
        {
          return false;
        }

        ++write_head;
        ++entry_count;

        array[wh] = t;
        return true;
      }

      Type pop_front(bool& has_data)
      {
        std::lock_guard<spinlock> _lg(lock);
        return unlocked_pop_front(has_data);
      }

      Type quick_pop_front(bool& has_data)
      {
        if (!lock.try_lock())
        {
          has_data = false;
          return{};
        }
        std::lock_guard<spinlock> _lg(lock, std::adopt_lock);

        return unlocked_pop_front(has_data);
      }

      uint32_t size() const
      {
        return entry_count;
      }

      void clear()
      {
        std::lock_guard<spinlock> _lg(lock);
        read_head = 0;
        write_head = 0;
        entry_count = 0;
      }

    private:
      static uint32_t get_head(uint32_t h)
      {
        return (h % Size);
      }

      Type unlocked_pop_front(bool& has_data)
      {
        const uint32_t rh = get_head(read_head);
        const uint32_t wh = get_head(write_head);

        // no data
        if (rh == wh || entry_count == 0)
        {
          has_data = false;
          return {};
        }

        // has data
        --entry_count;
        ++read_head;
        has_data = true;
//         return array[rh];
        Type temp = array[rh];
        array[rh] = {};
        return temp;
      }

    private:
      uint32_t entry_count = 0;
      uint32_t read_head = 0;
      uint32_t write_head = 0;

      Type array[Size + 1];
      mutable spinlock lock;
  };
}

