//
// created by : Timothée Feuillet
// date: 2022-1-30
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

// Add support for standardized fmt output for ANY struct/class that can be serialized
// (it cannot be a container, only a struct/class with metadata)
//
// To serialize any other type, call neam::type_to_string(your value here).
// The helper in this file is simply a short-hand for serializable structs


#if __has_include(<fmt/format.h>)
#include "type_to_string.hpp"
#include <fmt/format.h>
template<neam::metadata::concepts::StructWithMetadata Struct> struct fmt::formatter<Struct>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const Struct& v, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(), "{}", neam::metadata::type_to_string<Struct>(v));
  }
};

#endif

