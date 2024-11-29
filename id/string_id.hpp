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
#include "../ct_string.hpp"

#include "id.hpp"

#ifndef N_STRIP_DEBUG
  #define N_STRIP_DEBUG false
#endif

namespace neam
{
#if !N_STRIP_DEBUG
  namespace internal::id::debug
  {
    /// \brief Register a string for id, check for potential conflicts, returns a string that can be safely stored in a string_id
    std::string_view register_string(id_t id, std::string_view view);
    /// \brief Returns the string view associated for a given id
    std::string_view get_string_for_id(id_t id);
  }
#endif

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
      // Implicit string -> string_id.
      // WARNING: The string MUST be a globally availlable object (with linkage)
      template<size_t Count>
      consteval string_id(const char (&_str)[Count])
       : id((id_t)ct::hash::fnv1a<64, Count>(_str))
#if !N_STRIP_DEBUG
       , str(_str)
       , str_length(Count - 1)
#endif
      {
      }

      template<size_t Count>
      consteval string_id(const ct::string_holder<Count>& _str)
       : id((id_t)ct::hash::fnv1a<64, Count + 1>(_str.str))
#if !N_STRIP_DEBUG
       , str(_str.str)
       , str_length(Count)
#endif
      {
      }

      consteval string_id(const ct::string& _str)
       : id((id_t)ct::hash::fnv1a<64>(_str.str, _str.size))
#if !N_STRIP_DEBUG
       , str(_str.str)
       , str_length(_str.size)
#endif
      {
      }

      consteval string_id(const char* str, size_t len)
       : id((id_t)ct::hash::fnv1a<64>(str, len))
#if !N_STRIP_DEBUG
       , str(str)
       , str_length(len)
#endif
      {
      }

      // Create an indentifier from type-id + res name (in the form of type-id:res-name)
      template<size_t Count>
      consteval string_id(id_t res_id, const char (&_type_id)[Count])
       : id(specialize(res_id, _type_id))
#if !N_STRIP_DEBUG
       // FIXME: if that's even possible...
#endif
      {
      }

#if !N_STRIP_DEBUG
      constexpr ~string_id()
      {
        if !consteval
        {
          str = internal::id::debug::register_string(id, {str, str_length}).data();
        }
      }
#endif

#if !N_STRIP_DEBUG
      constexpr string_id() = default;
#else
      consteval string_id() = default;
#endif

      constexpr string_id(const string_id& o) = default;

      constexpr operator id_t () const
      {
        return id;
      }

      constexpr auto operator <=> (const string_id& o) const { return id <=> o.id; }

#if !N_STRIP_DEBUG
      constexpr const char* get_string() const { return str; }
      constexpr size_t get_string_length() const { return str_length; }
      constexpr std::string_view get_string_view() const { return {str, str_length}; }
#else
      constexpr const char* get_string() const { return nullptr; }
      constexpr size_t get_string_length() const { return 0; }
      constexpr std::string_view get_string_view() const { return {nullptr, 0}; }
#endif
#if !N_STRIP_DEBUG
#define N_STRING_ID_RT_CONSTEXPR
#else
#define N_STRING_ID_RT_CONSTEXPR constexpr
#endif

      /// \brief Slowly build from a string at runtime.
      /// \warning Slow. And may leave the string in the binary
      [[nodiscard]] N_STRING_ID_RT_CONSTEXPR static string_id _runtime_build_from_string(const char* const _str, const size_t len)
      {
#if !N_STRIP_DEBUG
        const id_t id = (id_t)ct::hash::fnv1a<64>(_str, len);
        return string_id(id, _str, len);
#else
        return string_id((id_t)ct::hash::fnv1a<64>(_str, len));
#endif
      };
      /// \brief Slowly build from a string at runtime.
      /// \warning Slow. And may leave the string in the binary
      [[nodiscard]] N_STRING_ID_RT_CONSTEXPR static string_id _runtime_build_from_string(std::string_view view)
      {
#if !N_STRIP_DEBUG
        const id_t id = (id_t)ct::hash::fnv1a<64>(view.data(), view.size());
        return string_id(id, view.data(), view.size());
#else
        return string_id((id_t)ct::hash::fnv1a<64>(view.data(), view.size()));
#endif
      };

      [[nodiscard]] N_STRING_ID_RT_CONSTEXPR static string_id _runtime_build_from_string(string_id prev, const char* const _str, const size_t len)
      {
        const id_t id = (id_t)ct::hash::fnv1a_continue<64>((uint64_t)(id_t)prev, _str, len);
#if !N_STRIP_DEBUG
        if (prev.get_string() != nullptr)
        {
          std::string str;
          str += prev.get_string_view();
          str += std::string_view(_str, len);
          return string_id(id, str.data(), str.size());
        }
        return string_id(id);
#else
        return string_id(id);
#endif
      };

      [[nodiscard]] N_STRING_ID_RT_CONSTEXPR static string_id _runtime_build_from_string(id_t prev, const char* const _str, const size_t len)
      {
        return string_id((id_t)ct::hash::fnv1a_continue<64>((uint64_t)prev, _str, len));
      };

      constexpr static string_id _from_id_t(id_t id)
      {
        return { id };
      }

#undef N_STRING_ID_RT_CONSTEXPR
    private:
      constexpr string_id(id_t rid) : id(rid)
      {
#if !N_STRIP_DEBUG
        if !consteval
        {
          auto view = internal::id::debug::get_string_for_id(id);
          str = view.data();
          str_length = view.size();
        }
#endif
      }
#if !N_STRIP_DEBUG
      constexpr string_id(id_t rid, const char* _str, size_t _len) : id(rid), str(_str), str_length(_len)
      {
        if !consteval
        {
          str = internal::id::debug::register_string(id, {str, str_length}).data();
        }
      }
#endif

      id_t id = id_t::none;
#if !N_STRIP_DEBUG
      const char* str = nullptr;
      size_t str_length = 0;
#endif
  };
}

consteval neam::string_id operator ""_rid (const char* str, size_t len)
{
  return neam::string_id(str, len);
}

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
template<> struct fmt::formatter<neam::id_t>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template<typename FormatContext>
  static auto no_debug_format(neam::id_t id, FormatContext& ctx)
  {
    if (id == neam::id_t::invalid)
      return fmt::format_to(ctx.out(), "[id:invalid]");
    if (id == neam::id_t::none)
      return fmt::format_to(ctx.out(), "[id:none]");
    return fmt::format_to(ctx.out(), "[id:0x{:X}]", std::to_underlying(id));
  }
  template<typename FormatContext>
  auto format(neam::id_t id, FormatContext& ctx) const
  {
#if !N_STRIP_DEBUG
    {
      const std::string_view sv = neam::internal::id::debug::get_string_for_id(id);
      if (sv.data())
        return fmt::format_to(ctx.out(), "[id:0x{:X}]({})", std::to_underlying(id), sv);
    }
#endif
    return no_debug_format(id, ctx);
  }
};
template<> struct fmt::formatter<neam::string_id>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(neam::string_id sid, FormatContext& ctx) const
  {
#if !N_STRIP_DEBUG
    if (sid.get_string())
    {
      return fmt::format_to(ctx.out(), "[id:0x{:X}]({})", std::to_underlying((neam::id_t)sid), std::string_view(sid.get_string(), sid.get_string_length()));
    }
#endif

    // fallback to formatting the id:
    return fmt::formatter<neam::id_t>::no_debug_format(sid, ctx);
  }
};

#endif
