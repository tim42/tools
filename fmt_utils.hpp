//
// created by : Timothée Feuillet
// date: 2023-3-25
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


#include <fmt/format.h>
#include "type_id.hpp"

#include <filesystem>
template<> struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const std::filesystem::path& v, FormatContext& ctx) const
  {
    return fmt::formatter<std::string>::format((std::string)v, ctx);
  }
};

#include <tuple>
template<typename... Args> struct fmt::formatter<std::tuple<Args...>>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template<typename FormatContext>
  auto format(const std::tuple<Args...>& args, FormatContext& ctx) const
  {
    fmt::format_to(ctx.out(), "{{");
    return format_it<FormatContext, 0>(args, ctx);
  }

  template<typename FormatContext, size_t Idx>
  auto format_it(const std::tuple<Args...>& args, FormatContext& ctx) const
  {
    if constexpr (Idx >= sizeof...(Args))
      return fmt::format_to(ctx.out(), "}}");
    else
    {
      using arg_t = std::remove_cvref_t<decltype(std::get<Idx>(args))>;
      if constexpr (std::is_pointer_v<arg_t>)
      {
        if constexpr (fmt::has_formatter<std::remove_pointer_t<arg_t>, FormatContext>::value && std::is_object_v<std::remove_pointer_t<arg_t>>)
          fmt::format_to(ctx.out(), "{}{}", *std::get<Idx>(args), Idx + 1 == sizeof...(Args) ? "" : ", ");
        else
          fmt::format_to(ctx.out(), "({}){}{}", neam::ct::type_name<arg_t>.str, (void*)std::get<Idx>(args), Idx + 1 == sizeof...(Args) ? "" : ", ");
      }
      else if constexpr (std::is_enum_v<arg_t>)
      {
          fmt::format_to(ctx.out(), "({}){}{}", neam::ct::type_name<arg_t>.str, std::to_underlying(std::get<Idx>(args)), Idx + 1 == sizeof...(Args) ? "" : ", ");
      }
      else
      {
        fmt::format_to(ctx.out(), "{}{}", std::get<Idx>(args), Idx + 1 == sizeof...(Args) ? "" : ", ");
      }
      return format_it<FormatContext, Idx + 1>(args, ctx);
    }
  }
};


