// cr: a c++ template parser
// Copyright (c) 2012-2016 Timothée Feuillet
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
