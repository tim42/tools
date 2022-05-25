//
// created by : Timothée Feuillet
// date: 2021-12-12
//
//
// Copyright (c) 2021 Timothée Feuillet
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

#include <string_view>
#include <charconv>
#include "../logger/logger.hpp"

namespace neam::cmdline::helper
{
  template<typename T>
  struct from_string
  {
    static constexpr bool is_valid_type = false;
  };

  template<>
  struct from_string<std::string_view>
  {
    static constexpr bool is_valid_type = true;
    static std::string_view convert(std::string_view v, bool& /*valid*/) { return v; }
  };

  template<>
  struct from_string<bool>
  {
    static constexpr bool is_valid_type = true;
    static bool convert(std::string_view v, bool& valid)
    {
      if (v == "true" || v == "1")
        return true;
      if (v == "false" || v == "0")
        return false;
      valid = false;
      cr::out().warn("boolean options can either be 'true'/'1' or 'false'/'0' but is instead: {}", v);
      return false;
    }
  };

  template<typename Type>
  struct number_helper
  {
    static constexpr bool is_valid_type = true;
    static Type convert(std::string_view v, bool& valid)
    {
      Type value = 0;
      const auto [ptr, res] = std::from_chars(v.data(), v.data() + v.size(), value);
      if (res == std::errc::invalid_argument)
      {
        cr::out().warn("expecting a number, got: {}", v);
        valid = false;
        return 0;
      }
      else if (res == std::errc::result_out_of_range)
      {
        cr::out().warn("number is out-of-range: {} (underlying type: {})", v, ct::type_name<Type>.str);
        valid = false;
        return 0;
      }

      valid = true;
      return value;
    }
  };

  template<> struct from_string<uint8_t> :  public number_helper<uint8_t> {};
  template<> struct from_string<uint16_t> : public number_helper<uint16_t> {};
  template<> struct from_string<uint32_t> : public number_helper<uint32_t> {};
  template<> struct from_string<uint64_t> : public number_helper<uint64_t> {};
  template<> struct from_string<int8_t> :  public number_helper<int8_t> {};
  template<> struct from_string<int16_t> : public number_helper<int16_t> {};
  template<> struct from_string<int32_t> : public number_helper<int32_t> {};
  template<> struct from_string<int64_t> : public number_helper<int64_t> {};

  template<> struct from_string<float> :  public number_helper<double> {};
  template<> struct from_string<double> : public number_helper<float> {};
}

