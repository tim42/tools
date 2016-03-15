//
// file : array.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/array.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 08/01/2014 14:24:10
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


#ifndef __N_1633553069808326696_1809251105__ARRAY_HPP__
# define __N_1633553069808326696_1809251105__ARRAY_HPP__

#include <cstddef>

namespace neam
{
  namespace cr
  {
    template <typename Type, size_t Num>
    struct array
    {
      static constexpr size_t size()
      {
        return Num;
      }

      // access
      operator Type *()
      {
        return data;
      }

      constexpr operator const Type *() const
      {
        return data;
      }

      Type &operator [](size_t pos)
      {
        return data[pos];
      }
      constexpr const Type &operator [](size_t pos) const
      {
        return data[pos];
      }

      // iterators
      constexpr Type *begin() const
      {
        return data;
      }
      constexpr Type *end() const
      {
        return data + Num;
      }
      Type *begin()
      {
        return data;
      }
      Type *end()
      {
        return data + Num;
      }

      // data (the C type)
      Type data[Num];
    };
  } // namespace cr
} // namespace neam

#endif /*__N_1633553069808326696_1809251105__ARRAY_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

