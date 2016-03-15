//
// file : ct_list.hpp
// in : file:///home/tim/projects/yaggler/yaggler/klmb/ct_list.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 27/01/2014 16:26:19
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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
          using last = void;
        };
        template<typename First, typename... Nexts>
        struct except_first<First, Nexts...>
        {
          using type = type_list<Nexts...>;
          using first = First;
          using last = typename type_list<First, Nexts...>::template get_type<sizeof...(Nexts)>;
        };

        template<typename... O>
        struct _pop_back
        {
          using type = type_list<>;
        };
        template<typename First, typename... Nexts>
        struct _pop_back<First, Nexts...>
        {
          using type = typename type_list<First, Nexts...>::template sublist<0, sizeof...(Nexts)>;
        };

        // conditionally append (can't use the append_type_if in merge_pack.hpp)
        template <bool Cond, typename Cr, typename Type> struct _append_type_if {};
        template <typename Cr, typename ...Others>
        struct _append_type_if<true, Cr, ct::type_list<Others...>> { using type = ct::type_list<Others..., Cr>; };
        template <typename Cr, typename ...Others>
        struct _append_type_if<false, Cr, ct::type_list<Others...>> { using type = ct::type_list<Others...>; };
        // can't use merger for the same reason
        template<typename Type1, typename Type2> struct _merger {};
        template<typename... Types1, typename... Types2>
        struct _merger<ct::type_list<Types1...>, ct::type_list<Types2...>> { using type_list = ct::type_list<Types1..., Types2...>; };

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
        template<template<typename X, typename Y> class Predicate, typename List, typename...> struct unique_filter_pred { using type = List; };
        template<template<typename X, typename Y> class Predicate, typename List, typename Current, typename... OtherTypes>
        struct unique_filter_pred<Predicate, List, Current, OtherTypes...>
        {
          using type = typename unique_filter_pred
          <
            Predicate,
            typename _append_type_if<Predicate<List, Current>::value, Current, List>::type,
            OtherTypes...
          >::type;
        };
        // conditionally merge
        template<bool ExpectedValue, template<typename X> class Predicate, typename List, typename...> struct cmerge_filter { using type = List; };
        template<bool ExpectedValue, template<typename X> class Predicate, typename List, typename Current, typename... OtherTypes>
        struct cmerge_filter<ExpectedValue, Predicate, List, Current, OtherTypes...>
        {
          using type = typename cmerge_filter
          <
            ExpectedValue,
            Predicate,
            typename _append_type_if<(Predicate<Current>::value == ExpectedValue), Current, List>::type,
            OtherTypes...
          >::type;
        };
        // flatten merge
        template<typename List, typename...> struct flatten_merge_filter { using type = List; };
        template<typename List, typename Current, typename... OtherTypes>
        struct flatten_merge_filter<List, Current, OtherTypes...>
        {
          using type = typename flatten_merge_filter
          <
            typename _append_type_if<true, Current, List>::type,
            OtherTypes...
          >::type;
        };
        template<typename List, typename... Current, typename... OtherTypes>
        struct flatten_merge_filter<List, type_list<Current...>, OtherTypes...>
        {
          using type = typename flatten_merge_filter
          <
            typename _merger<type_list<Current...>, List>::type_list,
            OtherTypes...
          >::type;
        };
        template<size_t StartIndex, size_t Size, typename Result, typename... OTypes>
        struct _sublist_gen
        {
          using type = Result;
        };
        template<size_t StartIndex, size_t Size, typename Result>
        struct _sublist_gen<StartIndex, Size, Result> // the end (of the list)
        {
          using type = Result;
        };
        template<typename Result, typename CType, typename... OTypes>
        struct _sublist_gen<0, 0, Result, CType, OTypes...> // the end (size has been reached)
        {
          using type = Result;
        };

        template<size_t StartIndex, size_t Size, typename Result, typename CType, typename... OTypes>
        struct _sublist_gen<StartIndex, Size, Result, CType, OTypes...> : public _sublist_gen<StartIndex - 1, Size, Result, OTypes...>
        {
        };
        template<size_t Size, typename Result, typename CType, typename... OTypes>
        struct _sublist_gen<0, Size, Result, CType, OTypes...> : public _sublist_gen<0, Size - 1, typename Result::template append<CType>, OTypes...>
        {
        };

        template<bool, typename Res, typename... Next> struct _reverse {};

        template<bool X, typename Res, typename Current, typename... Next>
        struct _reverse<X, Res, Current, Next...> : public _reverse<X, typename Res::template prepend<Current>, Next...> {};
        template<bool X, typename Res>
        struct _reverse<X, Res>
        {
          using type = Res;
        };

      public:

        template<size_t Index>
        using get_type = typename type_at_index<Index, Types...>::type;

        // get_type_index<int>::index will give the first index of "int" in the list. -1 if not found.
        template<typename Type>
        using get_type_index = type_index<0, Type, Types...>;

        // Predicate Should be a class that takes an unique parameter and defines a boolean static attribute value.
        // ::index will give you the index, could be -1 if not found
        // ::type will give you the type, could be ct::type_not_found if not found
        template<template<typename X> class Predicate>
        using find_if = find_type_index<Predicate, 0, Types...>;

        // defines a static property value that is a boolean indicating if the type is in the list
        template<typename Type>
        using is_in_list = ct::is_in_list<ct::type_list<Types...>, Type>;

        // Create a new list without all types that matches Predicate
        // Predicate Should be a class that takes an unique parameter and defines a boolean ::value.
        template<template<typename X> class Predicate>
        using remove_if = typename cmerge_filter<false, Predicate, ct::type_list<>, Types...>::type;

        // Predicate Should be a class that takes an unique parameter and defines a type ::type.
        // It applies the predicate on every type of the list and return the generated list
        template<template<typename X> class Predicate>
        using for_each = ct::type_list<typename Predicate<Types>::type...>;

        // Same as for_each, but the Predicate itself replace the type
        template<template<typename X> class Predicate>
        using direct_for_each = ct::type_list<Predicate<Types>...>;

        // Filter the list by a predicate (takes an element as template parameter, has a static (constexpr) attribute value that can be casted to a boolean)
        // The result type is the list that contains all the elements for whose Predicate<>::value has been true.
        // It is the exate opposite of remove_if
        template<template<typename X> class Predicate>
        using filter_by = typename cmerge_filter<true, Predicate, ct::type_list<>, Types...>::type;

        // remove duplicates
        using make_unique = typename unique_filter<ct::type_list<>, Types...>::type;

        // remove duplicates, with a predicate to indicate if there's already CurrentElement in List
        template<template<typename List, typename CurrentElement> class Predicate>
        using make_unique_pred = typename unique_filter_pred<Predicate, ct::type_list<>, Types...>::type;

        // merge type_list<>
        using flatten = typename flatten_merge_filter<ct::type_list<>, Types...>::type;

        using tuple_type = cr::tuple<Types...>;
        static constexpr size_t size = sizeof...(Types);

        template<size_t StartIndex, size_t Size>
        using sublist = typename _sublist_gen<StartIndex, Size, ct::type_list<>, Types...>::type;

        using pop_front = typename except_first<Types...>::type;
        using front = typename except_first<Types...>::first;
        using pop_back = typename _pop_back<Types...>::type;
        using back = typename except_first<Types...>::last;

        template<typename... OtherTypes>
        using append = ct::type_list<Types..., OtherTypes...>;
        template<typename... OtherTypes>
        using prepend = ct::type_list<OtherTypes..., Types...>;

        template<typename List>
        using append_list = typename _merger<ct::type_list<Types...>, List>::type_list;
        template<typename List>
        using prepend_list = typename _merger<List, ct::type_list<Types...>>::type_list;

        // reverse the order of the list (front is back, and back is front)
        // NOTE: The GCC bug is very VERY nasty as it make the compiler consume the whole available memory
        //       It seems the only workaround is to make this a template to avoid that HUGE memory consumption
        //       The value of the boolean template parameter does not mean anything and any value will fit.
        template<bool WorkAroundGccBug>
        using reverse = typename _reverse<WorkAroundGccBug, type_list<>, Types...>::type;

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

