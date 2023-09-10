//
// created by : Timothée Feuillet
// date: 2021-12-3
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

#include <string.h>
#include <memory>

namespace neam
{
  namespace internal { struct operator_delete_deleter { void operator()(void* ptr) const { operator delete (ptr); } }; }
  struct raw_data
  {
    using unique_ptr = std::unique_ptr<void, internal::operator_delete_deleter>;
    unique_ptr data;
    size_t size = 0;

    operator void* () { return data.get(); }
    operator const void* () const { return data.get(); }

    void* get() { return data.get(); }
    const void* get() const { return data.get(); }

    void reset() { data.reset(); }

    /// \brief duplicate the raw data / its allocation.
    /// Explicit (no operator = ) so as to avoid mistakenly duplicating this stuff
    [[nodiscard]] raw_data duplicate() const { return duplicate(*this); }

    /// \brief Allocates and setup a raw_data
    [[nodiscard]] static raw_data allocate(size_t size) { return { unique_ptr(operator new(size)), size }; }

    /// \brief Copy a contiguous container (must have a .size() and a .data()) to a new raw_data
    template<typename ContinuousContainer>
    [[nodiscard]] static raw_data allocate_from(const ContinuousContainer& c)
    {
      raw_data ret = allocate(c.size() * sizeof(typename ContinuousContainer::value_type));
      memcpy(ret.data.get(), c.data(), ret.size);
      return ret;
    }

    /// \brief duplicate the raw data / its allocation.
    /// Explicit (no operator = ) so as to avoid mistakenly duplicating this stuff
    [[nodiscard]] static raw_data duplicate(const raw_data& other)
    {
      if (!other.data || !other.size)
        return {};
      raw_data ret = allocate(other.size);
      memcpy(ret.data.get(), other.data.get(), other.size);
      return ret;
    }

    /// \brief duplicate some pointer + size data.
    [[nodiscard]] static raw_data duplicate(const void* data, size_t size)
    {
      if (data == nullptr || !size)
        return {};
      raw_data ret = allocate(size);
      memcpy(ret.data.get(), data, size);
      return ret;
    }

    [[nodiscard]] static bool is_same(const raw_data& a, const raw_data& b)
    {
      if (&a == &b) return true;
      if (a.size != b.size) return false;
      return memcmp(a.get(), b.get(), a.size) == 0;
    }
  };
}
