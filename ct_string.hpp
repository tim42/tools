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
    /// \brief C-String + Size from array
    struct string
    {
      string() = delete;
      ~string() noexcept = default;

      template<size_t N>
      constexpr string(const char (&_str)[N]) noexcept : str(_str), size(N - 1) {}
      constexpr string(const string& o) noexcept : str(o.str), size(o.size) {}
      constexpr string(const char* _str, size_t _size) noexcept : str(_str), size(_size) {}
      template<size_t N>
      constexpr string &operator = (const char (&_str)[N]) noexcept
      {
        str = _str;
        size = N - 1;
        return *this;
      }
      constexpr string &operator = (const string &o) noexcept
      {
        str = (o.str);
        size = (o.size);
        return *this;
      }

      constexpr const char* begin() const noexcept { return str; }
      constexpr const char* end() const noexcept { return str + size; }

      constexpr size_t find(const string& substr) const noexcept
      {
        if (substr.size > size)
          return ~0ul;
        else if (substr.size == size)
          return (substr == *this ? 0 : ~0ul);
        else if (!substr.size)
          return 0;

        for (size_t i = 0; i < size - substr.size; ++i)
        {
          if (str[i] == substr.str[0])
          {
            size_t j = 1;
            for (; j < substr.size && str[i + j] == substr.str[j]; ++j) {}
            if (j == substr.size)
              return i;
          }
        }
        return ~0ul;
      }

      constexpr string pad(size_t begin_offset, size_t end_offset) const noexcept
      {
        return { str + begin_offset, size - end_offset - begin_offset };
      }

      constexpr bool operator == (const string& o) const noexcept
      {
        if (o.size != size)
          return false;
        for (size_t i = 0; i < size; ++i)
        {
          if (o.str[i] != str[i])
            return false;
        }
        return true;
      }

      constexpr bool operator != (const string& o) const noexcept
      {
        return !(*this == o);
      }

      const char* str;
      size_t size;
    };

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
    template<size_t N>
    inline static constexpr size_t strlen(const char (&)[N])
    {
      return N - 1;
    }
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
