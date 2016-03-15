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

/// \file regexp.hpp
/// \brief A general purpose compile-time regexp that can matches strings at compile-time and runtime
/// A little disclaimer about this file, I wrote it back in 2012, just after learning C++ at school.
/// The file as since been sleeping on my hard disk drive, until I (re)-discover it in early 2016
/// I've cleaned it a bit, but as it works I don't have any reason to rewrite it.
///
/// It works by having the atom<> class associated to one or more character of the regexp string.
/// An atom can represent special characters (., $, ^, (, ), \, |, ...) which has then a special meaning
/// To an atom<> could be associated an operator (+, ?, *) than changes the matching behavior of the atom<>
/// If I don't clearly remember how the alternative atom works ( '|' ), the sub-expression ( '(' ) works by creating a
/// sub-expression terminated by a ')' (this way, you don't have to count opening and closing brackets and the expression
/// is terminated at the sight of a closing bracket).

#ifndef __NCT_REGEXP_HPP__
#define __NCT_REGEXP_HPP__

#include "atom.hpp"

namespace neam
{
  namespace ct
  {
    /// \brief A compile-time regular expression that can matches strings at compile-time and runtime
    /// The regular expression is compiled at compile-time, generating compilation errors if the syntax is not correct
    /// It currently handles (...), ...|..., ...*, ...?, ...+, $, \\, ., ...! (postfix not), [...] (match a range), {...} (repeat from n to m time)
    template <const char *Str>
    class regexp
    {
      public:
        regexp() = delete; // can't be constructed (this makes no sens)

        /// \brief Matches another string.
        /// This version may be optimized-out by the compiler
        /// \param index The start index
        /// \return the maximum index that the regular expression can match (that index could be strlen(MatchStr) if the string matches whole)
        ///         It returns -1 if nothing can matches
        template<const char *MatchStr>
        static constexpr long match(long index = 0)
        {
          return (list_t::head_t::is_head ? list_t::head_t::pipe_match(MatchStr, index) : list_t::match(MatchStr, index));
        }

        /// \brief Matches another string.
        /// This version may be optimized-out by the compiler
        /// \param index The start index
        /// \return the maximum index that the regular expression can match (that index could be strlen(s) if the string matches whole)
        ///         It returns -1 if nothing can matches
        static constexpr long match(const char *s, long index = 0)
        {
          return (list_t::head_t::is_head ? list_t::head_t::pipe_match(s, index) : list_t::match(s, index));
        }

        /// \brief Matches another string.
        /// \param index The start index
        /// \return true if the string matches whole, false otherwise
        static constexpr bool bmatch(const char *s, long index = 0)
        {
          const long ret = match(s, index);
          return ret == -1 ? false : (s[ret] == '\0');
        }

        /// \brief Debug print the regular expression
        /// (note: in fact it walk the generated tree and print an equivalent regular expression)
        static void print()
        {
          list_t::print(0);
          std::cout << std::endl;
        }

      public:
        using list_t = internal::atom<0, Str[0], Str, internal::no_prev>;

        // error checking (too much closing brackets).
        static_assert(!Str[list_t::end], "too many closing brackets in regular expression");
    };
  } // namespace ct
} // namespace neam

#include "operator.hpp"

#endif /*__NCT_REGEXP_HPP__*/
