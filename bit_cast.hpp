//
// file : bit_cast.hpp
// in : file:///home/tim/projects/ntools/bit_cast.hpp
//
// created by : Timothée Feuillet
// date: dim. mai 6 21:37:34 2018 GMT-0400
//
//
// Copyright (c) 2018 Timothée Feuillet
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

#include <cstring>

namespace neam::cr
{
  /// \brief Perform a binary cast operation.
  /// Sadly, this cannot be in `ct` yet :(
  /// gcc and clang will emit on single instruction (a mov*) when casting floating points to/from integers
  /// msvc will copy the value from xmm register to the stack then copy back that value into rax.
  template<typename DestType, typename SrcType>
  static DestType bit_cast(SrcType v)
  {
    static_assert(std::is_trivially_copyable_v<DestType> && std::is_trivially_copyable_v<SrcType>, "the source and the destination types should be trivially copyabe");
    static_assert(sizeof(DestType) == sizeof(SrcType), "the source and the destination types should have the same size");
    DestType x = DestType{};
    memcpy(&x, &v, sizeof(DestType));
    return x;
  }
} // namespace neam::ct
