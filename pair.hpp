//
// file : pair.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/pair.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 11/02/2014 08:57:23
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

#ifndef __N_10163000121490696598_1767804787__PAIR_HPP__
# define __N_10163000121490696598_1767804787__PAIR_HPP__

namespace neam
{
  namespace ct
  {
    // a pair of types
    template<typename Type1, typename Type2>
    struct pair
    {
      using type_1 = Type1;
      using first = Type1;
      using type_2 = Type2;
      using second = Type2;
    };
  } // namespace ct
} // namespace neam

#endif /*__N_10163000121490696598_1767804787__PAIR_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

