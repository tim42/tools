// cr: a c++ template parser
// Copyright (C) 2012-2013-2014  Timothée Feuillet
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

#include <initializer_list>
#include <type_traits>
#include <utility>
#include <memory>
#include <vector>
#include <stdexcept>

#ifndef __CR_STRING_HPP__
#define __CR_STRING_HPP__

#define _SL_STRINGIZE(x) #x
#define STRINGIZE(x) _SL_STRINGIZE(x)

namespace neam
{
  // use this as 'constexpr string_t myvar = "blablabla";'
  using string_t = char [];

  namespace ct
  {
    namespace internal
    {
      // strlen function
      inline static constexpr size_t strlen(const char *const str)
      {
        return str[0] ? internal::strlen(str + 1) + 1 : 0;
      }
    } // namespace internal

    static constexpr size_t npos = static_cast<size_t>(-1);

    // strlen function
    inline static constexpr size_t strlen(const char *const str)
    {
//       return str ? internal::strlen(str) : 0; // GCC 5.2.1 HATE this on constexpr string_t...
      return internal::strlen(str);
    }

    // a safe strlen function
    inline static constexpr size_t safe_strlen(const char *const str)
    {
      return str ? internal::strlen(str) : 0; // GCC 5.2.1 HATE this on constexpr string_t...
    }
  } // namespace ct
} // namespace neam

#endif /*__CR_STRING_HPP__*/
