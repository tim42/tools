//
// file : integer_tools.hpp
// in : file:///home/tim/projects/ntools/integer_tools.hpp
//
// created by : Timothée Feuillet
// date: lun. juil. 2 18:00:53 2018 GMT-0400
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

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace neam
{
  namespace ct
  {
    template<size_t Size, bool Signed = true>
    [[maybe_unused]] static constexpr void* integer_v = nullptr;

    template<> [[maybe_unused]] constexpr int8_t integer_v<1, true> = 0;
    template<> [[maybe_unused]] constexpr int16_t integer_v<2, true> = 0;
    template<> [[maybe_unused]] constexpr int32_t integer_v<4, true> = 0;
    template<> [[maybe_unused]] constexpr int64_t integer_v<8, true> = 0;
    template<> [[maybe_unused]] constexpr __int128 integer_v<16, true> = 0;
    // template<> [[maybe_unused]] constexpr _BitInt(256) integer_v<32, true> = 0;

    template<> [[maybe_unused]] constexpr uint8_t integer_v<1, false> = 0;
    template<> [[maybe_unused]] constexpr uint16_t integer_v<2, false> = 0;
    template<> [[maybe_unused]] constexpr uint32_t integer_v<4, false> = 0;
    template<> [[maybe_unused]] constexpr uint64_t integer_v<8, false> = 0;
    template<> [[maybe_unused]] constexpr unsigned __int128 integer_v<16, false> = 0;
    // template<> [[maybe_unused]] constexpr unsigned _BitInt(256) integer_v<32, false> = 0;

    template<size_t Size>
    using sint_t = std::remove_cv_t<decltype(integer_v<Size, true>)>;
    template<size_t Size>
    using uint_t = std::remove_cv_t<decltype(integer_v<Size, false>)>;
  } // namespace ct
} // namespace neam
