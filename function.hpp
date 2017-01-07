//
// file : function.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/function.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 17/07/2014 11:59:09
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

// ATTENTION NOT FINISHED YET !!!

#ifndef __N_2111737739159269973_1204098121__FUNCTION_HPP__
# define __N_2111737739159269973_1204098121__FUNCTION_HPP__

#include "ct_list.hpp"
#include "execute_pack.hpp" // some magic
#include "constructor_call.hpp"
#include "param.hpp"

namespace neam
{
  namespace ct
  {
    template<typename Ptr>
    struct is_function_pointer
    {
      static constexpr bool value = false;
    };
    template<typename Ret, typename... Params>
    struct is_function_pointer<Ret(*)(Params...)>
    {
      static constexpr bool value = true;
    };
    template<typename Ret, typename Class, typename... Params>
    struct is_function_pointer<Ret(Class::*)(Params...)>
    {
      static constexpr bool value = true;
    };
    template<typename Ret, typename... Params>
    struct is_function_pointer<Ret(&)(Params...)>
    {
      static constexpr bool value = true;
    };

    // /// ///
    // for a ct function wrapper
    // /// ///

    // the wrappers
    template<typename PtrType, PtrType Ptr, typename... Args> struct function_wrapper {};      // the one that wrap
    template<typename PtrType, PtrType Ptr, typename... Args> struct _sub_function_wrapper {}; // the result one

    // for C functions
    template<typename PtrType, PtrType Ptr, typename Return, typename... FuncArgList>
    struct function_wrapper<PtrType, Ptr, Return, type_list<FuncArgList...>>
    {
      template<typename... Types>
      static constexpr _sub_function_wrapper<PtrType, Ptr, Return, type_list<FuncArgList...>, type_list<Types...>> wrap(Types... values)
      {
        return _sub_function_wrapper<PtrType, Ptr, Return, type_list<FuncArgList...>, type_list<Types...>>(values...);
      }
    };

    template<typename PtrType, PtrType Ptr, typename Return, typename... FuncArgList, typename... ArgList>
    struct _sub_function_wrapper<PtrType, Ptr, Return, type_list<FuncArgList...>, type_list<ArgList...>>
    {
      static_assert(sizeof...(FuncArgList) == sizeof...(ArgList), "the number of supplied argument does not match the function's arity");

//       constexpr _sub_function_wrapper(ArgList... values)
//       
//       {
//       }
    };

    // /// ///
    // function traits
    // /// ///

    template<typename FunctionType>
    struct function_traits : public function_traits<decltype(&FunctionType::operator())>
    {
//       static constexpr bool is_function = false;
// #ifdef IN_IDE_PARSER
//       static constexpr bool is_member = false;
//       using return_type = void;
//       using ptr_type = void; // not a pointer
//       using arg_list = type_list<>;
// #endif
    };

//     template<typename FunctionType>
//     constexpr bool function_traits<FunctionType>::is_function;

    // standard function
    template<typename Return, typename... Parameters>
    struct function_traits<Return(*)(Parameters...)>
    {
      static constexpr bool is_function = true;
      static constexpr bool is_member = false;
      using return_type = Return;
      using ptr_type = Return (*)(Parameters...);
      using arg_list = type_list<Parameters...>;

      template<ptr_type ptr>
      using create_wrapper = function_wrapper<ptr_type, ptr, Return, /*Class,*/ arg_list>;
    };
    template<typename Return, typename... Parameters>
    constexpr bool function_traits<Return(*)(Parameters...)>::is_function;
    template<typename Return, typename... Parameters>
    constexpr bool function_traits<Return(*)(Parameters...)>::is_member;

    // class member function
    template<typename Return, typename Class,typename... Parameters>
    struct function_traits<Return (Class::*)(Parameters...)>
    {
      static constexpr bool is_function = true;
      static constexpr bool is_member = true;
      static constexpr bool is_const = false;
      static constexpr bool is_volatile = false;
      using return_type = Return;
      using class_type = Class;
      using ptr_type = Return (Class::*)(Parameters...);
      using arg_list = type_list<Parameters...>;
    };
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...)>::is_function;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...)>::is_member;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...)>::is_const;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...)>::is_volatile;

    template<typename Return, typename Class, typename... Parameters>
    struct function_traits<Return (Class::*)(Parameters...) const>
    {
      static constexpr bool is_function = true;
      static constexpr bool is_member = true;
      static constexpr bool is_const = true;
      static constexpr bool is_volatile = false;
      using return_type = Return;
      using class_type = Class;
      using ptr_type = Return (Class::*)(Parameters...) const;
      using arg_list = type_list<Parameters...>;
    };
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const>::is_function;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const>::is_member;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const>::is_const;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const>::is_volatile;

    template<typename Return, typename Class, typename... Parameters>
    struct function_traits<Return (Class::*)(Parameters...) volatile>
    {
      static constexpr bool is_function = true;
      static constexpr bool is_member = true;
      static constexpr bool is_const = false;
      static constexpr bool is_volatile = true;
      using return_type = Return;
      using class_type = Class;
      using ptr_type = Return (Class::*)(Parameters...) volatile;
      using arg_list = type_list<Parameters...>;
    };
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) volatile>::is_function;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) volatile>::is_member;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) volatile>::is_const;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) volatile>::is_volatile;

    template<typename Return, typename Class, typename... Parameters>
    struct function_traits<Return (Class::*)(Parameters...) const volatile>
    {
      static constexpr bool is_function = true;
      static constexpr bool is_member = true;
      static constexpr bool is_const = false;
      static constexpr bool is_volatile = true;
      using return_type = Return;
      using class_type = Class;
      using ptr_type = Return (Class::*)(Parameters...) const volatile;
      using arg_list = type_list<Parameters...>;
    };
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const volatile>::is_function;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const volatile>::is_member;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const volatile>::is_const;
    template<typename Return, typename Class, typename... Parameters>
    constexpr bool function_traits<Return (Class::*)(Parameters...) const volatile>::is_volatile;


    

    
  } // namespace ct
} // namespace neam

#endif /*__N_2111737739159269973_1204098121__FUNCTION_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

