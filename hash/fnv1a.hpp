//
// file : fnv1a.hpp
// in : file:///home/tim/projects/ntools/hash/fnv1a.hpp
//
// created by : Timothée Feuillet
// date: Mon May 22 2017 16:30:57 GMT-0400 (EDT)
//
//
// Copyright (c) 2017 Timothée Feuillet
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

#include <cstdint>
#include <cstddef>

#include "../ct_list.hpp"
#include "../embed.hpp"

namespace neam
{
  namespace ct
  {
    namespace hash
    {
      namespace internal
      {
        using fnv_return_types = type_list<uint32_t, uint64_t>;

        template<typename T> inline constexpr T fnv_offset_basis = T();
        template<> inline constexpr uint64_t fnv_offset_basis<uint64_t> = 0xcbf29ce484222325ul;
        template<> inline constexpr uint32_t fnv_offset_basis<uint32_t> = 0x811c9dc5u;

        template<typename T> inline constexpr T fnv_prime = T();
        template<> inline constexpr uint64_t fnv_prime<uint64_t> = 0x100000001b3ul;
        template<> inline constexpr uint32_t fnv_prime<uint32_t> = 0x01000193u;
      } // namespace internal

      template<size_t BitCount, typename T>
      static constexpr auto fnv1a(const T *const data, size_t len) -> auto
      {
        static_assert(BitCount == 32 || BitCount == 64, "We only support 32 and 64bit FNV-1a hash function");

        constexpr size_t index = BitCount / 32 - 1;
        using type = list::get_type<internal::fnv_return_types, index>;

        static_assert(sizeof(T) == 1, "Input type must be 8bit");

        static_assert(internal::fnv_offset_basis<type> != 0, "Invalid offset basis (are you using a supported type ?)");
        static_assert(internal::fnv_prime<type> != 0, "Invalid prime (are you using a supported type ?)");

        type hash = internal::fnv_offset_basis<type>;
        for (size_t i = 0; i < len; ++i)
          hash = ((uint8_t)(data)[i] ^ hash) * internal::fnv_prime<type>;
        return hash;
      }

      template<size_t BitCount, size_t StrLen>
      static constexpr auto fnv1a(const char (&str)[StrLen]) -> auto
      {
        static_assert(BitCount == 32 || BitCount == 64, "We only support 32 and 64bit FNV-1a hash function");

        constexpr size_t index = BitCount / 32 - 1;
        using type = list::get_type<internal::fnv_return_types, index>;

        static_assert(internal::fnv_offset_basis<type> != 0, "Invalid offset basis (are you using a supported type ?)");
        static_assert(internal::fnv_prime<type> != 0, "Invalid prime (are you using a supported type ?)");


        type hash = internal::fnv_offset_basis<type>;
        // StrLen - 1 is to skip the ending \0
        for (size_t i = 0; i < StrLen - 1; ++i)
          hash = ((uint8_t)(str[i]) ^ hash) * internal::fnv_prime<type>;
        return hash;
      }

      template<size_t BitCount, typename T>
      static constexpr auto fnv1a_continue(list::get_type<internal::fnv_return_types, BitCount / 32 - 1> initial,
                                           const T* const data, size_t len) -> auto
      {
        static_assert(BitCount == 32 || BitCount == 64, "We only support 32 and 64bit FNV-1a hash function");

        using type = list::get_type<internal::fnv_return_types, BitCount / 32 - 1>;

        static_assert(sizeof(T) == 1, "Input type must be 8bit");

        static_assert(internal::fnv_prime<type> != 0, "Invalid prime (are you using a supported type ?)");

        type hash = initial;

        for (size_t i = 0; i < len; ++i)
          hash = ((uint8_t)(data[i]) ^ hash) * internal::fnv_prime<type>;
        return hash;
      }

      template<size_t BitCount, size_t StrLen>
      static constexpr auto fnv1a_continue(list::get_type<internal::fnv_return_types, BitCount / 32 - 1> initial,
                                           const char (&str)[StrLen]) -> auto
      {
        static_assert(BitCount == 32 || BitCount == 64, "We only support 32 and 64bit FNV-1a hash function");

        using type = list::get_type<internal::fnv_return_types, BitCount / 32 - 1>;

        static_assert(internal::fnv_prime<type> != 0, "Invalid prime (are you using a supported type ?)");

        type hash = initial;
        // StrLen - 1 is to skip the ending \0
        for (size_t i = 0; i < StrLen - 1; ++i)
          hash = ((uint8_t)(str[i]) ^ hash) * internal::fnv_prime<type>;
        return hash;
      }
    } // namespace hash
  } // namespace ct
} // namespace neam


