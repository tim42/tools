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
#pragma once

#include <initializer_list>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <stdexcept>


#define _SL_STRINGIZE(x) #x
#define STRINGIZE(x) _SL_STRINGIZE(x)

namespace neam
{
  // use this as 'constexpr string_t myvar = "blablabla";'
  using string_t = char [];

  namespace ct
  {
    inline static constexpr size_t strlen(const char* const str);

    /// \brief Allows string litterals in templates
    template<size_t Count>
    struct string_holder
    {
      consteval string_holder(const char (&_string)[Count])
      {
        std::copy_n(_string, Count, string);
      }
      consteval static string_holder from_c_string(const char* _string)
      {
        string_holder ret;
        size_t len = ct::strlen(_string);
        std::copy_n(_string, len < Count ? len : Count, ret.string);
        return ret;
      }
      consteval string_holder(const string_holder&) = default;
      consteval string_holder() = default;

      char string[Count == 0 ? 1 : Count] = {0};
    };

    /// \brief C-String + Size from array
    struct string
    {
      static constexpr string_t empty_string = "";
      consteval string() : string(empty_string) {}
      ~string() noexcept = default;

      template<size_t N>
      consteval string(const char (&_str)[N]) noexcept : str(_str), size(N - 1) {}
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

      constexpr bool starts_with(const string& prefix) const
      {
        return view().starts_with(prefix.view());
      }
      constexpr bool ends_with(const string& prefix) const
      {
        return view().ends_with(prefix.view());
      }
      constexpr bool contains(const string& prefix) const
      {
        return view().contains(prefix.view());
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

      constexpr std::string_view view() const { return {str, size}; }

      const char* str;
      size_t size;
    };

    /// \brief Acts as a valid way to store and reference strings inside template arguments:
    /// usage: ct::string_storage<"my c string"> or c_string_t<"my c string">
    template<string_holder h>
    struct string_storage_t
    {
      static constexpr string_holder holder {h};
      static constexpr const char (&string)[sizeof(holder.string)] { holder.string };
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

/// \brief static storage for a C string litteral. Is a C array. (and thus can be used where size is to be deduced)
template<neam::ct::string_holder h>
constexpr const char (&c_string_t)[sizeof(h.string)] = neam::ct::string_storage_t<h>::string;

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
template<> struct fmt::formatter<neam::ct::string>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const neam::ct::string& v, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(), "{:{}}", v.str, v.size);
  }
};

#endif
