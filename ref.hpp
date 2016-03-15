//
// file : ref.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/ref.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 16/12/2013 19:59:53
//
//
// Copyright (c) 2013-2016 Timothée Feuillet
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


#ifndef __N_462308859677856537_1340905228__REF_HPP__
# define __N_462308859677856537_1340905228__REF_HPP__
#include <type_traits>

namespace neam
{
  namespace cr
  {
    // a reference struct
    template<typename Type>
    struct ref
    {
      constexpr ref(Type &_value) : value(_value) {}

      operator Type& ()
      {
        return value;
      }
      operator const Type& () const
      {
        return value;
      }

      Type &value;
    };

    // helpers
    template<typename RefType>
    constexpr ref<RefType> make_ref(RefType &value)
    {
      return ref<RefType>(value);
    }
    template<typename RefType>
    constexpr ref<const RefType> make_const_ref(const RefType &value)
    {
      return ref<const RefType>(value);
    }

    // type traits
    template<typename X> struct is_ref : public std::false_type {};
    template<typename T> struct is_ref<ref<T>> : public std::true_type {};

  } // namespace cr
} // namespace neam

#endif /*__N_462308859677856537_1340905228__REF_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

