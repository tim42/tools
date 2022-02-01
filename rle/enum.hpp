//
// created by : Timothée Feuillet
// date: 2022-1-21
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

namespace neam::rle
{
  enum class status
  {
    failure = 0,
    success = 1,
  };

  enum class type_mode : uint32_t
  {
    raw,        // just raw binary, nothing more.
    tuple,      // tuple, structs, ... (should be named struct but guess what, it's a keyword)
    container,  // any stl container matches that. Basically a count + a list of the same type
    variant,    // should be named union, but same issue as with struct.
                // Is a [index] + {optional data depending on index} (with index == 0 meaning no data)
                // Works for both std::optional and std::variant

    // flags:

    version_flag = 1 << 8, // present if the type is prefixed by a version.
                            // Currently, only tuples can have versions.

    // aliases:

    versioned_tuple = tuple | version_flag,
  };
}

