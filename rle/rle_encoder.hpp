//
// created by : Timothée Feuillet
// date: 2021-12-2
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

#include "../raw_data.hpp"
#include "../memory_allocator.hpp" // for a basic / growable allocator

namespace neam::rle
{
  class encoder
  {
    public:
      encoder(cr::memory_allocator& _ma) : ma(&_ma) {}
      encoder(const encoder& o) = default;
      encoder& operator = (const encoder& o) = default;

      bool is_valid() const { return ma != nullptr && ma->has_failed(); }

      void* allocate(size_t count) { return ma->allocate(count); }

      template<typename SizeType = uint32_t>
      void encode(SizeType value)
      {
        static_assert(std::is_integral_v<SizeType>, "SizeType must be an integer type");
        *(SizeType*)(ma->allocate(sizeof(SizeType))) = value;
      }

      template<typename SizeType = uint32_t>
      void* encode_and_alocate(SizeType value)
      {
        static_assert(std::is_integral_v<SizeType>, "SizeType must be an integer type");
        *(SizeType*)(ma->allocate(sizeof(SizeType))) = value;
        return ma->allocate(value);
      }

      raw_data to_raw_data()
      {
        const size_t size = ma->size();
        return { raw_data::unique_ptr { ma->give_up_data() }, size};
      }

    private:
      cr::memory_allocator* ma = nullptr;
  };
}
