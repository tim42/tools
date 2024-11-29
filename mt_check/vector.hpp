//
// created by : Timothée Feuillet
// date: 2024-3-24
//
//
// Copyright (c) 2024 Timothée Feuillet
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

#pragma once

#include <vector>

#include "mt_check_base.hpp"
#include "../debug/assert.hpp"

namespace std
{
#if !N_ENABLE_MT_CHECK
  template<class T, class Allocator = std::allocator<T>>
  using mtc_vector = vector<T, Allocator>;
#else
  template<class T, class Allocator = std::allocator<T>>
  class mtc_vector : public vector<T, Allocator>, public neam::cr::mt_checked<vector<T, Allocator>>
  {
    private:
      using parent_t = vector<T, Allocator>;

      void check_out_of_bounds(size_t pos) const
      {
        neam::check::debug::n_assert(pos < this->size(), "mtc_deque: out of bounds: reading at position {} of a container of size {}", pos, this->size());
      }
    public:
      using vector<T, Allocator>::vector;
      using vector<T, Allocator>::operator =;

      N_MTC_WRAP_METHOD(reserve, write,)
      N_MTC_WRAP_METHOD(clear, write,)
      N_MTC_WRAP_METHOD(insert, write,)
      N_MTC_WRAP_METHOD(emplace, write,)
      N_MTC_WRAP_METHOD(erase, write,)
      N_MTC_WRAP_METHOD_FIRST_PARAM(push_back, const T&, write,)
      N_MTC_WRAP_METHOD_FIRST_PARAM(push_back, T&&, write,)
      N_MTC_WRAP_METHOD(emplace_back, write,)
      N_MTC_WRAP_METHOD(pop_back, write,)
      N_MTC_WRAP_METHOD(resize, write,)
      N_MTC_WRAP_METHOD(swap, write,)


      N_MTC_WRAP_METHOD_FIRST_PARAM_CHK(operator[], size_t, read,, check_out_of_bounds(x))
      N_MTC_WRAP_METHOD_FIRST_PARAM_CHK(operator[], size_t, read, const, check_out_of_bounds(x))

      N_MTC_WRAP_METHOD_CHK(front, read,, check_out_of_bounds(0))
      N_MTC_WRAP_METHOD_CHK(front, read,const, check_out_of_bounds(0))
      N_MTC_WRAP_METHOD_CHK(back, read,, check_out_of_bounds(0))
      N_MTC_WRAP_METHOD_CHK(back, read,const, check_out_of_bounds(0))

      N_MTC_WRAP_METHOD(begin, read,)
      N_MTC_WRAP_METHOD(begin, read,const)
      N_MTC_WRAP_METHOD(end, read,)
      N_MTC_WRAP_METHOD(end, read,const)
  };
#endif
}
