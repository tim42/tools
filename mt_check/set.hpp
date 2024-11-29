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

#include <set>

#include "mt_check_base.hpp"

namespace std
{
#if !N_ENABLE_MT_CHECK
  template<class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
  using mtc_set = set<Key, Compare, Allocator>;
#else
  template<class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
  class mtc_set : public set<Key, Compare, Allocator>, public neam::cr::mt_checked<set<Key, Compare, Allocator>>
  {
    private:
      using parent_t = set<Key, Compare, Allocator>;

    public:
      using set<Key, Compare, Allocator>::set;
      using set<Key, Compare, Allocator>::operator =;

      N_MTC_WRAP_METHOD(clear, write,)
      N_MTC_WRAP_METHOD(insert, write,)
      // N_MTC_WRAP_METHOD(insert_range, write,)
      N_MTC_WRAP_METHOD(emplace, write,)
      N_MTC_WRAP_METHOD(emplace_hint, write,)
      N_MTC_WRAP_METHOD(erase, write,)
      N_MTC_WRAP_METHOD(swap, write,)


      N_MTC_WRAP_METHOD(find, read,)
      N_MTC_WRAP_METHOD(find, read, const)
      N_MTC_WRAP_METHOD(contains, read, const)
  };
#endif
}
