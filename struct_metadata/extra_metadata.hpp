//
// created by : Timothée Feuillet
// date: 2022-7-14
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
#include "../ct_string.hpp"

namespace neam::metadata
{
  struct info
  {
    // args:
    ct::string description = c_string_t<"">;
    ct::string doc_url = c_string_t<"">;

    struct metadata
    {
      metadata() = default;
      metadata(const metadata&) = default;
      metadata(const info& _info) : description(_info.description.str), doc_url(_info.doc_url.str) {}

      std::string description;
      std::string doc_url;
    };
  };

  template<typename T>
  struct range
  {
    using metadata = range;

    T min {};
    T max {};
    T step {};
  };
}

N_METADATA_STRUCT(neam::metadata::info::metadata)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(description),
    N_MEMBER_DEF(doc_url)
  >;
};

template<typename T>
N_METADATA_STRUCT_TPL(neam::metadata::range, T)
{
  N_DECLARE_STRUCT_TYPE_TPL(neam::metadata::range, T);

  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(min),
    N_MEMBER_DEF(max),
    N_MEMBER_DEF(step)
  >;
};


