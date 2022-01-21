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
}
