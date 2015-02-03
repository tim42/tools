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
  using string_t = char[];

  namespace ct
  {

    static constexpr size_t npos = static_cast<size_t>(-1);

    // strlen function
    inline static constexpr size_t strlen(const char *str)
    {
      return str[0] ? ct::strlen(str + 1) + 1 : 0;
    }

    // Compile time string class
    // support substring, comparison,
    // casting to char (Except when it's a substring, and when the end is < the length of the string)
    // length computation,
    template <const char *Str, size_t Start = 0, size_t End = npos>
    class string
    {
      private: // helpers
        // comparison helpers
        template<const char *const S1, const char *const S2, size_t Pos = Start>
        inline static constexpr typename std::enable_if < S1[Pos] != S2[Pos], bool >::type str_is_same()
        {
          return false;
        }
        template<const char *const S1, const char *const S2, size_t Pos = Start>
        inline static constexpr typename std::enable_if < S1[Pos] == S2[Pos] && (S1[Pos] == 0 || S2[Pos] == 0 || Pos == End), bool >::type str_is_same()
        {
          return true;
        }
        template<const char *const S1, const char *const S2, size_t Pos = Start>
        inline static constexpr typename std::enable_if < S1[Pos] == S2[Pos] && S1[Pos] != 0 && S2[Pos] != 0 && Pos != End, bool >::type str_is_same()
        {
          return str_is_same < S1, S2, Pos + 1 > ();
        }

      public:
        constexpr string() {}
        ~string() {}

        // allow casting back the class to a const char *
        constexpr operator const char *() const
        {
          return value + Start;
        }

        // comparison operators
        template<const char *OStr>
        constexpr bool operator == (const string<OStr> &) const
        {
          return str_is_same<Str, OStr>();
        }
        template<const char *OStr>
        constexpr bool operator != (const string<OStr> &other) const
        {
          return !(*this == other);
        }

        constexpr char operator[](size_t pos) const
        {
          return pos + Start < End ? Str[pos + Start] : 0;
        }

      private: // helpers
        // length computation
        template<size_t Pos>
        constexpr static typename std::enable_if<Str[Pos], size_t>::type _compute_length()
        {
          return _compute_length < Pos + 1 > () + 1;
        }
        template<size_t Pos>
        constexpr static typename std::enable_if < !Str[Pos], size_t >::type _compute_length()
        {
          return 0;
        }
        constexpr static size_t compute_length()
        {
          return _compute_length<0>();
        }

      public:
        static constexpr const char *value = Str + Start; // data
        static constexpr size_t length = compute_length(); // length of the string
        static constexpr size_t start_index = Start;
        static constexpr size_t end_index = End;
    };
  } // namespace ct
} // namespace neam

#endif /*__CR_STRING_HPP__*/
