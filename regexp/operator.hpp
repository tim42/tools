// cr: a c++ template parser
// Copyright (C) 2012-2016  Timothée Feuillet
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

#ifndef __NCT_OPERATOR_HPP__
#define __NCT_OPERATOR_HPP__

#include <cstddef>
#include "regexp.hpp"

namespace neam
{
  namespace ct
  {
    namespace internal
    {
      template <typename Atom, size_t Index, const char *Str>
      class atom_operator<'*', Atom, Index, Str>
      {
        private: // helpers
          // this is used to implement greedy operators
          constexpr static long ce_movetoend(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            long ret = 0;
            if (s[index] == '\0' || (ret = Atom::single_match(s, index)) == -1)
              return Atom::next_match(s, index, emt);
            return ce_movetoend(s, ret, emt);
          }

        public:
          atom_operator() = delete;

          constexpr static long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return ce_movetoend(s, index, emt);
          }

          static void print()
          {
            std::cout << '*';
          }

        public:
          constexpr static bool is_operator = true;
          constexpr static size_t len = 1;
      };

      template <typename Atom, size_t Index, const char *Str>
      class atom_operator<'+', Atom, Index, Str>
      {
        private: // helpers
          // this is used to implement greedy operators
          constexpr static long ce_movetoend(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            if (index == -1)
              return -1;
            long ret = 0;
            if (s[index] == '\0' || (ret = Atom::single_match(s, index)) == -1)
              return Atom::next_match(s, index, emt);
            return ce_movetoend(s, ret, emt);
          }

        public:
          atom_operator() = delete;

          constexpr static long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return ce_movetoend(s, Atom::single_match(s, index), emt);
          }

          static void print()
          {
            std::cout << '+';
          }

        public:
          constexpr static bool is_operator = true;
          constexpr static size_t len = 1;
      };

      template <typename Atom, size_t Index, const char *Str>
      class atom_operator<'?', Atom, Index, Str>
      {
        public:
          atom_operator() = delete;

          constexpr static long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            const long ret = Atom::end_match(s, index, emt);
            return ret != -1 ? ret : Atom::next_match(s, index, emt);
          }

          static void print()
          {
            std::cout << '?';
          }

        public:
          constexpr static bool is_operator = true;
          constexpr static size_t len = 1;
      };

      // Don't match (postfix)
      template <typename Atom, size_t Index, const char *Str>
      class atom_operator<'!', Atom, Index, Str>
      {
        public:
          atom_operator() = delete;

          constexpr static long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return Atom::end_match(s, index, emt) != -1 ? -1 : Atom::next_match(s, index, emt);
          }

          static void print()
          {
            std::cout << '!';
          }

        public:
          constexpr static bool is_operator = true;
          constexpr static size_t len = 1;
      };

      // Repeat. Syntax: {n} (exactly n time), {n,} (at least n time), {,m} (at most m time) or {n,m} (from n to m times)
      template <typename Atom, size_t Index, const char *Str>
      class atom_operator<'{', Atom, Index, Str>
      {
        public:
          atom_operator() = delete;

        private:
          static constexpr long get_size()
          {
            for (size_t i = 1; Str[i + Index] != '\0'; ++i)
            {
              if (Str[i + Index] == '}')
                return i + 1;
            }
            return -1;
          }

          static_assert(get_size() != -1, "Missing closing } in regular expression"); // there's a { but not a }
          static_assert(get_size() > 2, "Empty repeat construct in regular expression"); // there's a {} in your regexp

          static constexpr long get_comma(size_t index)
          {
            for (size_t i = 1; Str[i + index] != '\0'; ++i)
            {
              if (Str[i + index] == '}')
                return -1;
              if (Str[i + index] == ',')
                return i + index;
            }
            return -1;
          }
          static constexpr long comma_position = get_comma(Index); // absolute position, -1 if no comma
          static constexpr bool has_comma = (comma_position != -1);

          static constexpr long atoul(size_t index)
          {
            long ret = 0;
            size_t i = Str[index] == '{' ? 1 : 0;
            bool valid_input = false;

            for (; Str[i + index] && Str[i + index] == ' '; ++i); // skip spaces

            for (; Str[i + index]; ++i)
            {
              if (Str[i + index] > '9' || Str[i + index] < '0')
              {
                if (!valid_input)
                  return -1;
                return ret;
              }
              ret *= 10;
              ret += Str[i + index] - '0';
              valid_input = true;
            }
            return -1; // ??
          }

          constexpr static long _min = atoul(Index + 1);
          constexpr static long min = _min == -1 ? 0 : _min;
          constexpr static long max = has_comma ? atoul(comma_position + 1) : -1;

          static_assert(_min != max || _min != -1, "{,} is not a valid repeat pattern in a regular expression"); // there's a {,} or a {qc,fr} somewhere
          static_assert(min <= max || max == -1, "min > max in a {min,max} repeat pattern (regular expression)"); // well, min is > max

          // this is used to implement greedy operators
          constexpr static long ce_movetoend(size_t count, const char *s, long index, e_match_type emt = e_match_type::full)
          {
            if (max >= 0 && count > size_t(max))
              return -1;
            if (index == -1)
              return -1;
            if (s[index] == '\0')
              return Atom::next_match(s, index, emt);
            else
            {
              long ret = ce_movetoend(count + 1, s, Atom::single_match(s, index), emt);
              if (ret != -1)
                return ret;
              if (count < min)
                return -1;
              return Atom::next_match(s, index, emt);
            }
          }

        public:
          constexpr static long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return ce_movetoend(min ? 1 : 0, s, (min == 0 ? index : Atom::single_match(s, index)), emt);
          }

          static void print()
          {
            std::cout << '{';
            if (min)
              std::cout << min;
            if (has_comma)
              std::cout << ',';
            if (max >= 0)
              std::cout << max;
            std::cout << '}';
          }

        public:
          constexpr static bool is_operator = true;
          constexpr static size_t len = get_size();
      };
    } // namespace internal
  } // namespace ct
} // namespace neam

#endif /*__NCT_OPERATOR_HPP__*/
