
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
  // A reference to a type
  struct type_reference
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    uint64_t hash;
    std::string name; // optional, in structs (tuple) is the member name
  };

  // Provide information regarding a type (including its full, canonical name)
  struct type_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    type_mode mode;
    uint32_t size; // optional, only for raw or tuple types

    // tuple, container and variant have entries there
    std::vector<type_reference> contained_types = {};

    std::string name = {}; // filled by add_type
    uint64_t hash = 0; // filled by add_type
  };

  // Can be used to decode / encode data in the RLE format used by this serializer
  // This struct is versionned and can be serialized using RLE. (and thus metadata about this struct can be generated)
  struct serialization_metadata
  {
    static constexpr uint32_t min_supported_version = 0;
    static constexpr uint32_t current_version = 0;

    uint64_t root;
    std::map<uint64_t, type_metadata> types;

    template<typename T>
    void add_type(type_metadata metadata)
    {
      metadata.hash = ct::type_hash<T>;
      metadata.name = ct::type_name<T>.str;
      types.emplace(metadata.hash, std::move(metadata));
    }

    template<typename T>
    static type_reference ref()
    {
      return { ct::type_hash<T>, {} };
    }
    template<typename T>
    static type_reference ref(std::string member_name)
    {
      return { ct::type_hash<T>, std::move(member_name) };
    }
  };
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

