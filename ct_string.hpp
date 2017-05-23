// cr: a c++ template parser
//
// Copyright (c) 2012-2016 Timothée Feuillet
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
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <memory>
#include <vector>
#include <stdexcept>

#ifndef __CR_STRING_HPP__
#define __CR_STRING_HPP__

#define _SL_STRINGIZE(x) #x
#define STRINGIZE(x) _SL_STRINGIZE(x)

namespace neam
{
  // use this as 'constexpr string_t myvar = "blablabla";'
  using string_t = char [];

  namespace ct
  {
    namespace internal
    {
      // strlen function
      inline static constexpr size_t strlen(const char *const str)
      {
        return str[0] ? internal::strlen(str + 1) + 1 : 0;
      }
    } // namespace internal

    static constexpr size_t npos = static_cast<size_t>(-1);

    // strlen function
    inline static constexpr size_t strlen(const char *const str)
    {
      return internal::strlen(str);
    }

    inline static constexpr size_t strlen(std::nullptr_t)
    {
      return 0;
    }

    // a safe strlen function
    inline static constexpr size_t safe_strlen(const char *const str)
    {
      return str ? internal::strlen(str) : 0; // GCC 5.2.1 HATE this on constexpr string_t...
    }
  } // namespace ct
} // namespace neam

#endif /*__CR_STRING_HPP__*/
