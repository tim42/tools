//
// file : ct_union_tuple.hpp
// in : file:///home/tim/projects/alphyn/alphyn/tools/ct_union_tuple.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: Mon Feb 29 2016 15:55:05 GMT+0100 (CET)
//
//
// Copyright (c) 2016 Timothée Feuillet
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
#ifndef __N_251786322243149703_111686023__CT_TUPLE_HPP__
# define __N_251786322243149703_111686023__CT_TUPLE_HPP__

#include <tuple>
#include <utility>
#include "ct_list.hpp"

namespace neam
{
  namespace ct
  {
    /// \brief Manage an union of multiple types, with compile-time in mind
    /// \param TypeList The ct::type_list<> of all types
    /// \note It currently only manages well types that does not perform dynamic allocation
    template<typename TypeList>
    class tuple
    {
      private: // node
        template<size_t Index, typename...> struct node
        {
          constexpr node() = default;

          template<typename RetType, size_t CIndex>
          struct getter
          {
            constexpr static RetType apply(const node &) { return RetType(); }
          };
          template<typename Type, size_t CIndex>
          struct setter
          {
            constexpr static void apply(const node &, Type) { /*throw std::runtime_error("union_tuple: out of range access");*/ }
          };
          constexpr void rec_clear() {}
          constexpr void clear() {}
        };

        template<size_t Index, typename Current, typename... Other>
        struct node<Index, Current, Other...> : public node<Index + 1, Other...>
        {
          constexpr node() = default;

          Current value = decltype(value)();

          using next_t = node < Index + 1, Other... >;

          /// \brief getter structure
          template<typename RetType, size_t CIndex> struct getter : public next_t::template getter<RetType, CIndex - 1> {};
          template<typename RetType>
          struct getter<RetType, 0>
          {
            constexpr static RetType apply(const node &n)
            {
              return n.value;
            }
          };

          /// \brief setter structure
          template<typename Type, size_t CIndex>
          struct setter : public next_t::template setter<Type, CIndex - 1> {};
          template<typename Type>
          struct setter<Type, 0>
          {
            constexpr static void apply(node &n, Type val)
            {
              n.value = val;
            }
          };
        };

      private: // helpers
        template<typename... X>
        using node_rec_list = node<0, X...>;

      public: // types
        /// \brief The underlying type_list
        using type_list = typename TypeList::make_unique;

      private:
        using node_list_t = typename ct::extract_types<node_rec_list, type_list>::type;


      public:
        constexpr tuple() = default;

        /// \brief Return the value held for type T
        template<typename T>
        constexpr auto get() const -> typename std::remove_cv<typename std::remove_reference<T>::type>::type
        {
          using stored_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
          constexpr long index = type_list::template get_type_index<stored_type>::index;
          static_assert(index >= 0, "type not in union_tuple_list");
          return node_list_t::template getter<stored_type, index>::apply(node_list);
        }

        /// \brief Set a value
        template<typename T>
        constexpr void set(T val)
        {
          using stored_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
          constexpr long index = type_list::template get_type_index<stored_type>::index;
          static_assert(index >= 0, "type not in union_tuple_list");
          node_list_t::template setter<T, index>::apply(node_list, val);
        }

      private:
        node_list_t node_list = node_list_t();
    };
  } // namespace ct
} // namespace neam

#endif /*__N_251786322243149703_111686023__CT_TUPLE_HPP__*/