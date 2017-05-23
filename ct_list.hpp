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
#include <tuple>

namespace neam
{
  namespace ct
  {
    /// \brief Set of type function that operate with any class with variadic template parameter
    /// Those class can be std::tuple, ct::type_list, ...
    namespace list
    {
      namespace internal
      {
        // return the type at a given index
        template<size_t Index, typename... Types> struct type_at_index {};
        template<typename Current, typename... Other, size_t Index>
        struct type_at_index<Index, Current, Other...> : public type_at_index<Index - 1, Other...> {};
        template<typename Current, typename... Other>
        struct type_at_index<0, Current, Other...>
        {
          using type = Current;
        };
        template<size_t Index> struct type_at_index<Index> { static_assert(!(Index + 1), "Out of range access in get_type"); };

        template<typename List, size_t Index> struct type_at_index_list {};
        template<template<typename...> class List, typename... Types, size_t Index>
        struct type_at_index_list<List<Types...>, Index> : public type_at_index<Index, Types...> {};

        // return the index of a given type
        template<size_t Index, typename Type, typename... Types>
        struct type_index
        {
          static constexpr long index = -1;
        };
        template<size_t Index, typename Current, typename... Types>
        struct type_index<Index, Current, Current, Types...>
        {
          static constexpr long index = Index;
        };

        template<size_t Index, typename Type, typename Current, typename... Types>
        struct type_index<Index, Type, Current, Types...> : public type_index < Index + 1, Type, Types... > {};

        template<size_t Index, typename Type>
        struct type_index<Index, Type>
        {
          static constexpr long index = -1;
        };
        template<typename List, typename Type> struct type_index_list {};
        template<template<typename...> class List, typename... Types, typename Type>
        struct type_index_list<List<Types...>, Type> : public type_index<0, Type, Types...> {};

        template<typename List, typename Type>
        struct has_type_s : public std::conditional<(internal::type_index_list<List, Type>::index != -1), std::true_type, std::false_type>
        {};
      } // namespace internal

      /// \brief Return the type at a given index. static_assert if out of range
      template<typename List, size_t Index>
      using get_type = typename internal::type_at_index_list<List, Index>::type;

      /// \brief Return the index of a given type, or -1 if not found
      /// \see has_type
      template<typename List, typename Type>
      static constexpr long index_of = internal::type_index_list<List, Type>::index;

      /// \brief true if the list has a given type, false otherwise
      template<typename List, typename Type>
      static constexpr bool has_type = (internal::type_index_list<List, Type>::index != -1);

      namespace internal
      {
        template<typename List, typename... Y> struct prepend_s {};
        template<typename List, typename... Y> struct append_s {};

        template<template<typename...> class Type, typename... Others, typename... Types>
        struct prepend_s<Type<Others...>, Types...>
        {
          using type = Type<Types..., Others...>;
        };

        template<template<typename...> class Type, typename... Others, typename... Types>
        struct append_s<Type<Others...>, Types...>
        {
          using type = Type<Others..., Types...>;
        };
      } // namespace internal

      /// \brief Prepend an arbitrary number of types to a list
      template<typename List, typename... Y>
      using prepend = typename internal::prepend_s<List, Y...>::type;

      /// \brief Append an arbitrary number of types to a list
      template<typename List, typename... Y>
      using append = typename internal::append_s<List, Y...>::type;

      /// \brief conditionally prepend an arbitrary number of types to a list
      template<bool Cond, typename List, typename... Y>
      using append_if = typename std::conditional<Cond, list::append<List, Y...>, List>::type;

      /// \brief conditionally append an arbitrary number of types to a list
      template<bool Cond, typename List, typename... Y>
      using prepend_if = typename std::conditional<Cond, list::prepend<List, Y...>, List>::type;

      /// \brief Merge two lists (they can have different base type)
      template<typename List1, typename List2>
      struct merge
      {
        using as_first = void;
        using as_second = void;
      };

      template<template<typename...> class L1, typename... Types1, template<typename...> class L2, typename... Types2>
      struct merge<L1<Types1...>, L2<Types2...>>
      {
        template<template<typename...> class ResT>
        using as = ResT<Types1..., Types2...>;

        using as_first = L1<Types1..., Types2...>;
        using as_second = L2<Types1..., Types2...>;
      };

      /// \brief Extract template arguments and forward them to another base type
      template<typename List> struct extract {};

      template<template<typename...> class List, typename... Types>
      struct extract<List<Types...>>
      {
        template<template<typename...> class ResT>
        using as = ResT<Types...>;
      };

      namespace internal
      {
        template<typename List> struct size_s {};
        template<template<typename...> class List, typename... Types>
        struct size_s<List<Types...>>
        {
          static constexpr size_t count = sizeof...(Types);
        };

        template<typename List1, typename List2> struct same_base_s : public std::false_type {};

        template<template<typename...> class L1, typename... Types1, typename... Types2>
        struct same_base_s<L1<Types1...>, L1<Types2...>> : public std::true_type {};
      } // namespace internal

      /// \brief The number of template arguments in the list
      template<typename List>
      static constexpr size_t size = internal::size_s<List>::count;

      /// \brief True if both list are the same base (std::tuple, ct::type_list, ...)
      /// same_base< std::tuple< int, double, float >, std::tuple< std::string > > will be true
      /// same_base< ct::type_list< std::string >, std::tuple< std::string > > will be false
      template<typename List1, typename List2>
      static constexpr bool same_base = internal::same_base_s<List1, List2>::value;

      namespace internal
      {
        template<typename List, long Index, typename... Types>
        struct remove_s { using type = List; };
        template<typename List, long Index, typename Current, typename... Types>
        struct remove_s<List, Index, Current, Types...> :
        public remove_s<append_if<Index != 0, List, Current>, Index - 1, Types...> {};

        template<typename List, template<typename X> typename Pred, bool ExpectedResult, typename... Types>
        struct remove_if_s { using type = List; };
        template<typename List, template<typename X> typename Pred, bool ExpectedResult, typename Current, typename... Types>
        struct remove_if_s<List, Pred, ExpectedResult, Current, Types...> :
        public remove_if_s<append_if<Pred<Current>::value != ExpectedResult, List, Current>, Pred, ExpectedResult, Types...> {};

        template<typename List, size_t Index> struct remove_entry_s {};
        template<template<typename...> class List, typename... Types, size_t Index>
        struct remove_entry_s<List<Types...>, Index> : public remove_s<List<>, Index, Types...> {};

        template<typename List, template<typename X> typename Pred, bool ExpectedResult> struct remove_if_entry_s {};
        template<template<typename...> class List, typename... Types, template<typename X> typename Pred, bool ExpectedResult>
        struct remove_if_entry_s<List<Types...>, Pred, ExpectedResult> : public remove_if_s<List<>, Pred, ExpectedResult, Types...> {};

        template<typename X>
        struct remove_type_s
        {
          template<typename Y>
          struct pred : public std::is_same<X, Y> {};

          template<typename List>
          using list_op = remove_if_entry_s<List, pred, true>;
        };

        template<typename List, template<typename X> typename Pred> struct for_each_s {};
        template<template<typename...> class List, typename... Types, template<typename X> typename Pred>
        struct for_each_s<List<Types...>, Pred>
        {
          using type = List<Pred<Types>...>;
        };
        template<typename List, template<typename X> typename Pred> struct for_each_indirect_s {};
        template<template<typename...> class List, typename... Types, template<typename X> typename Pred>
        struct for_each_indirect_s<List<Types...>, Pred>
        {
          using type = List<typename Pred<Types>::type...>;
        };

        template<typename List, typename... Types>
        struct make_unique_s { using type = List; };
        template<typename List, typename Current, typename... Types>
        struct make_unique_s<List, Current, Types...> : public make_unique_s<append_if<!has_type<List, Current>, List, Current>, Types...> {};

        template<typename List> struct make_unique_list_s {};
        template<template<typename...> class List, typename... Types> struct make_unique_list_s<List<Types...>> : public make_unique_s<List<>, Types...> {};
      } // namespace internal

      /// \brief Remove a type at a given index
      template<typename List, size_t Index>
      using remove = typename internal::remove_entry_s<List, Index>::type;

      /// \brief Remove a given type
      template<typename List, typename ToRemove>
      using remove_type = typename internal::remove_type_s<ToRemove>::template list_op<List>::type;

      /// \brief Remove elements for whose the predicate (Pred< X >::value) is true
      template<typename List, template<typename Elem> typename Pred>
      using remove_if = typename internal::remove_if_entry_s<List, Pred, true>::type;

      /// \brief Return a list containing only elements for which the predicate (Pred< X >::value) is true
      template<typename List, template<typename Elem> typename Pred>
      using filter = typename internal::remove_if_entry_s<List, Pred, false>::type;

      /// \brief Remove elements for whose the predicate (Pred< X >::value) is true
      template<typename List>
      using make_unique = typename internal::make_unique_list_s<List>::type;

      /// \brief Remove the first element
      template<typename List>
      using pop_front = remove<List, 0>;
      /// \brief Remove the last element
      template<typename List>
      using pop_back = remove<List, size<List> - 1>;

      /// \brief Apply a predicate to every type of the list
      template<typename List, template<typename Elem> typename Pred>
      using for_each = typename internal::for_each_s<List, Pred>::type;

      /// \brief Same as for_each but retrieve use type in the Pred< X >::type alias
      template<typename List, template<typename Elem> typename Pred>
      using for_each_indirect = typename internal::for_each_indirect_s<List, Pred>::type;

      namespace internal
      {
        template<typename List>
        struct flatten_list {};
        template<typename List>
        struct flatten_rec_list {using type = List;};

        template<typename List, typename... Types>
        struct flatten_s
        {
          using type = List;
        };
        template<typename List, typename Current, typename... Others>
        struct flatten_s<List, Current, Others...> : public flatten_s
        <
          typename std::conditional
          <
            same_base<List, Current>,
            typename merge<List, Current>::as_first,
            append<List, Current>
          >::type,
          Others...
        > {};

        template<typename List, typename... Types>
        struct flatten_rec_s
        {
          using type = List;
        };
        template<template<typename...> class List, typename... Types>
        struct flatten_rec_list<List<Types...>> : public flatten_rec_s<List<>, Types...>{};

        template<typename List, typename Current, typename... Others>
        struct flatten_rec_s<List, Current, Others...> : public flatten_rec_s
        <
          typename std::conditional
          <
            same_base<List, Current>,
            typename merge<List, typename flatten_rec_list<Current>::type>::as_first,
            append<List, Current>
          >::type,
          Others...
        > {};

        template<typename List, typename... Types>
        struct reverse_s
        {
          using type = List;
        };
        template<typename List, typename Current, typename... Others>
        struct reverse_s<List, Current, Others...> : public reverse_s<prepend<List, Current>, Others...> {};

        template<typename List, size_t CurrentIndex, size_t Start, size_t Count, typename... Types>
        struct sublist_s
        {
          using type = List;
        };
        template<typename List, size_t CurrentIndex, size_t Start, size_t Count, typename Current, typename... Types>
        struct sublist_s<List, CurrentIndex, Start, Count, Current, Types...> :
        public sublist_s<append_if<CurrentIndex >= Start && CurrentIndex < Start + Count, List, Current>, CurrentIndex + 1, Start, Count, Types...> {};


        template<template<typename...> class List, typename... Types>
        struct flatten_list<List<Types...>> : public flatten_s<List<>, Types...>{};

        template<typename List>
        struct reverse_list {};
        template<template<typename...> class List, typename... Types>
        struct reverse_list<List<Types...>> : public reverse_s<List<>, Types...>{};

        template<typename List, size_t Start, size_t Count>
        struct sublist_list {};
        template<template<typename...> class List, typename... Types, size_t Start, size_t Count>
        struct sublist_list<List<Types...>, Start, Count> : public sublist_s<List<>, 0, Start, Count, Types...>{};
      } // namespace internal

      /// \brief Flatten the list.
      /// a list like list< a, list< b, c, d >, e, list< >, f, list< g > >, after a flatten will look like
      /// list< a, b, c, d, e, f, g >
      template<typename List>
      using flatten = typename internal::flatten_list<List>::type;

      /// \brief A recursive flatten
      template<typename List>
      using flatten_rec = typename internal::flatten_rec_list<List>::type;

      /// \brief Reverse a list
      template<typename List>
      using reverse = typename internal::reverse_list<List>::type;

      // sublist
      template<typename List, size_t StartIndex, size_t Count>
      using sublist = typename internal::sublist_list<List, StartIndex, Count>::type;

    } // namespace list

    /// \brief A really simple type list class.
    /// Cannot be instantiated
    template<typename... Types> class type_list;


    // some tests to check that everything is OK (the file test.cpp define it)
#ifdef NCT_TESTS
    static_assert(std::is_same_v<list::get_type<type_list<int, double, float>, 2>, float>, "list::get_type: failed");
    static_assert(std::is_same_v<list::get_type<type_list<int, double, float>, 0>, int>, "list::get_type: failed");

    static_assert(list::index_of<type_list<int, double, float>, int> == 0, "list::index_of: failed");
    static_assert(list::index_of<type_list<int, double, float>, float> == 2, "list::index_of: failed");
    static_assert(list::index_of<type_list<int, double, float>, char> == -1, "list::index_of: failed");

    static_assert(list::has_type<type_list<int, double, float>, char> == false, "list::has_type: failed");
    static_assert(list::has_type<type_list<int, double, float>, double> == true, "list::has_type: failed");
    static_assert(list::has_type<type_list<int, double, float>, int> == true, "list::has_type: failed");

    static_assert(std::is_same_v<list::append<type_list<int, double, float>, char, int>, type_list<int, double, float, char, int>>, "list::append: failed");
    static_assert(std::is_same_v<list::append<type_list<int, double, float>, int>, type_list<int, double, float, int>>, "list::append: failed");
    static_assert(std::is_same_v<list::append<type_list<int, double, float>>, type_list<int, double, float>>, "list::append: failed");

    static_assert(std::is_same_v<list::prepend<type_list<int, double, float>, char, int>, type_list<char, int, int, double, float>>, "list::prepend: failed");
    static_assert(std::is_same_v<list::prepend<type_list<int, double, float>, int>, type_list<int, int, double, float>>, "list::prepend: failed");
    static_assert(std::is_same_v<list::prepend<type_list<int, double, float>>, type_list<int, double, float>>, "list::prepend: failed");

    static_assert(std::is_same_v<list::append_if<true, type_list<int, double, float>, char, int>, type_list<int, double, float, char, int>>, "list::append_if: failed");
    static_assert(std::is_same_v<list::append_if<true, type_list<int, double, float>, int>, type_list<int, double, float, int>>, "list::append_if: failed");
    static_assert(std::is_same_v<list::append_if<true, type_list<int, double, float>>, type_list<int, double, float>>, "list::append_if: failed");

    static_assert(std::is_same_v<list::prepend_if<true, type_list<int, double, float>, char, int>, type_list<char, int, int, double, float>>, "list::prepend_if: failed");
    static_assert(std::is_same_v<list::prepend_if<true, type_list<int, double, float>, int>, type_list<int, int, double, float>>, "list::prepend_if: failed");
    static_assert(std::is_same_v<list::prepend_if<true, type_list<int, double, float>>, type_list<int, double, float>>, "list::prepend_if: failed");

    static_assert(std::is_same_v<list::append_if<false, type_list<int, double, float>, char, int>, type_list<int, double, float>>, "list::append_if: failed");
    static_assert(std::is_same_v<list::append_if<false, type_list<int, double, float>, int>, type_list<int, double, float>>, "list::append_if: failed");
    static_assert(std::is_same_v<list::append_if<false, type_list<int, double, float>>, type_list<int, double, float>>, "list::append_if: failed");

    static_assert(std::is_same_v<list::prepend_if<false, type_list<int, double, float>, char, int>, type_list<int, double, float>>, "list::prepend_if: failed");
    static_assert(std::is_same_v<list::prepend_if<false, type_list<int, double, float>, int>, type_list<int, double, float>>, "list::prepend_if: failed");
    static_assert(std::is_same_v<list::prepend_if<false, type_list<int, double, float>>, type_list<int, double, float>>, "list::prepend_if: failed");

    static_assert(std::is_same_v<typename list::merge<type_list<>, type_list<>>::as_first, type_list<>>, "list::merge: failed");
    static_assert(std::is_same_v<typename list::merge<type_list<>, type_list<int, double, float>>::as_first, type_list<int, double, float>>, "list::merge: failed");
    static_assert(std::is_same_v<typename list::merge<type_list<int, double>, type_list<int, double, float>>::as_first, type_list<int, double, int, double, float>>, "list::merge: failed");

    static_assert(std::is_same_v<typename list::extract<type_list<int, double, float>>::as<type_list>, type_list<int, double, float>>, "list::extract: failed");
    static_assert(std::is_same_v<typename list::extract<type_list<int, double, float>>::as<std::tuple>, std::tuple<int, double, float>>, "list::extract: failed");
    static_assert(std::is_same_v<typename list::extract<type_list<>>::as<type_list>, type_list<>>, "list::extract: failed");
    static_assert(std::is_same_v<typename list::extract<type_list<>>::as<std::tuple>, std::tuple<>>, "list::extract: failed");

    static_assert(list::size<type_list<>> == 0, "list::size: failed");
    static_assert(list::size<type_list<int>> == 1, "list::size: failed");
    static_assert(list::size<type_list<int, int, int, int, type_list<int, int>>> == 5, "list::size: failed");

    static_assert(list::same_base<type_list<int, int, int, int>, type_list<char, double>> == true, "list::same_base: failed");
    static_assert(list::same_base<type_list<>, type_list<char, double>> == true, "list::same_base: failed");
    static_assert(list::same_base<type_list<>, std::tuple<char, double>> == false, "list::same_base: failed");
    static_assert(list::same_base<std::tuple<>, type_list<char, double>> == false, "list::same_base: failed");

    static_assert(std::is_same_v<list::remove<type_list<int, long, double, char>, 0>, type_list<long, double, char>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<int, long, double, char>, 1>, type_list<int, double, char>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<int, long, double, char>, 3>, type_list<int, long, double>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<int, long, double, char>, 4>, type_list<int, long, double, char>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<int, long, double, char>, 40>, type_list<int, long, double, char>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<>, 0>, type_list<>>, "list::remove: failed");
    static_assert(std::is_same_v<list::remove<type_list<>, 10>, type_list<>>, "list::remove: failed");

    // remove_type tests for remove_type and remove_if (it is a remove_if with std::is_same<> as predicate)
    static_assert(std::is_same_v<list::remove_type<type_list<int, int, int, int, type_list<int, int>>, int>, type_list<type_list<int, int>>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<int, int, int, int>, int>, type_list<>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<int, int, char, int, int>, int>, type_list<char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<char, int, int, int, int>, int>, type_list<char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<int, int, int, int, char>, int>, type_list<char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<char, int, int, int, int, char>, int>, type_list<char, char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<char, int, int, char, int, int, char>, int>, type_list<char, char, char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<char, char, char>, int>, type_list<char, char, char>>, "list::remove_type: failed");
    static_assert(std::is_same_v<list::remove_type<type_list<>, int>, type_list<>>, "list::remove_type: failed");

    static_assert(std::is_same_v<list::filter<type_list<char, int, int, char, int, int, char>, std::is_floating_point>, type_list<>>, "list::filter: failed");
    static_assert(std::is_same_v<list::filter<type_list<char, int, int, double, int, int, char>, std::is_floating_point>, type_list<double>>, "list::filter: failed");
    static_assert(std::is_same_v<list::filter<type_list<float, char, int, int, double, int, int, char>, std::is_floating_point>, type_list<float, double>>, "list::filter: failed");
    static_assert(std::is_same_v<list::filter<type_list<float, char, int, int, double, int, int, char, long double>, std::is_floating_point>, type_list<float, double, long double>>, "list::filter: failed");

    static_assert(std::is_same_v<list::make_unique<type_list<int, int, int, int, char>>, type_list<int, char>>, "list::make_unique: failed");
    static_assert(std::is_same_v<list::make_unique<type_list<char, int, int, int, int, char>>, type_list<char, int>>, "list::make_unique: failed");
    static_assert(std::is_same_v<list::make_unique<type_list<int, int, int, int, int>>, type_list<int>>, "list::make_unique: failed");
    static_assert(std::is_same_v<list::make_unique<type_list<>>, type_list<>>, "list::make_unique: failed");

    static_assert(std::is_same_v<list::flatten<type_list<>>, type_list<>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<int, int, double, int>>, type_list<int, int, double, int>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<type_list<int>, int, type_list<double, int>>>, type_list<int, int, double, int>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<type_list<int>, int, type_list<double, type_list<int>>>>, type_list<int, int, double, type_list<int>>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<type_list<>, int, type_list<double, type_list<int>>>>, type_list<int, double, type_list<int>>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<type_list<>>>, type_list<>>, "list::flatten: failed");
    static_assert(std::is_same_v<list::flatten<type_list<type_list<int>>>, type_list<int>>, "list::flatten: failed");

    static_assert(std::is_same_v<list::flatten_rec<type_list<>>, type_list<>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<int, int, double, int>>, type_list<int, int, double, int>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<type_list<int>, int, type_list<double, int>>>, type_list<int, int, double, int>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<type_list<int>, int, type_list<double, type_list<int>>>>, type_list<int, int, double, int>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<type_list<type_list<type_list<type_list<type_list<type_list<>>>>>>>>, type_list<>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<type_list<type_list<type_list<type_list<type_list<type_list<int>>>>>>>>, type_list<int>>, "list::flatten_rec: failed");
    static_assert(std::is_same_v<list::flatten_rec<type_list<type_list<type_list<int>, int, type_list<double, type_list<int>>>>>, type_list<int, int, double, int>>, "list::flatten_rec: failed");

    static_assert(std::is_same_v<list::reverse<type_list<>>, type_list<>>, "list::reverse: failed");
    static_assert(std::is_same_v<list::reverse<type_list<int>>, type_list<int>>, "list::reverse: failed");
    static_assert(std::is_same_v<list::reverse<type_list<int, int>>, type_list<int, int>>, "list::reverse: failed");
    static_assert(std::is_same_v<list::reverse<type_list<float, int>>, type_list<int, float>>, "list::reverse: failed");
    static_assert(std::is_same_v<list::reverse<type_list<double, float, int>>, type_list<int, float, double>>, "list::reverse: failed");

    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 0, 0>, type_list<>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 0, 1>, type_list<double>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 0, 10>, type_list<double, float, int>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 1, 0>, type_list<>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 1, 1>, type_list<float>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 1, 10>, type_list<float, int>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 2, 0>, type_list<>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 2, 1>, type_list<int>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 2, 10>, type_list<int>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 3, 0>, type_list<>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 3, 1>, type_list<>>, "list::sublist: failed");
    static_assert(std::is_same_v<list::sublist<type_list<double, float, int>, 3, 10>, type_list<>>, "list::sublist: failed");
#endif // NCT_TESTS
  } // namespace ct
} // namespace neam

#endif /*__N_48888409651922371_1596013433__CT_LIST_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

