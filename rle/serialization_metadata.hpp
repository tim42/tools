
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
#include "concepts.hpp"
#include "../struct_metadata/struct_metadata.hpp"
#include "../type_id.hpp" // for type_hash
#include "../id/id.hpp" // for id_t
#include "../id/string_id.hpp" // for string_id
#include "../container_utils.hpp" // for for_each

namespace neam::rle
{
  // FIXME: a forward decl header:
  template<typename T> static T deserialize(const raw_data& data, status* opt_st = nullptr, uint64_t offset = 0);
  template<typename T> static raw_data serialize(const T& v, status* opt_st = nullptr);

  using type_hash_t = uint64_t;

  /// \brief Work around the non-copiability of raw_data
  struct attribute_t
  {
    attribute_t() = default;
    attribute_t(std::map<type_hash_t, raw_data> _attr) : attributes(std::move(_attr)) {}
    attribute_t(attribute_t&&) = default;
    attribute_t&operator = (attribute_t&&) = default;
    attribute_t(const attribute_t& o)
    {
      for (const auto& it : o.attributes)
        attributes.emplace_hint(attributes.end(), it.first, raw_data::duplicate(it.second));
    }
    attribute_t&operator = (const attribute_t& o)
    {
      if (&o == this) return *this;
      attributes.clear();
      for (const auto& it : o.attributes)
        attributes.emplace_hint(attributes.end(), it.first, raw_data::duplicate(it.second));
      return *this;
    }


    std::map<type_hash_t, raw_data> attributes = {};

    bool has(type_hash_t h) const { return attributes.contains(h); }
    template<typename T> bool has() const { return attributes.contains(ct::type_hash<std::remove_cv_t<T>>); }
    const raw_data* get(type_hash_t h) const
    {
      if (auto it = attributes.find(h); it != attributes.end())
        return &it->second;
      return nullptr;
    }
    template<typename T>
    T get() const
    {
      if (const raw_data* rd = get(ct::type_hash<std::remove_cv_t<T>>); rd != nullptr)
        return rle::deserialize<T>(*rd);
      return {};
    }
  };

  /// \brief Work around the non-copiability of raw_data
  struct default_value_t
  {
    default_value_t() = default;
    ~default_value_t() = default;
    default_value_t(raw_data&& dt) : data(std::move(dt)) {}
    default_value_t(default_value_t&& o) : data(std::move(o.data)) {}
    default_value_t(const default_value_t& o) : data(o.data.duplicate()) {}
    default_value_t& operator =(default_value_t&& o) { data = std::move(o.data); return *this; }
    default_value_t& operator =(const default_value_t& o) { data = o.data.duplicate(); return *this; }

    raw_data data = {};
  };


  /// \brief A reference to a type
  /// \note also used to provide attributes to the type-reference
  /// What attribute are provided are dependent on the encoder
  ///   (and in case of a struct/class, provided by the user in N_MEMBER_DEF([member], attributes...) )
  struct type_reference
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_hash_t hash;
    std::string name = {}; // optional, in structs (tuple) is the member name
    id_t name_hash = id_t::none; // optional, set in structs (auto-constructed from the name)

    // attributes handling:
    attribute_t attributes {};


    /// \brief Comparison is only done on type-hash. Anything specialized must be done by other means.
    friend auto operator <=> (const type_reference& a, const type_reference& b)
    {
      return a.hash <=> b.hash;
    }
  };

  /// \brief Provide information regarding a type (including its full, canonical name)
  struct type_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    static constexpr uint32_t k_invalid_index = ~0u;

    type_mode mode;

    // optional, only for raw or tuple types. Always in byte.
    // NOTE: for tuples, it is the actual size of the underlying C++ struct, not the size of the serialized data
    uint32_t size = 0;

    // tuple, container and variant have entries there
    // for tuple, they are in order of serialization
    std::vector<type_reference> contained_types = {};

    std::string name = {}; // filled by add_type
    type_hash_t hash = 0; // filled by add_type

    // Version of the type. Only if the versioned flag is present.
    // TODO: support metadata for older versions too.
    uint32_t version = current_version;

    // if not empty, the default value of the type (only really used for struct)
    // prefer using get_default_value() as it is not guaranteed to hold a value.
    default_value_t default_value = {};

    static type_metadata from(type_mode mode, std::vector<type_reference>&& refs, type_hash_t hash = 0, raw_data&& default_value = {})
    {
      return { mode, 0, std::move(refs), {}, hash, current_version, std::move(default_value) };
    }
    // raw:
    static type_metadata from(type_hash_t hash)
    {
      return { type_mode::raw, 0, {}, {}, hash };
    }

    /// \brief Convert a type to a generic representation of this type
    /// \note All metadata is stripped from the type-references
    type_metadata to_generic() const
    {
      std::vector<type_reference> refs;
      refs.reserve(contained_types.size());
      for (const auto& it : contained_types)
        refs.push_back({ it.hash, it.name, it.name_hash });
      return { mode, 0, std::move(refs), {}, (mode == type_mode::raw ? hash : 0), version, default_value };
    }

    /// \brief Return the index of the member that matches this name or k_invalid_index if not found
    uint32_t find_member(id_t name) const
    {
      if (mode == type_mode::tuple || mode == type_mode::versioned_tuple)
      {
        for (uint32_t i = 0; i < contained_types.size(); ++i)
        {
          if (contained_types[i].name_hash == name)
            return i;
        }
      }
      return k_invalid_index;
    }

    /// \brief Encode the default value in ec.
    /// \note Will always provide a valid default value.
    void get_default_value(const struct serialization_metadata& md, encoder& ec) const;

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
  // This struct is versioned and can be serialized using RLE. (and thus metadata about this struct can be generated)
  struct serialization_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_hash_t root;
    std::map<type_hash_t, type_metadata> types;

    raw_data generate_default_value() const;

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
      return { hash_of<T>(), {}, id_t::none, };
    }
    template<typename T>
    static type_reference ref(std::string member_name)
    {
      return { hash_of<T>(), std::move(member_name), string_id::_runtime_build_from_string(member_name.data(), member_name.size()), };
    }
    template<typename T, typename TypeInfo>
    static type_reference ref(std::string member_name)
    {
      std::map<type_hash_t, raw_data> attr;
      cr::for_each(TypeInfo::metadata_tuple(), [&](const auto& it)
      {
        using type = std::remove_cvref_t<decltype(it)>;
        if constexpr (concepts::SerializableStruct<type>)
        {
          // FIXME: attribute metadata?
          attr.emplace(hash_of<type>(), rle::serialize<type>(it));
        }
      });
      return { hash_of<T>(), std::move(member_name), string_id::_runtime_build_from_string(member_name.data(), member_name.size()), std::move(attr), };
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

N_METADATA_STRUCT(neam::rle::attribute_t)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(attributes)
  >;
};

N_METADATA_STRUCT(neam::rle::default_value_t)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(data)
  >;
};

N_METADATA_STRUCT(neam::rle::type_reference)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(hash),
    N_MEMBER_DEF(name),
    N_MEMBER_DEF(name_hash),
    N_MEMBER_DEF(attributes)
  >;
};

N_METADATA_STRUCT(neam::rle::type_metadata)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(hash),
    N_MEMBER_DEF(size),
    N_MEMBER_DEF(version),
    N_MEMBER_DEF(mode),
    N_MEMBER_DEF(name),
    N_MEMBER_DEF(contained_types),
    N_MEMBER_DEF(default_value)
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

