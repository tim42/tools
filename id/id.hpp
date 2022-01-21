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

#include <cstdint>
#include "../hash/fnv1a.hpp"

namespace neam
{
  /// \brief an identifier.
  enum class id_t : uint64_t
  {
    none = 0,

    invalid = ~uint64_t(0)
  };

  /// \brief Add a parameter to an identifier
  /// In hydra (using the resource system) :
  /// parametrize("/path/to/shader.frag:spirv", "main") is equivalent to "/path/to/shader.frag:spirv(main)"
  /// or:
  /// parametrize("/path/to/image.png:image:mipmap", "2") is equivalent to "/path/to/image.png:image:mipmap(2)"
  template<size_t Count>
  static constexpr id_t parametrize(id_t id, const char (&str)[Count])
  {
    id = (id_t)ct::hash::fnv1a_continue<64>((uint64_t)id, "(");
    id = (id_t)ct::hash::fnv1a_continue<64, Count>((uint64_t)id, str);
    id = (id_t)ct::hash::fnv1a_continue<64>((uint64_t)id, ")");
    return id;
  }

  /// specialize("/path/to/image.png", "image") is equivalent to "/path/to/image.png:image"
  /// specialize("/path/to/image.png:image", "mipmap") is equivalent to "/path/to/image.png:image:mipmap"
  template<size_t Count>
  static constexpr id_t specialize(id_t id, const char (&str)[Count])
  {
    id = (id_t)ct::hash::fnv1a_continue<64>((uint64_t)id, ":");
    id = (id_t)ct::hash::fnv1a_continue<64, Count>((uint64_t)id, str);
    return id;
  }
  template<size_t Count>
  static constexpr id_t specialize(id_t id, const ct::string_holder<Count>& _str)
  {
    return specialize(id, _str.string);
  }
}

// Define the hash function for id_t (which is already a hash, so no hashing needed)
namespace std
{
  template<> struct hash<neam::id_t>
  {
    constexpr size_t operator()(neam::id_t id) const
    {
      static_assert(sizeof(size_t) == sizeof(uint64_t), "Hydra may require some code change to run on this system");
      return static_cast<uint64_t>(id);
    }
  };
}