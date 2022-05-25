//
// created by : Timothée Feuillet
// date: 2021-11-25
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

#include "../hash/fnv1a.hpp"

#include "id.hpp"

#ifndef N_STRIP_DEBUG
  #define N_STRIP_DEBUG false
#endif

namespace neam
{
  /// \brief Convert a C string to a id_t, providing debug information when possible.
  /// \warning Do no rely on the size. Use id_t when the debug information is not necessary.
  /// Resources have the following format:
  ///   [format:/some/path/here.xyz]
  ///
  /// For a spriv shader, it will be [spriv:/engine/imgui/imgui.frag]
  ///   (you expect the format to be spirv, and the source file is /engine/imgui/imgui.frag)
  ///
  /// For a pack file: [pack:/path/to/pack]
  /// For an index file: [index:/path/to/index]
  /// They are a bit special as the path is not the original path but the real path
  ///
  ///
  /// We provide the consteval _rid string litteral for your conveniance: "[spriv:/my/shader.frag]"_rid
  class string_id
  {
    public:
      // Implicit string -> string_id 
      template<size_t Count>
      consteval string_id(const char (&_str)[Count])
       : id((id_t)ct::hash::fnv1a<64, Count>(_str))
#if !N_STRIP_DEBUG
       , str(_str)
       , str_length(Count)
#endif
      {
      }

      template<size_t Count>
      consteval string_id(const ct::string_holder<Count>& _str)
       : id((id_t)ct::hash::fnv1a<64, Count>(_str.string))
#if !N_STRIP_DEBUG
       , str(_str.string)
       , str_length(Count)
#endif
      {
      }

      // Create an indentifier from type-id + res name (in the form of type-id:res-name)
      template<size_t Count>
      consteval string_id(id_t res_id, const char (&_type_id)[Count])
       : id(specialize(res_id, _type_id))
#if !N_STRIP_DEBUG
       // FIXME:
#endif
      {
      }

      consteval string_id() {}
      constexpr string_id(const string_id& o) = default;

      constexpr operator id_t () const
      {
        return id;
      }

#if !N_STRIP_DEBUG
      constexpr const char* get_string() const { return str; }
      constexpr size_t get_string_length() const { return str_length; }
#else
      constexpr const char* get_string() const { return nullptr; }
      constexpr size_t get_string_length() const { return 0; }
#endif

      /// \brief Slowly build from a string at runtime.
      /// \warning Slow. And may leave the string in the binary
      [[nodiscard]] constexpr static string_id _runtime_build_from_string(const char* const _str, const size_t len)
      {
#if !N_STRIP_DEBUG
        return string_id((id_t)ct::hash::fnv1a<64>(_str, len)/*, _str, len*/);
#else
        return string_id((id_t)ct::hash::fnv1a<64>(_str, len));
#endif
      };

      constexpr static string_id _runtime_build_from_string(id_t prev, const char* const _str, const size_t len)
      {
        return string_id((id_t)ct::hash::fnv1a_continue<64>((uint64_t)prev, _str, len));
      };

    private:
      constexpr string_id(id_t rid) : id(rid) {}
#if !N_STRIP_DEBUG
      consteval string_id(id_t rid, const char* _str, size_t _len) : id(rid), str(_str), str_length(_len) {}
#endif

      const id_t id = id_t::none;
#if !N_STRIP_DEBUG
      const char* const str = nullptr;
      const size_t str_length = 0;
#endif
  };
}

consteval neam::string_id operator ""_rid (const char* str, size_t len)
{
  return neam::string_id::_runtime_build_from_string(str, len);
}

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
template <> struct fmt::formatter<neam::string_id>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(neam::string_id sid, FormatContext& ctx)
  {
#if !N_STRIP_DEBUG
    if (sid.get_string())
    {
      return format_to(ctx.out(), "{}({})", (neam::id_t)sid, std::string_view(sid.get_string(), sid.get_string_length()));
    }
#endif

    // fallback to formatting the id:
    return format_to(ctx.out(), "{}", (neam::id_t)sid);
  }
};
#endif
