//
// file : rng.hpp
// in : file:///home/tim/projects/ntools/rng.hpp
//
// created by : Timothée Feuillet
// date: dim. mai 6 21:32:25 2018 GMT-0400
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

#include <cstdint>
#include <type_traits>

namespace neam::ct
{
  /// \brief Some kind of magic PRGN, absolutely lite-weight
  /// (as long a integer (self-)multiplication is lite).
  /// Its "entropy" for least significant bit is quite bad, and particularly below the 16th bit.
  /// Because of that, you should consider shifting instead of and-ing in order to get a correct random-ish number
  template<typename IntegerType>
  static constexpr IntegerType invwk_rng(IntegerType seed)
  {
    static_assert(std::is_unsigned_v<IntegerType>);
    return seed + ((seed * seed) | IntegerType(5));
  }

  
} // namespace neam::ct
