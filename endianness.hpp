//
// file : endianness.hpp
// in : file:///home/tim/projects/persistence/persistence/tools/endianness.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: 07/01/2016 13:45:49
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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

#ifndef __N_212844892490344838_178107287__ENDIANNESS_HPP__
# define __N_212844892490344838_178107287__ENDIANNESS_HPP__

#include <cstdint>

namespace neam
{
  namespace ct
  {
    namespace internal
    {
      template<typename DestT, typename OrigT>
      DestT raw_cast(OrigT t)
      {
        char *dptr = reinterpret_cast<char *>(&t);
        DestT *ret = reinterpret_cast<DestT *>(dptr);
        return *ret;
      }
    } // namespace internal

    /// \brief Test if the target architecture is little endian
    constexpr bool is_little_endian()
    {
      // At least it works on GCC and Clang.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      return 1;
#elif defined(__BYTE_ORDER__)
      return 0;
#else
#warning defaulting to little endian
      return 1; // WARNING
#endif
    }
    /// \brief Test if the target architecture is big endian
    constexpr bool is_big_endian()
    {
      return !is_little_endian();
    }

    // 8 bit thing: nothing to do
    constexpr inline uint8_t htole(uint8_t v) { return v; };
    constexpr inline int8_t htole(int8_t v) { return v; };
    constexpr inline uint8_t htobe(uint8_t v) { return v; };
    constexpr inline int8_t htobe(int8_t v) { return v; };
    constexpr inline uint8_t letoh(uint8_t v) { return v; };
    constexpr inline int8_t letoh(int8_t v) { return v; };
    constexpr inline uint8_t betoh(uint8_t v) { return v; };
    constexpr inline int8_t betoh(int8_t v) { return v; };


#ifndef __has_builtin         // Optional of course
  #define __has_builtin(x) 0  // Compatibility with non-clang compilers
#endif
    constexpr inline uint16_t swap_bytes(uint16_t v) { return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8); };
    inline uint32_t swap_bytes(uint32_t v)
    {
#if (defined(__GNUC__) || (defined(__clang__) && __has_builtin(__builtin_bswap32)))
      return __builtin_bswap32(v);
#else
      uint8_t *p = reinterpret_cast<uint8_t *>(&v);
      return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | static_cast<uint32_t>(p[3]);
#endif
    };
    inline uint64_t swap_bytes(uint64_t v)
    {
#if (defined(__GNUC__) || (defined(__clang__) && __has_builtin(__builtin_bswap64)))
      return __builtin_bswap64(v);
#else
      const uint8_t *p = reinterpret_cast<uint8_t *>(&v);
      return static_cast<uint64_t>(p[0]) << 56 | static_cast<uint64_t>(p[1]) << 48 | static_cast<uint64_t>(p[2]) << 40 | static_cast<uint64_t>(p[3]) << 32
             | static_cast<uint64_t>(p[4]) << 24 | static_cast<uint64_t>(p[5]) << 16 | static_cast<uint64_t>(p[6]) << 8 | static_cast<uint64_t>(p[7]);
#endif
    };

    inline uint16_t htole(uint16_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint16_t htobe(uint16_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline uint32_t htole(uint32_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint32_t htobe(uint32_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline uint64_t htole(uint64_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint64_t htobe(uint64_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline unsigned long htole(unsigned long v) { return htole(uint64_t(v)); }
    inline unsigned long htobe(unsigned long v) { return htobe(uint64_t(v)); }

    inline uint16_t letoh(uint16_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint16_t betoh(uint16_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline uint32_t letoh(uint32_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint32_t betoh(uint32_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline uint64_t letoh(uint64_t v) { return is_little_endian() ? v : swap_bytes(v); }
    inline uint64_t betoh(uint64_t v) { return is_big_endian() ? v : swap_bytes(v); }
    inline unsigned long letoh(unsigned long v) { return letoh(uint64_t(v)); }
    inline unsigned long betoh(unsigned long v) { return betoh(uint64_t(v)); }

    inline int16_t htole(int16_t v) { return is_little_endian() ? v : internal::raw_cast<int16_t>(htole(internal::raw_cast<uint16_t>(v))); }
    inline int16_t htobe(int16_t v) { return is_little_endian() ? v : internal::raw_cast<int16_t>(htobe(internal::raw_cast<uint16_t>(v))); }
    inline int32_t htole(int32_t v) { return is_little_endian() ? v : internal::raw_cast<int32_t>(htole(internal::raw_cast<uint32_t>(v))); }
    inline int32_t htobe(int32_t v) { return is_little_endian() ? v : internal::raw_cast<int32_t>(htobe(internal::raw_cast<uint32_t>(v))); }
    inline int64_t htole(int64_t v) { return is_little_endian() ? v : internal::raw_cast<int64_t>(htole(internal::raw_cast<uint64_t>(v))); }
    inline int64_t htobe(int64_t v) { return is_little_endian() ? v : internal::raw_cast<int64_t>(htobe(internal::raw_cast<uint64_t>(v))); }
    inline long htole(long v) { return htole(int64_t(v)); }
    inline long htobe(long v) { return htobe(int64_t(v)); }

    inline int16_t letoh(int16_t v) { return is_little_endian() ? v : internal::raw_cast<int16_t>(letoh(internal::raw_cast<uint16_t>(v))); }
    inline int16_t betoh(int16_t v) { return is_little_endian() ? v : internal::raw_cast<int16_t>(betoh(internal::raw_cast<uint16_t>(v))); }
    inline int32_t letoh(int32_t v) { return is_little_endian() ? v : internal::raw_cast<int32_t>(letoh(internal::raw_cast<uint32_t>(v))); }
    inline int32_t betoh(int32_t v) { return is_little_endian() ? v : internal::raw_cast<int32_t>(betoh(internal::raw_cast<uint32_t>(v))); }
    inline int64_t letoh(int64_t v) { return is_little_endian() ? v : internal::raw_cast<int64_t>(letoh(internal::raw_cast<uint64_t>(v))); }
    inline int64_t betoh(int64_t v) { return is_little_endian() ? v : internal::raw_cast<int64_t>(betoh(internal::raw_cast<uint64_t>(v))); }
    inline long letoh(long v) { return letoh(int64_t(v)); }
    inline long betoh(long v) { return betoh(int64_t(v)); }

    // WARNING: This is not garanteed to work !
    inline float htole(float v) { return is_little_endian() ? v : internal::raw_cast<float>(htole(internal::raw_cast<uint32_t>(v))); }
    inline float htobe(float v) { return is_little_endian() ? v : internal::raw_cast<float>(htobe(internal::raw_cast<uint32_t>(v))); }
    inline double htole(double v) { return is_little_endian() ? v : internal::raw_cast<double>(htole(internal::raw_cast<uint64_t>(v))); }
    inline double htobe(double v) { return is_little_endian() ? v : internal::raw_cast<double>(htobe(internal::raw_cast<uint64_t>(v))); }

    inline float letoh(float v) { return is_little_endian() ? v : internal::raw_cast<float>(letoh(internal::raw_cast<uint32_t>(v))); }
    inline float betoh(float v) { return is_little_endian() ? v : internal::raw_cast<float>(betoh(internal::raw_cast<uint32_t>(v))); }
    inline double letoh(double v) { return is_little_endian() ? v : internal::raw_cast<double>(letoh(internal::raw_cast<uint64_t>(v))); }
    inline double betoh(double v) { return is_little_endian() ? v : internal::raw_cast<double>(betoh(internal::raw_cast<uint64_t>(v))); }
  } // namespace ct
} // namespace neam

#endif /*__N_212844892490344838_178107287__ENDIANNESS_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

