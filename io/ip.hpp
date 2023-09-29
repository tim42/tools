//
// created by : Timothée Feuillet
// date: 2023-9-21
//
//
// Copyright (c) 2023 Timothée Feuillet
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
namespace neam::io
{
  struct ipv6
  {
    uint8_t addr[16];


    static constexpr ipv6 from_ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
    {
      return { 0,0, 0,0, 0,0, 0,0, 0,0, 0xFF,0xFF, a,b, c,d };
    }

    static constexpr ipv6 from_ipv4(uint32_t ipv4)
    {
      return from_ipv4((ipv4 >> 24) & 0xFF, (ipv4 >> 16) & 0xFF, (ipv4 >> 8) & 0xFF, ipv4 & 0xFF);
    }

    static constexpr ipv6 any()
    {
      return { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 };
    }
    static constexpr ipv6 localhost()
    {
      return { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,1 };
    }
  };
}

