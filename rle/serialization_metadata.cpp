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

#include "rle.hpp"
#include "serialization_metadata.hpp"

namespace neam::rle
{
  void type_metadata::get_default_value(const struct serialization_metadata& md, encoder& ec) const
  {
    // simple case: we already have a default_value
    if (default_value.data.size > 0)
    {
      memcpy(ec.allocate(default_value.data.size), default_value.data.data.get(), default_value.data.size);
      return;
    }

    // hard case: no default value, go over the types and
    if (mode == type_mode::tuple || mode == type_mode::versioned_tuple)
    {
      if (mode == type_mode::versioned_tuple)
        ec.encode(version);
      for (const auto& type_ref : contained_types)
        md.type(type_ref.hash).get_default_value(md, ec);
      return;
    }

    // easy case: no default value, generate a zeroed raw_data of the correct size and memset it to 0
    uint32_t ret_size = 0;
    switch (mode)
    {
      case type_mode::container:
      case type_mode::variant:
        ret_size = sizeof(uint32_t); // the index/count, set to 0 is enough
        break;
      case type_mode::raw:
        ret_size = size;
        break;
      default:
        break;
    }

    if (ret_size > 0)
      memset(ec.allocate(ret_size), 0, ret_size);
  }

  raw_data serialization_metadata::generate_default_value() const
  {
    cr::memory_allocator ma;
    encoder ec(ma);
    type(root).get_default_value(*this, ec);
    return ec.to_raw_data();
  }
}
