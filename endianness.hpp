//
// file : endianness.hpp
// in : file:///home/tim/projects/persistence/persistence/tools/endianness.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: 07/01/2016 13:45:49
//
//
// Copyright (C) 2014 Timothée Feuillet
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
      union _chk
      {
        uint16_t d;
        uint8_t a[2];
      };

      static constexpr _chk _endianness_chk{1};

      template<typename DestT, typename OrigT>
      constexpr DestT raw_cast(OrigT t)
      {
        char *dptr = reinterpret_cast<char *>(&t);
        DestT *ret = reinterpret_cast<DestT *>(dptr);
        return *ret;
      }
    } // namespace internal

    /// \brief Test if the target architecture is little endian
    constexpr bool is_little_endian()
    {
      return internal::_endianness_chk.a[0] == 1;
    }
    /// \brief Test if the target architecture is big endian
    constexpr bool is_big_endian()
    {
      return internal::_endianness_chk.a[1] == 1;
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
    constexpr inline uint32_t swap_bytes(uint32_t v)
    {
#if (defined(__GNUC__) || (defined(__clang__) && __has_builtin(__builtin_bswap32)))
      return __builtin_bswap32(v);
#else
      uint8_t *p = reinterpret_cast<uint8_t *>(&v);
      return static_cast<uint32_t>(p)[0] << 24 | static_cast<uint32_t>(p)[1] << 16 | static_cast<uint32_t>(p)[2] << 8 | static_cast<uint32_t>(p)[3];
#endif
    };
    constexpr inline uint64_t swap_bytes(uint64_t v)
    {
#if (defined(__GNUC__) || (defined(__clang__) && __has_builtin(__builtin_bswap64)))
      return __builtin_bswap64(v);
#else
      const uint8_t *p = reinterpret_cast<uint8_t *>(&v);
      return static_cast<uint64_t>(p)[0] << 56 | static_cast<uint64_t>(p)[1] << 48 | static_cast<uint64_t>(p)[2] << 40 | static_cast<uint64_t>(p)[3] << 32
             | static_cast<uint64_t>(p)[4] << 24 | static_cast<uint64_t>(p)[5] << 16 | static_cast<uint64_t>(p)[6] << 8 | static_cast<uint64_t>(p)[7];
#endif
    };

    constexpr inline uint16_t htole(uint16_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint16_t htobe(uint16_t v) { return is_big_endian() ? v : swap_bytes(v); }
    constexpr inline uint32_t htole(uint32_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint32_t htobe(uint32_t v) { return is_big_endian() ? v : swap_bytes(v); }
    constexpr inline uint64_t htole(uint64_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint64_t htobe(uint64_t v) { return is_big_endian() ? v : swap_bytes(v); }

    constexpr inline uint16_t letoh(uint16_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint16_t betoh(uint16_t v) { return is_big_endian() ? v : swap_bytes(v); }
    constexpr inline uint32_t letoh(uint32_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint32_t betoh(uint32_t v) { return is_big_endian() ? v : swap_bytes(v); }
    constexpr inline uint64_t letoh(uint64_t v) { return is_little_endian() ? v : swap_bytes(v); }
    constexpr inline uint64_t betoh(uint64_t v) { return is_big_endian() ? v : swap_bytes(v); }

    constexpr inline int16_t htole(int16_t v) { return internal::raw_cast<int16_t>(htole(internal::raw_cast<uint16_t>(v))); }
    constexpr inline int16_t htobe(int16_t v) { return internal::raw_cast<int16_t>(htobe(internal::raw_cast<uint16_t>(v))); }
    constexpr inline int32_t htole(int32_t v) { return internal::raw_cast<int32_t>(htole(internal::raw_cast<uint32_t>(v))); }
    constexpr inline int32_t htobe(int32_t v) { return internal::raw_cast<int32_t>(htobe(internal::raw_cast<uint32_t>(v))); }
    constexpr inline int64_t htole(int64_t v) { return internal::raw_cast<int64_t>(htole(internal::raw_cast<uint64_t>(v))); }
    constexpr inline int64_t htobe(int64_t v) { return internal::raw_cast<int64_t>(htobe(internal::raw_cast<uint64_t>(v))); }

    constexpr inline int16_t letoh(int16_t v) { return internal::raw_cast<int16_t>(letoh(internal::raw_cast<uint16_t>(v))); }
    constexpr inline int16_t betoh(int16_t v) { return internal::raw_cast<int16_t>(betoh(internal::raw_cast<uint16_t>(v))); }
    constexpr inline int32_t letoh(int32_t v) { return internal::raw_cast<int32_t>(letoh(internal::raw_cast<uint32_t>(v))); }
    constexpr inline int32_t betoh(int32_t v) { return internal::raw_cast<int32_t>(betoh(internal::raw_cast<uint32_t>(v))); }
    constexpr inline int64_t letoh(int64_t v) { return internal::raw_cast<int64_t>(letoh(internal::raw_cast<uint64_t>(v))); }
    constexpr inline int64_t betoh(int64_t v) { return internal::raw_cast<int64_t>(betoh(internal::raw_cast<uint64_t>(v))); }

    // WARNING: This is not garanteed to work !
    constexpr inline float htole(float v) { return internal::raw_cast<float>(htole(internal::raw_cast<uint32_t>(v))); }
    constexpr inline float htobe(float v) { return internal::raw_cast<float>(htobe(internal::raw_cast<uint32_t>(v))); }
    constexpr inline double htole(double v) { return internal::raw_cast<double>(htole(internal::raw_cast<uint64_t>(v))); }
    constexpr inline double htobe(double v) { return internal::raw_cast<double>(htobe(internal::raw_cast<uint64_t>(v))); }

    constexpr inline float letoh(float v) { return internal::raw_cast<float>(letoh(internal::raw_cast<uint32_t>(v))); }
    constexpr inline float betoh(float v) { return internal::raw_cast<float>(betoh(internal::raw_cast<uint32_t>(v))); }
    constexpr inline double letoh(double v) { return internal::raw_cast<double>(letoh(internal::raw_cast<uint64_t>(v))); }
    constexpr inline double betoh(double v) { return internal::raw_cast<double>(betoh(internal::raw_cast<uint64_t>(v))); }
  } // namespace ct
} // namespace neam

#endif /*__N_212844892490344838_178107287__ENDIANNESS_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

