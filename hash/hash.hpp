//
// created by : Timothée Feuillet
// date: 2022-7-10
//
//
// Copyright (c) 2022 Timothée Feuillet
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
#include <utility>

namespace neam::ct
{
  static constexpr uint64_t combine(uint64_t a, uint64_t b)
  {
    return (a ^ (b + 0x9e3779b97f4a7c15 + (a << 6) + (a >> 2)));
  }
  static constexpr uint32_t combine(uint32_t a, uint32_t b)
  {
    return (a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2)));
  }

  static constexpr uint32_t fold32(uint64_t id)
  {
    return uint32_t(((id >> 32) ^ id) & 0xFFFFFFFF);
  }
  static constexpr uint32_t fold31(uint64_t id)
  {
    return uint32_t(((id >> 32) ^ id) & 0x7FFFFFFF);
  }

  static constexpr uint64_t murmur_scramble(uint64_t h)
  {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
  }
  static constexpr uint32_t murmur_scramble(uint32_t h)
  {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
  }
}

