//
// created by : Timothée Feuillet
// date: 2022-5-20
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

#include <utility> // for to_underlying

#define N_ENUM_FLAG(Enum)  \
constexpr static Enum operator~ (Enum a) { return static_cast<Enum>(~std::to_underlying(a)); } \
constexpr static Enum operator| (Enum a, Enum b) { return static_cast<Enum>(std::to_underlying(a) | std::to_underlying(b)); } \
constexpr static Enum operator& (Enum a, Enum b) { return static_cast<Enum>(std::to_underlying(a) & std::to_underlying(b)); } \
constexpr static Enum operator^ (Enum a, Enum b) { return static_cast<Enum>(std::to_underlying(a) ^ std::to_underlying(b)); } \
constexpr static Enum& operator|= (Enum& a, Enum b) { a = static_cast<Enum>(std::to_underlying(a) | std::to_underlying(b)); return a; } \
constexpr static Enum& operator&= (Enum& a, Enum b) { a = static_cast<Enum>(std::to_underlying(a) & std::to_underlying(b)); return a; } \
constexpr static Enum& operator^= (Enum& a, Enum b) { a = static_cast<Enum>(std::to_underlying(a) ^ std::to_underlying(b)); return a; } \
constexpr static bool has_flag(Enum a, Enum flag) { return (a & flag) == flag; }

