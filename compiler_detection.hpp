//
// file : compiler_detection.hpp
// in : file:///home/tim/projects/ntools/compiler_detection.hpp
//
// created by : Timothée Feuillet
// date: sam. mai 5 21:50:18 2018 GMT-0400
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

namespace neam
{
  /// \brief Set of the most used compilers out there.
  enum class compiler_t
  {
    unknown = 0,

    clang,
    gcc,
    msvc,
    edg,
    icc,
  };

  /// \brief The active compiler for the current translation unit
  static constexpr compiler_t compiler =
#if defined(__clang__)
          compiler_t::clang;
#define NEAM_ACTIVE_COMPILER CLANG
#define NEAM_COMPILER_CLANG
#elif defined(__INTEL_COMPILER)
          compiler_t::icc;
#define NEAM_ACTIVE_COMPILER ICC
#define NEAM_COMPILER_ICC
#elif defined(__GNUC__)
          compiler_t::gcc;
#define NEAM_ACTIVE_COMPILER GCC
#define NEAM_COMPILER_GCC
#elif defined(__EDG__)
          compiler_t::edg;
#define NEAM_ACTIVE_COMPILER EDG
#define NEAM_COMPILER_EDG
#elif defined(_MSC_VER)
          compiler_t::msvc;
#define NEAM_ACTIVE_COMPILER MSVC
#define NEAM_COMPILER_MSVC
#else
          compiler_t::unknown;
#define NEAM_ACTIVE_COMPILER UNKNOWN
#define NEAM_COMPILER_UNKNOWN
#endif
} // namespace neam
