//
// file : type_id.hpp
// in : file:///home/tim/projects/ntools/type_id.hpp
//
// created by : Timothée Feuillet
// date: sam. avr. 28 18:58:38 2018 GMT-0400
//
//
// Copyright (c) 2018 Timothée Feuillet
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

#include <utility>
#include "ct_string.hpp"
#include "ct_array.hpp"
#include "hash/fnv1a.hpp"

namespace neam::ct
{
  namespace details
  {
    template<typename TID, size_t... Idxs>
    constexpr std::array<char, sizeof...(Idxs) + 1> id_make_array(std::index_sequence<Idxs...>&&) { return {{TID::str[Idxs]..., '\0'}}; }

    template<typename TID, size_t Count = TID::size>
    constexpr auto id_make_array() { return id_make_array<TID>(std::make_index_sequence<Count>{}); }

    template<typename T>
    struct tid
    {
      static constexpr string type_name() noexcept
      {
        constexpr string s = { __PRETTY_FUNCTION__ };
        constexpr string t = s.pad(s.find("= ") + 2, neam::ct::strlen("]"));
        return t;
      }

      static constexpr const char* str = type_name().str;
      static constexpr size_t size = type_name().size;
    };
    template<auto V>
    struct vid
    {
      static constexpr string type_name() noexcept
      {
        constexpr string s = { __PRETTY_FUNCTION__ };
        constexpr string t = s.pad(s.find("= ") + 2, neam::ct::strlen("]"));
        return t;
      }

      static constexpr const char* str = type_name().str;
      static constexpr size_t size = type_name().size;
    };

    /// This class has for effect to reduce the size of the executable:
    /// it holds the name for a given type T, and only that name (no other irrelevant data).
    /// It will generate some type-info (the name of the type will be stored in the binary), if rtti is enabled.
    template<typename T>
    struct tholder { static constexpr auto name = details::id_make_array<details::tid<T>>(); };
    template<auto V>
    struct vholder { static constexpr auto name = details::id_make_array<details::vid<V>>(); };
  } // namespace details

  /// \brief The name of the type, as a constexpr string.
  template<typename T>
  static constexpr string type_name = { details::tholder<T>::name.data(), details::tholder<T>::name.size() - 1 };
  /// \brief The name of the decayed type, as a constexpr string.
  template<typename T>
  static constexpr string decayed_type_name = type_name<std::decay_t<T>>;

  /// \brief A string for the corresponding value
  template<auto V>
  static constexpr string value_name = { details::vholder<V>::name.data(), details::vholder<V>::name.size() - 1 };

  /// \brief A hash for the type. No uniqueness guaranteed.
  template<typename T>
  static constexpr auto type_hash = hash::fnv1a<64>(details::tholder<T>::name.data(), details::tholder<T>::name.size() - 1);
  /// \brief A hash for the decayed type. No uniqueness guaranteed.
  template<typename T>
  static constexpr auto decayed_type_hash = type_hash<std::decay_t<T>>;

  /// \brief A (gratuitous) hash for the corresponding value. No uniqueness guaranteed.
  template<auto V>
  static constexpr auto value_hash = hash::fnv1a<64>(details::vholder<V>::name.data(), details::vholder<V>::name.size() - 1);
} // namespace neam::ct

#ifdef NCT_TESTS
    static_assert(neam::ct::type_name<int> == neam::ct::string{"int"});
    static_assert(neam::ct::type_name<double> == neam::ct::string{"double"});
    static_assert(neam::ct::type_name<double[]> == neam::ct::string{"double []"});
    static_assert(neam::ct::type_name<neam::ct::string> == neam::ct::string{"neam::ct::string"});
#endif

