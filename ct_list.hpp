//
// file : ct_list.hpp
// in : file:///home/tim/projects/yaggler/yaggler/klmb/ct_list.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 27/01/2014 16:26:19
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

#ifndef __N_48888409651922371_1596013433__CT_LIST_HPP__
# define __N_48888409651922371_1596013433__CT_LIST_HPP__

#include <memory>
#include <type_traits>

#include "type_at_index.hpp"
#include "tuple.hpp"

namespace neam
{
  namespace ct
  {
    /// \brief Check if a type is in a list
    template<typename TypeList, typename Type>
    struct is_in_list
    {
      static constexpr bool value = TypeList::template get_type_index<Type>::index != -1;
    };

    /// \brief a list of types
    template<typename... Types>
    struct type_list
    {
      private:
        template<typename... O>
        struct except_first
        {
          using type = type_list<>;
          using first = void;
        };
        template<typename First, typename... Nexts>
        struct except_first<First, Nexts...>
        {
          using type = type_list<Nexts...>;
          using first = First;
        };

        // conditionally append (can't use the append_type_if in merge_pack.hpp)
        template <bool Cond, typename Cr, typename Type> struct _append_type_if {};
        template <typename Cr, typename ...Others>
        struct _append_type_if<true, Cr, ct::type_list<Others...>> { using type = ct::type_list<Others..., Cr>; };
        template <typename Cr, typename ...Others>
        struct _append_type_if<false, Cr, ct::type_list<Others...>> { using type = ct::type_list<Others...>; };

        // filter for uniqueness
        template<typename List, typename...> struct unique_filter { using type = List; };
        template<typename List, typename Current, typename... OtherTypes>
        struct unique_filter<List, Current, OtherTypes...>
        {
          using type = typename unique_filter
          <
            typename _append_type_if<!is_in_list<List, Current>::value, Current, List>::type,
            OtherTypes...
          >::type;
        };
        // conditionally merge
        template<template<typename X> class Predicate, typename List, typename...> struct cmerge_filter { using type = List; };
        template<template<typename X> class Predicate, typename List, typename Current, typename... OtherTypes>
        struct cmerge_filter<Predicate, List, Current, OtherTypes...>
        {
          using type = typename cmerge_filter
          <
            Predicate,
            typename _append_type_if<!Predicate<Current>::value, Current, List>::type,
            OtherTypes...
          >::type;
        };

      public:
        template<size_t Index>
        using get_type = typename type_at_index<Index, Types...>::type;

        // get_type_index<int>::index will give the first index of "int" in the list. -1 if not found.
        template<typename Type>
        using get_type_index = type_index<0, Type, Types...>;

        // Predicate Should be a class that takes an unique parameter and defines a boolean ::value.
        // ::index will give you the index, could be -1 if not found
        template<template<typename X> class Predicate>
        using find_if = find_type_index<Predicate, 0, Types...>;

        // Create a new list without all types that matches Predicate
        // Predicate Should be a class that takes an unique parameter and defines a boolean ::value.
        template<template<typename X> class Predicate>
        using remove_if = typename cmerge_filter<Predicate, ct::type_list<>, Types...>::type;

        // Predicate Should be a class that takes an unique parameter and defines a type ::type.
        // It applies the predicate on every type of the list and return the generated list
        template<template<typename X> class Predicate>
        using for_each = ct::type_list<typename Predicate<Types>::type...>;

        // remove duplicates
        using make_unique = typename unique_filter<ct::type_list<>, Types...>::type;

        using tuple_type = cr::tuple<Types...>;
        static constexpr size_t size = sizeof...(Types);

        using pop_front = typename except_first<Types...>::type;
        using front = typename except_first<Types...>::first;


        template<typename... Values>
        static constexpr cr::tuple<Types...> instanciate_tuple(Values... vals)
        {
          return cr::tuple<Types...>(std::move(vals)...);
        }

        template<size_t Index, typename... Values>
        static typename type_at_index<Index, Types...>::type instanciate_one(Values... vals)
        {
          return type_at_index<Index, Types...>::type(std::move(vals)...);
        };
    };

    // a list of types, with a static instance of the tuple
    template<typename... Types>
    struct type_list_static_instance : public type_list<Types...>
    {
      static cr::tuple<Types...> instance;
    };

    template<typename... Types>
    cr::tuple<Types...> type_list_static_instance<Types...>::instance;

    // a list of types, with a non-static instance of the tuple
    template<typename... Types>
    struct type_list_member_instance : public type_list<Types...>
    {
      type_list_member_instance() : instance() {}
      type_list_member_instance(const type_list_member_instance &o) : instance(o.instance) {}
      type_list_member_instance(type_list_member_instance &&o) : instance(std::move(o.instance)) {}
      type_list_member_instance(const cr::tuple<Types...> &o) : instance(o) {}
      type_list_member_instance(cr::tuple<Types...> &&o) : instance(std::move(o)) {}

      template<typename... Vals>
      explicit type_list_member_instance(Vals... vals) : instance(vals...) {}


      cr::tuple<Types...> instance;
    };

    // send a list of types in another class as template parameters
    template<template<typename... X> class Class, typename Type>
    struct extract_types
    {
    };

    template<template<typename... X> class Class, typename... Types>
    struct extract_types<Class, type_list<Types...>>
    {
      using type = Class<Types...>;
    };

    template<template<typename... X> class Class, typename... Types>
    struct extract_types<Class, cr::tuple<Types...>>
    {
      using type = Class<Types...>;
    };

    // an helper to construct a type list from a list of value
    template<typename... Types>
    static constexpr type_list<Types...> type_list_from_values(Types...)
    {
      return type_list<Types...>();
    }

// a macro to get the type_list from a variadic list of arguments.
#define N_CT_TYPE_LIST_FROM_VALUES(...)         decltype(neam::ct::type_list_from_values(__VA_ARGS__))

  } // namespace ct
} // namespace neam

#endif /*__N_48888409651922371_1596013433__CT_LIST_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

