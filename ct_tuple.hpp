//
// file : ct_union_tuple.hpp
// in : file:///home/tim/projects/alphyn/alphyn/tools/ct_union_tuple.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: Mon Feb 29 2016 15:55:05 GMT+0100 (CET)
//
//
// Copyright (C) 2016 Timothée Feuillet
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