//
// file : merge_pack.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/merge_pack.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 28/01/2014 13:16:17
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

#ifndef __N_894093274638761124_1022288400__MERGE_PACK_HPP__
# define __N_894093274638761124_1022288400__MERGE_PACK_HPP__

#include <tools/ct_list.hpp>
#include <tools/tuple.hpp>

// merge two type_list / merger / tuple in one type_list / tuple

namespace neam
{
  namespace ct
  {
    // append or prepend a type to a tuple
    template <typename Cr, typename Type> struct prepend_type {};
    template <typename Cr, typename Type> struct append_type {};

    template <typename Cr, typename ...Others>
    struct prepend_type<Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Cr, Others...>;
    };
    template <typename Cr, typename ...Others>
    struct prepend_type<Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Cr, Others...>;
    };
    template <typename Cr, typename ...Others>
    struct append_type<Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Others..., Cr>;
    };
    template <typename Cr, typename ...Others>
    struct append_type<Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Others..., Cr>;
    };

    template <bool Cond, typename Cr, typename Type> struct prepend_type_if {};
    template <bool Cond, typename Cr, typename Type> struct append_type_if {};

    template <typename Cr, typename ...Others>
    struct prepend_type_if<true, Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Cr, Others...>;
    };
    template <typename Cr, typename ...Others>
    struct prepend_type_if<false, Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Others...>;
    };
    template <typename Cr, typename ...Others>
    struct prepend_type_if<true, Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Cr, Others...>;
    };
    template <typename Cr, typename ...Others>
    struct prepend_type_if<false, Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Others...>;
    };
    template <typename Cr, typename ...Others>
    struct append_type_if<true, Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Others..., Cr>;
    };
    template <typename Cr, typename ...Others>
    struct append_type_if<false, Cr, cr::tuple<Others...>>
    {
      using type = cr::tuple<Others...>;
    };
    template <typename Cr, typename ...Others>
    struct append_type_if<true, Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Others..., Cr>;
    };
    template <typename Cr, typename ...Others>
    struct append_type_if<false, Cr, ct::type_list<Others...>>
    {
      using type = ct::type_list<Others...>;
    };

    // the merger
    template<typename Type1, typename Type2>
    struct merger {};

    // cr::tuple/cr::tuple case
    template<typename... Types1, typename... Types2>
    struct merger<cr::tuple<Types1...>, cr::tuple<Types2...>>
    {
      using tuple = cr::tuple<Types1..., Types2...>;
      using type_list = ct::type_list<Types1..., Types2...>;
    };

    // ct::type_list/ct::type_list
    template<typename... Types1, typename... Types2>
    struct merger<ct::type_list<Types1...>, ct::type_list<Types2...>>
    {
      using tuple = cr::tuple<Types1..., Types2...>;
      using type_list = ct::type_list<Types1..., Types2...>;
    };

    // ct::merger/ct::merger
    template<typename... Types1, typename... Types2>
    struct merger<ct::merger<Types1...>, ct::merger<Types2...>>
    {
      using tuple = typename ct::merger<typename ct::merger<Types1...>::type_list, typename ct::merger<Types2...>::type_list>::tuple;
      using type_list = typename ct::merger<typename ct::merger<Types1...>::type_list, typename ct::merger<Types2...>::type_list>::type_list;
    };

    // cr::tuple/ct::type_list case
    template<typename... Types1, typename... Types2>
    struct merger<cr::tuple<Types1...>, ct::type_list<Types2...>>
    {
      using tuple = cr::tuple<Types1..., Types2...>;
      using type_list = ct::type_list<Types1..., Types2...>;
    };

    // ct::type_list/cr::tuple case
    template<typename... Types1, typename... Types2>
    struct merger<ct::type_list<Types1...>, cr::tuple<Types2...>>
    {
      using tuple = cr::tuple<Types1..., Types2...>;
      using type_list = ct::type_list<Types1..., Types2...>;
    };

    // ct::merger/ct::type_list
    template<typename... Types1, typename... Types2>
    struct merger<ct::merger<Types1...>, ct::type_list<Types2...>>
    {
      using tuple = typename ct::merger<typename ct::merger<Types1...>::type_list, ct::type_list<Types2...>>::tuple;
      using type_list = typename ct::merger<typename ct::merger<Types1...>::type_list, ct::type_list<Types2...>>::type_list;
    };

    // ct::merger/cr::tuple
    template<typename... Types1, typename... Types2>
    struct merger<ct::merger<Types1...>, cr::tuple<Types2...>>
    {
      using tuple = typename ct::merger<typename ct::merger<Types1...>::type_list, ct::type_list<Types2...>>::tuple;
      using type_list = typename ct::merger<typename ct::merger<Types1...>::type_list, ct::type_list<Types2...>>::type_list;
    };

    // ct::type_list/ct::merger
    template<typename... Types1, typename... Types2>
    struct merger<ct::type_list<Types1...>, ct::merger<Types2...>>
    {
      using tuple = typename ct::merger<ct::type_list<Types1...>, typename ct::merger<Types2...>::type_list>::tuple;
      using type_list = typename ct::merger<ct::type_list<Types1...>, typename ct::merger<Types2...>::type_list>::type_list;
    };

    // cr::tuple/ct::merger
    template<typename... Types1, typename... Types2>
    struct merger<cr::tuple<Types1...>, ct::merger<Types2...>>
    {
      using tuple = typename ct::merger<ct::type_list<Types1...>, typename ct::merger<Types2...>::type_list>::tuple;
      using type_list = typename ct::merger<ct::type_list<Types1...>, typename ct::merger<Types2...>::type_list>::type_list;
    };

  } // namespace ct
} // namespace neam

#endif /*__N_894093274638761124_1022288400__MERGE_PACK_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

