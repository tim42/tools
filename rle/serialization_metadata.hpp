
//
// created by : Timothée Feuillet
// date: 2022-1-31
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

#include <string>
#include <vector>
#include <map>

#include "enum.hpp"
#include "../struct_metadata/struct_metadata.hpp"
#include "../type_id.hpp" // for type_hash

namespace neam::rle
{
  using type_hash_t = uint64_t;

  // A reference to a type
  struct type_reference
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_hash_t hash;
    std::string name = {}; // optional, in structs (tuple) is the member name

    friend auto operator <=> (const type_reference& a, const type_reference& b)
    {
      return a.hash <=> b.hash;
    }
  };

  // Provide information regarding a type (including its full, canonical name)
  struct type_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_mode mode;
    uint32_t size; // optional, only for raw or tuple types. Always in byte.

    // tuple, container and variant have entries there
    std::vector<type_reference> contained_types = {};

    std::string name = {}; // filled by add_type
    type_hash_t hash = 0; // filled by add_type

    static type_metadata from(type_mode mode, const std::vector<type_reference>& refs, type_hash_t hash = 0)
    {
      return { mode, 0, refs, {}, hash };
    }
    // raw:
    static type_metadata from(type_hash_t hash)
    {
      return { type_mode::raw, 0, {}, {}, hash };
    }

    type_metadata to_generic() const
    {
      return { mode, 0, contained_types, {}, (mode == type_mode::raw ? hash : 0) };
    }

    friend std::weak_ordering operator <=> (const type_metadata& a, const type_metadata& b)
    {
      if (a.hash == b.hash && a.mode == b.mode)
        return a.contained_types <=> b.contained_types;
      if (a.hash == b.hash)
        return a.mode <=> b.mode;
      return a.hash <=> b.hash;
    }
    friend bool operator == (const type_metadata& a, const type_metadata& b)
    {
      return (a <=> b) == std::weak_ordering::equivalent;
    }
  };

  // Can be used to decode / encode data in the RLE format used by this serializer
  // This struct is versionned and can be serialized using RLE. (and thus metadata about this struct can be generated)
  struct serialization_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_hash_t root;
    std::map<type_hash_t, type_metadata> types;

    const type_metadata& type(type_hash_t hash) const
    {
      if (const auto it = types.find(hash); it != types.end())
      {
        return it->second;
      }

      static const type_metadata def { type_mode::invalid, 0 };
      return def;
    }

    bool has_type(type_hash_t hash) const
    {
      return type(hash).mode != type_mode::invalid;
    }
    template<typename T>
    bool has_type() const
    {
      return type(hash_of<T>()).mode != type_mode::invalid;
    }

    template<typename T>
    static consteval type_hash_t hash_of()
    {
      return ct::type_hash<std::remove_cv_t<T>>;
    }

    // helpers for construction:
    template<typename T>
    void add_type(type_metadata metadata)
    {
      metadata.hash = hash_of<T>();
      metadata.name = ct::type_name<std::remove_cv_t<T>>.str;
      types.emplace(metadata.hash, std::move(metadata));
    }

    template<typename T>
    static type_reference ref()
    {
      return { hash_of<T>(), {} };
    }
    template<typename T>
    static type_reference ref(std::string member_name)
    {
      return { hash_of<T>(), std::move(member_name) };
    }
  };

  /// \brief check for deep equivalence between two type metadata
  static bool are_equivalent(const serialization_metadata& s, const type_metadata& a, const type_metadata& b)
  {
    if (a.mode != b.mode)
      return false;

    // for raw types, we simply compare the hash together
    // the hash is ignored for everything else
    if (a.mode == type_mode::raw)
      return a.hash == b.hash;

    if (a.contained_types.size() != b.contained_types.size())
      return false;
    for (uint32_t i = 0; i < a.contained_types.size(); ++i)
    {
      if (!are_equivalent(s, s.type(a.contained_types[i].hash), s.type(b.contained_types[i].hash)))
        return false;
    }
    return true;
  }
}

N_METADATA_STRUCT(neam::rle::type_reference)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(hash),
    N_MEMBER_DEF(name)
  >;
};

N_METADATA_STRUCT(neam::rle::type_metadata)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(hash),
    N_MEMBER_DEF(size),
    N_MEMBER_DEF(mode),
    N_MEMBER_DEF(name),
    N_MEMBER_DEF(contained_types)
  >;
};

N_METADATA_STRUCT(neam::rle::serialization_metadata)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(root),
    N_MEMBER_DEF(types)
  >;
};

