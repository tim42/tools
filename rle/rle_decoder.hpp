//
// created by : Timothée Feuillet
// date: 2021-12-1
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

#include "../type_id.hpp"
#include "../raw_data.hpp"

namespace neam::rle
{
  /// \brief helper utility to manage rle decode
  /// \note there's a file with helpers for the most common types
  class decoder
  {
    public:
      decoder(const raw_data& _data)
        : data(_data)
        , offset(0)
        , size(data.size)
      {
      }

      decoder(const raw_data& _data, uint64_t _offset, uint64_t _size)
        : data(_data)
        , offset(_offset)
        , size(_size == ~0ul ? data.size : _size)
      {
      }

      decoder(const decoder&o) = default;

      template<typename Type = void*>
      const Type* get_address() const
      {
        if (offset + size > data.size) return nullptr;
        return (const Type*)((const uint8_t*)data.data.get() + offset);
      }

      uint64_t get_size() const
      {
        if (offset + size > data.size) return 0;
        return size;
      }

      bool is_valid() const
      {
        if (offset + size > data.size) return false;
        return true;
      }

      bool skip(uint64_t bytes_to_skip)
      {
        if (bytes_to_skip > size)
        {
          N_RLE_LOG_FAIL("failed to skip {} bytes: size left: {}", bytes_to_skip, size);
          offset = data.size + 1;
          size = 0;
          return false;
        }
        offset += bytes_to_skip;
        size -= bytes_to_skip;
        return true;
      }

      template<typename SizeType = uint32_t>
      std::pair<SizeType, bool> decode()
      {
        static_assert(std::is_integral_v<SizeType>, "SizeType must be an integer type");

        if (!is_valid() || size < sizeof(SizeType))
        {
          N_RLE_LOG_FAIL("failed to read a size marker of type {}: size left: {}, marker size: {}", ct::type_name<SizeType>.str, size, sizeof(SizeType));
          return {0, false}; // invalid
        }

        SizeType decoded_size = *get_address<SizeType>();
        return {decoded_size, skip(sizeof(SizeType))};
      }

      /// \brief decode and advance the internal state
      /// \return an invalid entry in case raw_data goes outside the availlable memory
      template<typename SizeType = uint32_t>
      decoder decode_and_skip()
      {
        static_assert(std::is_integral_v<SizeType>, "SizeType must be an integer type");

        const auto [decoded_size, success] = decode<SizeType>();
        if (!success)
          return {data, data.size + 1, 0}; // invalid
        decoder dc = {data, offset, decoded_size};
        skip(decoded_size);
        if (!is_valid())
          return {data, data.size + 1, 0}; // invalid
        return dc;
      }

    private:
      const raw_data& data;
      uint64_t offset;
      uint64_t size; // from start_offset
  };
}
