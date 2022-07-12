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

#include "rle.hpp"
#include "generic_type.hpp"
#include "../container_utils.hpp"

namespace neam::rle
{
  // Instead of doing recursive deserialization, this use the metadata and part of the serialized data to skip over stuff
  static void skip_type(const serialization_metadata& md, const type_metadata& type, decoder& dc)
  {
    if (type.mode == type_mode::raw)
    {
      dc.skip(type.size);
      return;
    }
    if (type.mode == type_mode::variant)
    {
      const uint32_t index = dc.decode<uint32_t>().first;
      if (index != 0)
        return skip_type(md, md.type(type.contained_types[index - 1].hash), dc);
      return;
    }
    if (type.mode == type_mode::container)
    {
      const uint32_t count = dc.decode<uint32_t>().first;
      for (uint32_t i = 0; i < count; ++i)
        skip_type(md, md.type(type.contained_types[0].hash), dc);
      return;
    }
    else if (type.mode == type_mode::versioned_tuple || type.mode == type_mode::tuple)
    {
      if (type.mode == type_mode::versioned_tuple)
        dc.decode<uint32_t>();
      for (const auto& ref : type.contained_types)
        skip_type(md, md.type(ref.hash), dc);
      return;
    }
  }

  static raw_data get_raw_data_for_type(const serialization_metadata& md, const type_metadata& type, decoder& dc)
  {
    decoder sub_dc{dc};
    skip_type(md, type, sub_dc);
    const uint64_t size = sub_dc._get_offset() - dc._get_offset();
    raw_data ret = raw_data::duplicate(dc.get_address(), size);
    dc.skip(size);
    return ret;
  }

  void generic_type::serialize(encoder& ec) const
  {
    if (mode == type_mode::raw)
    {
      memcpy(ec.allocate(data[0].size), data[0].data.get(), data[0].size);
    }
    else if (mode == type_mode::variant)
    {
      ec.encode(variant_index);
      if (variant_index > 0)
        memcpy(ec.allocate(data[0].size), data[0].data.get(), data[0].size);
    }
    else if (mode == type_mode::container)
    {
      ec.encode((uint32_t)data.size());
      for (const auto& it : data)
        memcpy(ec.allocate(it.size), it.data.get(), it.size);
    }
    else if (mode == type_mode::versioned_tuple || mode == type_mode::tuple)
    {
      if (mode == type_mode::versioned_tuple)
        ec.encode(version);
      for (const auto& it : data)
        memcpy(ec.allocate(it.size), it.data.get(), it.size);
    }
  }

  generic_type generic_type::deserialize(const serialization_metadata& md, const type_metadata& type, const raw_data& rd)
  {
    decoder dc{rd};
    return deserialize(md, type, rd);
  }
  generic_type generic_type::deserialize(const serialization_metadata& md, const type_metadata& type, decoder& dc)
  {
    if (!dc.is_valid())
      return { .mode = type_mode::invalid, .type_hash = 0 };
    if (type.mode == type_mode::raw)
    {
      return { .mode = type.mode, .type_hash = type.hash, .data = cr::construct<std::vector>(raw_data::duplicate(dc.get_address(), type.size)) };
    }
    if (type.mode == type_mode::variant)
    {
      const uint32_t index = dc.decode<uint32_t>().first;
      if (index == 0)
      {
        return { .mode = type.mode, .type_hash = type.hash, .variant_index = index, .data = {} };
      }
      else
      {
        return
        {
          .mode = type.mode, .type_hash = type.hash, .variant_index = index,
          .data = cr::construct<std::vector>(get_raw_data_for_type(md, md.type(type.contained_types[index - 1].hash), dc))
        };
      }
    }
    if (type.mode == type_mode::container)
    {
      const uint32_t count = dc.decode<uint32_t>().first;
      std::vector<raw_data> data;
      data.reserve(count);
      for (uint32_t i = 0; i < count; ++i)
        data.push_back(get_raw_data_for_type(md, md.type(type.contained_types[0].hash), dc));
      return { .mode = type.mode, .type_hash = type.hash, .data = std::move(data) };
    }
    else if (type.mode == type_mode::versioned_tuple || type.mode == type_mode::tuple)
    {
      uint32_t version = 0;
      if (type.mode == type_mode::versioned_tuple)
        version = dc.decode<uint32_t>().first;
      std::vector<raw_data> data;
      data.reserve(type.contained_types.size());
      for (const auto& ref : type.contained_types)
        data.push_back(get_raw_data_for_type(md, md.type(ref.hash), dc));
      return { .mode = type.mode, .type_hash = type.hash, .version = version, .data = std::move(data) };
    }
  }



  void deep_generic_type::serialize(encoder& ec) const
  {
    if (mode == type_mode::raw)
    {
      memcpy(ec.allocate(raw_type_data.size), raw_type_data.data.get(), raw_type_data.size);
    }
    else if (mode == type_mode::variant)
    {
      ec.encode(variant_index);
      if (variant_index > 0)
        data[0].serialize(ec);
    }
    else if (mode == type_mode::container)
    {
      ec.encode((uint32_t)data.size());
      for (const auto& it : data)
        it.serialize(ec);
    }
    else if (mode == type_mode::versioned_tuple || mode == type_mode::tuple)
    {
      if (mode == type_mode::versioned_tuple)
        ec.encode(version);
      for (const auto& it : data)
        it.serialize(ec);
    }
  }

  deep_generic_type deep_generic_type::deserialize(const serialization_metadata& md, const type_metadata& type, const raw_data& rd)
  {
    decoder dc{rd};
    return deserialize(md, type, rd);
  }
  deep_generic_type deep_generic_type::deserialize(const serialization_metadata& md, const type_metadata& type, decoder& dc)
  {
    if (!dc.is_valid())
      return { .mode = type_mode::invalid, .type_hash = 0 };
    if (type.mode == type_mode::raw)
    {
      return { .mode = type.mode, .type_hash = type.hash,  .raw_type_data = raw_data::duplicate(dc.get_address(), type.size) };
    }
    if (type.mode == type_mode::variant)
    {
      const uint32_t index = dc.decode<uint32_t>().first;
      if (index == 0)
      {
        return { .mode = type.mode, .type_hash = type.hash, .variant_index = index, .data = {} };
      }
      else
      {
        return
        {
          .mode = type.mode, .type_hash = type.hash, .variant_index = index,
          .data = cr::construct<std::vector>(deserialize(md, md.type(type.contained_types[index - 1].hash), dc))
        };
      }
    }
    if (type.mode == type_mode::container)
    {
      const uint32_t count = dc.decode<uint32_t>().first;
      std::vector<deep_generic_type> data;
      data.reserve(count);
      for (uint32_t i = 0; i < count; ++i)
        data.push_back(deserialize(md, md.type(type.contained_types[0].hash), dc));
      return { .mode = type.mode, .type_hash = type.hash, .data = std::move(data) };
    }
    else if (type.mode == type_mode::versioned_tuple || type.mode == type_mode::tuple)
    {
      uint32_t version = 0;
      if (type.mode == type_mode::versioned_tuple)
        version = dc.decode<uint32_t>().first;
      std::vector<deep_generic_type> data;
      data.reserve(type.contained_types.size());
      for (const auto& ref : type.contained_types)
        data.push_back(deserialize(md, md.type(ref.hash), dc));
      return { .mode = type.mode, .type_hash = type.hash, .version = version, .data = std::move(data) };
    }
  }
}

