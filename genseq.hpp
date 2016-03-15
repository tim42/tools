//
// file : genseq.hpp
// in : file:///home/tim/projects/nkark/src/genseq.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 11/08/2013 15:42:19
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

#ifndef __GENSEQ_HPP__
# define __GENSEQ_HPP__

#include <cstdint>

namespace neam
{
  namespace cr
  {
    // a sequence generator (doesn't work on some mingw :( )
    // this comes from a stackoverflow answer.
    template <size_t... Idx> struct seq{};
    template <size_t Cr, size_t... Idx> struct gen_seq : gen_seq<Cr - 1, Cr - 1, Idx...> {};
    template <size_t... Idx> struct gen_seq<0, Idx...> : seq<Idx...> {};

  } // namespace internal
} // namespace nkark

#endif /*__GENSEQ_HPP__*/
