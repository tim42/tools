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

// Add support for standardized fmt output

#include "struct_metadata.hpp"
#include "../type_id.hpp"
#include "../ct_list.hpp"

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
template<neam::metadata::concepts::StructWithMetadata Struct> struct fmt::formatter<Struct>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const Struct& v, FormatContext& ctx)
  {
    using namespace neam;

    format_to(ctx.out(), "{{ ");
    [&v, &ctx]<size_t... Indices>(std::index_sequence<Indices...>)
    {
      ([&v, &ctx]<size_t Index>()
      {
        using member = ct::list::get_type<n_metadata_member_list<Struct>, Index>;
        using member_type = typename member::type;

        format_to(ctx.out(), " {}  = {},", /*ct::type_name<member_type>.str, */member::name.string, member_at<member_type, member::offset>(v));
      } .template operator()<Indices>(), ...);
    }(std::make_index_sequence<ct::list::size<n_metadata_member_list<Struct>>> {});
    return format_to(ctx.out(), " }}");
  }

  template<typename MT, size_t Offset>
  static MT& member_at(Struct& v)
  {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&v);
    return *reinterpret_cast<MT*>(ptr + Offset);
  }
  template<typename MT, size_t Offset>
  static const MT& member_at(const Struct& v)
  {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
    return *reinterpret_cast<const MT*>(ptr + Offset);
  }
};

#endif

