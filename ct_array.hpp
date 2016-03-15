//
// file : ct_array.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/ct_array.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 16/07/2014 17:31:04
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

#ifndef __N_14558597131808588598_248435571__CT_ARRAY_HPP__
# define __N_14558597131808588598_248435571__CT_ARRAY_HPP__

#include <cstddef>

namespace neam
{
  namespace ct
  {
    // a simple compile time array
    template<typename Type, Type... Values>
    struct array
    {
      static constexpr size_t size()
      {
        return sizeof...(Values);
      }

      // access
      constexpr operator Type *() const
      {
        return data;
      }

      constexpr operator const Type *() const
      {
        return data;
      }

      constexpr const Type &operator [](size_t pos) const
      {
        return data[pos];
      }

      template<size_t Index>
      static constexpr const Type &at()
      {
        static_assert(Index < sizeof...(Values), "index out of range");
        return data[Index];
      }

      static constexpr const Type &at(size_t index)
      {
        return data[index];
      }

      // iterators
      static constexpr Type *begin()
      {
        return data;
      }
      static constexpr Type *end()
      {
        return data + sizeof...(Values);
      }

      static constexpr Type *rbegin()
      {
        return data + sizeof...(Values) - 1;
      }
      static constexpr Type *rend()
      {
        return data - 1;
      }

      // the CT data
      static constexpr Type data[sizeof...(Values)] = {Values...};
    };

    template<typename Type, Type... Values>
    constexpr Type array<Type, Values...>::data[sizeof...(Values)];
  } // namespace ct
} // namespace neam

#endif /*__N_14558597131808588598_248435571__CT_ARRAY_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

