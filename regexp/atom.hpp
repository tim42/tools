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

#ifndef __NCT_ATOM_HPP__
#define __NCT_ATOM_HPP__

#include <cstddef>
#include <cstring>
#include <iostream>
#include <type_traits>

namespace neam
{
  namespace ct
  {
    namespace internal
    {
      enum class e_match_type : unsigned int
      {
        full = 0,
        single_branch = 1,
        parent_single_branch = 2
      };

      class no_prev {};
      class no_parent {};

      class no_head
      {
        public:
          constexpr static long pipe_match (const char *, long, e_match_type = e_match_type::full)
          {
            return -1;
          }
          static constexpr bool is_head = false;
      };

      ///
      /// OPERATOR
      ///
      template <char Op, typename Atom, size_t Index, const char *Str>
      class atom_operator
      {
        public:
          atom_operator() = delete;

          static constexpr long match(const char *, long, e_match_type = e_match_type::full)
          {
            return -1;
          }

          static void print() {}

        public:
          constexpr static bool is_operator = false;
          constexpr static size_t len = 0;
      };

      ///
      /// ATOM
      ///
      template <size_t Index, char First, const char *Str, typename Prev, bool IF = true, typename Parent = no_parent>
      class atom
      {
        public:
          atom() = delete;

          static constexpr long single_match(const char *test, long index)
          {
            return test[index] == current ? index + 1 : -1;
          }

          static constexpr long end_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return single_match(test, index) != -1 ? next_match(test, index + 1, emt) : -1;
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return aop_t::is_operator ? aop_t::match(test, index, emt) : end_match(test, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << current;
            aop_t::print();
            next_t::print(indent);
          }

        public:
          using this_t = atom<Index, First, Str, Prev, IF, Parent>;

          // error check: the current char MUST NOT BE an operator
          static_assert(!atom_operator<First, this_t, Index, Str>::is_operator, "misplaced operator in regular expression"); // you may have something like "...+?" or "...|?"

          using aop_t = atom_operator<Str[Index + 1], this_t, Index + 1, Str>;
          constexpr static size_t next_index = aop_t::len + 1;
          using next_t = atom<Index + next_index, Str[Index + next_index], Str, this_t, false, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static char current = First;
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // DOT
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '.', Str, Prev, IF, Parent>
      {
        public:
          atom() = delete;

          static constexpr long single_match(const char *test, long index)
          {
            return test[index] ? index + 1 : -1;
          }

          static constexpr long end_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return (single_match(test, index) != -1) ? next_match(test, index + 1, emt) : -1;
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return aop_t::is_operator ? aop_t::match(test, index, emt) : end_match(test, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << '.';
            aop_t::print();
            next_t::print(indent);
          }

        public:
          using this_t = atom<Index, '.', Str, Prev, IF, Parent>;
          using aop_t = atom_operator<Str[Index + 1], this_t, Index + 1, Str>;
          constexpr static size_t next_index = aop_t::len + 1;
          using next_t = atom<Index + next_index, Str[Index + next_index], Str, this_t, false, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static char current = '.';
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // [range] and [^notrange]
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '[', Str, Prev, IF, Parent>
      {
        public:
          atom() = delete;

          static constexpr long single_match(const char *test, long index)
          {
            bool res = range_list_t::match(test[index]);
            if (exclude)
              res = !res;
            return res ? index + 1 : -1;
          }

          static constexpr long end_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return (single_match(test, index) != -1) ? next_match(test, index + 1, emt) : -1;
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return aop_t::is_operator ? aop_t::match(test, index, emt) : end_match(test, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << '[';
            if (exclude)
              std::cout << '^';
            range_list_t::print();
            std::cout << ']';
            aop_t::print();
            next_t::print(indent);
          }

        private: // helpers
          static constexpr bool exclude = (Str[Index + 1] == '^' ? 1 : 0);
          static constexpr size_t start_index = exclude ? 1 : 0;

          // C++14, 'cause we are in 2016 !
          // return the size of the [] expression, including the closing ].
          static constexpr long get_size()
          {
            size_t i = 1;
            bool escape = false;
            for (; Str[i + Index] != '\0'; ++i)
            {
              if (!escape)
              {
                switch (Str[i + Index])
                {
                  case '\\': escape = true;break;
                  case ']': return i;
                }
              }
              else
                escape = false;
            }
            return -1;
          }

          static constexpr size_t size = (size_t)get_size();
          static_assert(get_size() != -1, "Missing closing ] in regular expression"); // You have an opening [ but not a closing ].

          // return the number of ranges the [] construct has
          // (a range could either be a x-y couple or a single character x )
          static constexpr long get_range_count()
          {
            size_t count = 0;
            for (size_t i = 1 + start_index; i < size; ++i)
            {
              // skip backslashes
              if (Str[i + Index] == '\\')
                ++i;

              // peek next
              if (i + 1 < (size) && Str[i + 1 + Index] == '-') // we got a x-y
              {
                if (i + 2 >= (size)) // the last char is a -
                {
                  ++i;
                  ++count; // so we have two entries
                }
                else
                  i += 2; // +1 in the increment
                ++count;
              }
              else
                ++count;
            }
            return count;
          }

          static constexpr size_t range_count = (size_t)get_range_count();
          static_assert(get_range_count() != -1, "Invalid [] in regular expression"); // You may have something like "[a-]"
          static_assert(range_count != 0, "Empty [] in regular expression"); // You have an empty []

          // default range
          template<size_t RangeNum, size_t RangeCount, size_t RIndex>
          struct range_list
          {
            static constexpr size_t st_shift = Str[RIndex] == '\\' ? 1 : 0;
            static constexpr char start_range = Str[RIndex + st_shift];
            static constexpr bool is_range = Str[RIndex + 1 + st_shift] == '-' && Str[RIndex + 1 + st_shift + 1] != ']';

            static constexpr size_t end_shift = 1 + 1 + st_shift + (is_range && Str[RIndex + 2 + st_shift] == '\\' ? 1 : 0);
            static constexpr char end_range = is_range ? Str[RIndex + end_shift] : '\0';

            static constexpr size_t last_index = is_range ? (RIndex + end_shift + 1) : (RIndex + st_shift + 1);

            using rl_next_t = range_list<RangeNum + 1, RangeCount, last_index>;

            // match a character
            static constexpr bool match(char c)
            {
              if (is_range)
              {
                if (c >= start_range && c <= end_range)
                  return true;
                else return rl_next_t::match(c);
              }
              else
              {
                if (c != start_range)
                  return rl_next_t::match(c);
                else return true;
              }
            }

            static void print()
            {
              if (is_range)
                std::cout << start_range << '-' << end_range;
              else if ((start_range != '-' || RangeNum + 1 == RangeCount) && start_range != ']')
                std::cout << start_range;
              else
                std::cout << '\\' << start_range;
              rl_next_t::print();
            }
          };

          // The final match
          template<size_t RangeCount, size_t RIndex>
          struct range_list<RangeCount, RangeCount, RIndex>
          {
            // the final match: nothing has matched, return false
            static constexpr bool match(char) { return false; }
            static void print() {}
          };

          // create the range_list
          using range_list_t = range_list<0, range_count, Index + 1 + start_index>;

        public:
          using this_t = atom<Index, '[', Str, Prev, IF, Parent>;
          using aop_t = atom_operator<Str[Index + size + 1], this_t, Index + size + 1, Str>;
          constexpr static size_t next_index = aop_t::len + size + 1;
          using next_t = atom<Index + next_index, Str[Index + next_index], Str, this_t, false, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static char current = '[';
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // $
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '$', Str, Prev, IF, Parent>
      {
        public:
          atom() = delete;

          static constexpr long single_match(const char *test, long index)
          {
            return test[index] == 0 ? index : -1;
          }

          static constexpr long end_match(const char *test, long index, e_match_type = e_match_type::full)
          {
            return (single_match(test, index));
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return end_match(test, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << '$';
            next_t::print(indent);
          }

        public:
          using this_t = atom<Index, '$', Str, Prev, IF, Parent>;

          // error check: the current char MUST NOT BE an operator
          static_assert(!atom_operator<'$', this_t, Index, Str>::is_operator, "invalid $ operator (should not be an operator)"); // an operator for $ is also defined
          static_assert(!atom_operator<Str[Index + 1], this_t, Index, Str>::is_operator, "misplaced operator after $ in regular expression"); // you may have something like "...$+"

          constexpr static size_t next_index = 1;
          using next_t = atom<Index + next_index, Str[Index + next_index], Str, this_t, false, Parent>;
          static_assert(next_t::is_end || next_t::is_head, "a non-ending token is present after $ in regular expression");  // you may have "...$a" or "...$..."
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static char current = '$';
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // backslash
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '\\', Str, Prev, IF, Parent>
      {
        public:
          atom() = delete;

          static constexpr long single_match(const char *test, long index)
          {
            return test[index] == current ? index + 1 : -1;
          }

          static constexpr long end_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return (single_match(test, index) != -1) ? next_match(test, index + 1, emt) : -1;
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return aop_t::is_operator ? aop_t::match(test, index, emt) : end_match(test, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << '\\' << current;
            aop_t::print();
            next_t::print(indent);
          }

        public:
          static_assert(Str[Index + 1], "trailing backslash");  // you have a backslash at the very end of your regular expression

          using this_t = atom<Index, '\\', Str, Prev, IF, Parent>;
          using aop_t = atom_operator<Str[Index + 2], this_t, Index + 2, Str>;
          constexpr static size_t next_index = aop_t::len + 2;
          using next_t = atom<Index + next_index, Str[Index + next_index], Str, this_t, false, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static char current = Str[Index + 1];
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // (
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '(', Str, Prev, IF, Parent>
      {
        private: // helper
          static constexpr e_match_type get_match_type_for_child(e_match_type emt)
          {
            return (static_cast<unsigned>(emt) ? static_cast<e_match_type>(static_cast<unsigned>(emt) + 1) : emt);
          }

        public:
          atom() = delete;

          static constexpr long end_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            //return single_match(s) ? next_match(single_match(s), emt) : nullptr;
            return child_t::head_t::is_head ? child_t::head_t::pipe_match(s, index, get_match_type_for_child(emt)) : child_t::match(s, index, get_match_type_for_child(emt));
          }

          static constexpr long next_match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return next_t::match(s, index, emt);
          }

          static constexpr long single_match(const char *s, long index)
          {
            return child_t::head_t::is_head ? child_t::head_t::pipe_match(s, index, e_match_type::single_branch) : child_t::match(s, index, (e_match_type::single_branch));
          }

          static constexpr long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return aop_t::is_operator ? aop_t::match(s, index, get_match_type_for_child(emt)) : end_match(s, index, emt);
          }

          static void print(size_t indent)
          {
            std::cout << '(';
            child_t::print(indent + 1);
            aop_t::print();
            next_t::print(indent);
          }

        public:
          constexpr static char current = '(';
          using this_t = atom<Index, '(', Str, Prev, IF, Parent >;
          using child_t = atom<Index + 1, Str[Index + 1], Str, no_prev, true, this_t>;

          // error checking (here to be the first printed) (missing closing bracket)
          static_assert(Str[child_t::end], "missing closing bracket in regular expression construct");

          using aop_t = atom_operator<Str[child_t::end + 1], this_t, child_t::end + 1, Str>;
          constexpr static size_t next_index = aop_t::len + 1;
          using next_t = atom<child_t::end + next_index, Str[next_index + child_t::end], Str, this_t, false, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using parent_t = Parent;
          using head_t = typename next_t::head_t;

          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent >
      class atom<Index, ')', Str, Prev, IF, Parent>
      {
        private: // helper
          static constexpr e_match_type get_match_type_for_parent(e_match_type emt)
          {
            return (emt != e_match_type::full ? static_cast<e_match_type>(static_cast<unsigned>(emt) - 1) : emt);
          }

        public:
          atom() = delete;

          static constexpr long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return emt == e_match_type::single_branch ? index : Parent::next_t::match(s, index, get_match_type_for_parent(emt));
          }

          static void print(size_t)
          {
            std::cout << ')';
          }

        public:
          // error check
          static_assert(!IF, "empty sub-expressions are forbidden in regular expression"); // you have: "...()..."

          using this_t = atom<Index, ')', Str, Prev, IF, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using head_t = no_head;
          using parent_t = Parent;

          constexpr static char current = ')';
          constexpr static bool is_end = true;
          constexpr static size_t end = Index;
          constexpr static bool is_head = false;
          constexpr static bool is_first = IF;
      };

      // OR
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent>
      class atom<Index, '|', Str, Prev, IF, Parent>
      {
        public:
          atom() = delete;

          static constexpr long match(const char *s, long index, e_match_type emt = e_match_type::full)
          {
            return (std::is_same<Parent, no_parent>::value || static_cast<unsigned>(emt) != 0 ? index : parent_t::next_t::match(s, index, emt));
          }

          static void print(size_t indent)
          {
            std::cout << '|';
            next_t::print(indent);
          }

        public:
          using this_t = atom<Index, '|', Str, Prev, IF, Parent>;
          constexpr static size_t next_index = 1;
          using next_t = atom<Index + next_index, Str[next_index + Index], Str, this_t, false, Parent>;

          // error check
          static_assert(!IF, "OR token shouldn't be the first in regular (sub)expression");      // you have something like that: "(|...)" or "|..."
          static_assert(Str[Index + 1], "OR token shouldn't be the last in regular expression"); // you have something like that: "(...|)" or "...|"
          static_assert(next_t::is_head == false, "misplaced OR token in regular expression");   // you have something like that: "...||..."

          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using head_t = this_t;
          using parent_t = Parent;

          constexpr static char current = '|';
          constexpr static bool is_end = false;
          constexpr static size_t end = next_t::end;
          constexpr static bool is_head = true;
          constexpr static bool is_first = IF;

        private:
          template<typename Curr>
          static constexpr long first_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return Curr::prev_t::is_head || std::is_same<typename Curr::prev_t, Curr>::value ?
                  Curr::match(test, index, emt) : first_match<typename Curr::prev_t>(test, index, emt);
          }

        public:
          static constexpr long pipe_match(const char *test, long index, e_match_type emt = e_match_type::full)
          {
            return (first_match<prev_t>(test, index, emt) != -1) ? first_match<prev_t>(test, index, emt) :
                  (next_t::head_t::is_head ? next_t::head_t::pipe_match(test, index, emt) : next_t::match(test, index, emt));
          }
      };

      // END
      template <size_t Index, const char *Str, typename Prev, bool IF, typename Parent >
      class atom<Index, 0, Str, Prev, IF, Parent>
      {
        public:
          constexpr atom() {}

          static constexpr long match(const char *, long index, e_match_type = e_match_type::full)
          {
            return index;
          }

          static void print(size_t)
          {
          }

        public:
          using this_t = atom<Index, 0, Str, Prev, IF, Parent>;
          using prev_t = typename std::conditional<std::is_same<Prev, no_prev>::value, this_t, Prev>::type;
          using head_t = no_head;
          using parent_t = Parent;

          constexpr static char current = 0;
          constexpr static bool is_end = true;
          constexpr static size_t end = Index;
          constexpr static bool is_head = false;
      };
    } // namespace internal
  } // namespace ct
} // namespace neam

#endif /*__NCT_ATOM_HPP__*/

