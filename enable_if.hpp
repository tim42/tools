//
// file : enable_if
// in : file:///home/tim/projects/yaggler/yaggler/tools/enable_if
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 18/10/2013 00:19:15
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

#ifndef __N_68570312629476691_194362390__ENABLE_IF__
# define __N_68570312629476691_194362390__ENABLE_IF__

namespace neam
{
  namespace cr
  {
    // an enable if (as the one packed with the gnu libc++ doesn't work in my case -- g++ 4.7 & 4.8 on linux --)
    // it cause an error where the sfinae isn't respected and g++ try to get the ::type when Cond is false.
    // this is a simple workaround that work with argument type instead of the sfinae
    // there's a new template parameter: Line (usually setted to __LINE__) to create a different type when false (WARNING: this is an **UGLY** solution)
    template <bool Cond, size_t Line, typename Type> struct enable_if
    {
      using type = struct _
      {
        _() = delete;
        ~_() = delete;
      };
    };
    template <typename Type, size_t Line> struct enable_if<true, Line, Type>
    {
      using type = Type;
    };

// and an easy macro to use it :)
// NOTE: DO NOT FORGET BRACKETS AROUND 'Cond' ;)
#define NCR_ENABLE_IF(Cond, Type)   typename neam::cr::enable_if<(Cond), __LINE__ + __COUNTER__, Type>::type

  } // namespace cr

} // namespace neam

#endif /*__N_68570312629476691_194362390__ENABLE_IF__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

