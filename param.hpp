//
// file : param.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/param.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 24/07/2014 20:32:28
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

#ifndef __N_3448725641047881181_206828962__PARAM_HPP__
# define __N_3448725641047881181_206828962__PARAM_HPP__

namespace neam
{
  namespace ct
  {
    template<size_t Index> struct param
    {
      constexpr param() {}
      static constexpr size_t index = Index;
    };
    template<size_t Index> constexpr size_t param<Index>::index;

    // stored param ref
    template<size_t Index, typename Type> struct _sub_param : param<Index>
    {
      Type value;
    };

    // reference to _sub_param
    template<size_t Index, typename Type> struct _ref_sub_param : param<Index>
    {
      _sub_param<Index, Type> &value;
    };

    // a parameter range (used in partial)
    // (suppling only the first parameter mean 'to infinity')
    template<size_t Start, size_t Number = 0xFFFFFFFF>
    struct param_range
    {
      static_assert(Number > 0, "invalid null number of arguments in param_range");

      static constexpr size_t index = Start;
      static constexpr size_t size = Number;
    };

  } // namespace ct
} // namespace neam

#endif /*__N_3448725641047881181_206828962__PARAM_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

