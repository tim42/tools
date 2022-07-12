//
// created by : Timothée Feuillet
// date: 2022-7-11
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

#include <vector>

#include "enum.hpp"
#include "../raw_data.hpp"

namespace neam::rle
{
  /// \brief A type that can hold (and re-serialize) any type_mode
  /// Used when the real type is not available or to provide an easier access to serialized data
  struct generic_type
  {
    type_mode mode;
    type_hash_t type_hash;

    union
    {
      uint32_t version = 0;
      uint32_t variant_index;
    };

    // for raw types: data[0] is the only entry and contains the actual data
    // for containers: data is what contains the container
    // for tuples: data contains the data in the order of the tuple. See type_metadata for member index
    //             if versionned, version contains the version number
    // for variants: data.size() == 0 if the variant is empty, data.size() == 1 if the variant is non-empty.
    //             in that case, data[0] contains the relevant data and variant_index contains the index of the type (+1)
    std::vector<raw_data> data = {};

    /// \brief Write (back) the type to the encoder
    void serialize(class encoder& ec) const;

    /// \brief Read the type from metadta + decoder
    static generic_type deserialize(const struct serialization_metadata& md, const struct type_metadata& type, class decoder& dc);
    static generic_type deserialize(const struct serialization_metadata& md, const struct type_metadata& type, const raw_data& rd);
  };

  /// \brief A type that can hold (and re-serialize) any type_mode
  /// Used when the real type is not available or to provide an easier access to serialized data
  /// \warning this will deserialize _everything_
  struct deep_generic_type
  {
    type_mode mode;
    type_hash_t type_hash;

    union
    {
      uint32_t version = 0;
      uint32_t variant_index;
    };

    // for raw types: raw_type_data is the only entry and contains the actual data
    // for containers: data is what contains the container
    // for tuples: data contains the data in the order of the tuple. See type_metadata for member index
    //             if versionned, version contains the version number
    // for variants: data.size() == 0 if the variant is empty, data.size() == 1 if the variant is non-empty.
    //             in that case, data[0] contains the relevant data and variant_index contains the index of the type (+1)
    std::vector<deep_generic_type> data = {};
    raw_data raw_type_data;


    /// \brief Write (back) the type to the encoder
    void serialize(class encoder& ec) const;

    /// \brief Read the type from metadta + decoder
    static deep_generic_type deserialize(const struct serialization_metadata& md, const struct type_metadata& type, class decoder& dc);
    static deep_generic_type deserialize(const struct serialization_metadata& md, const struct type_metadata& type, const raw_data& rd);
  };
}

