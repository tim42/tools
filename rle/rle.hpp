//
// created by : Timothée Feuillet
// date: 2021-12-2
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

#ifndef N_RLE_VERBOSE_FAILS
  #define N_RLE_VERBOSE_FAILS 0
#endif

#if N_RLE_VERBOSE_FAILS
  #include "../logger/logger.hpp"
  #define N_RLE_LOG_FAIL(...) ::neam::cr::out().warn(__VA_ARGS__)
#else
  #define N_RLE_LOG_FAIL(...)
#endif

#include "rle_decoder.hpp"
#include "rle_encoder.hpp"
#include "helpers.hpp"
#include "struct_helper.hpp"

namespace neam::rle
{
  // shortcuts for easy de/serialization /  metadata generation:

  /// \brief Serialize. Returns an empty raw_data on failure
  template<typename T>
  static raw_data serialize(const T& v, status* opt_st = nullptr)
  {
    cr::memory_allocator ma;
    encoder ec(ma);
    status st = status::success;
    coder<T>::encode(ec, v, st);
    if (opt_st != nullptr)
      *opt_st = st;
    if (st == status::failure)
      return {};
    return ec.to_raw_data();
  }

  /// \brief Deserialize. Returns a default constructed object if it failed.
  template<typename T>
  static T deserialize(const raw_data& data, status* opt_st = nullptr)
  {
    status st;
    if (opt_st == nullptr)
      opt_st = &st;
    *opt_st = status::success;
    decoder dc = data;
    return coder<T>::decode(dc, *opt_st);
  }

  /// \brief Deserialize in-place, destructing and re-constructing members as needed
  template<concepts::SerializableStruct T>
  status in_place_deserialize(const raw_data& data, T& dest)
  {
    status st = status::success;
    decoder dc = data;
    coder<T>::decode(dest, dc, st);
    return st;
  }

  /// \brief Generate the metadata for a given type
  template<typename T>
  static serialization_metadata generate_metadata()
  {
    serialization_metadata mt;
    mt.root = ct::type_hash<T>;
    coder<T>::generate_metadata(mt);
    return mt;
  }
  
}
