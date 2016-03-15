//
// file : bad_type.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/bad_type.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 27/01/2014 22:07:37
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

#ifndef __N_2094796812152749172_1184666918__BAD_TYPE_HPP__
# define __N_2094796812152749172_1184666918__BAD_TYPE_HPP__

namespace neam
{
  namespace cr
  {
    class bad_type
    {
      public:
        class construct_from_return {};

        bad_type() = delete;
        bad_type(construct_from_return) {}
        bad_type(bad_type &&) {}
        bad_type(const bad_type &) = delete;
    };
  } // namespace cr
} // namespace neam

#endif /*__N_2094796812152749172_1184666918__BAD_TYPE_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

