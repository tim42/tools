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

#include "ct_string.hpp"
#include "debug/assert.hpp"

namespace neam
{
  namespace internal
  {
#if !N_DISABLE_CHECKS // NOTE: This may interfer with valgrind ability to check overruns
    struct canary_t
    {
      static constexpr uint64_t k_deleted_value = 0x7FFFFFFFFFFAFFF8;
      static constexpr uint64_t k_value_base_hash = 0xA5EF7D1E098B4A33;
      static constexpr uint64_t k_size_mask = 0x000000FFFFFFFFFF;
      uint64_t value;
      uint64_t size;
    };
    inline uint64_t compute_canari_value(const void* base_ptr, uint64_t size)
    {
      return (1 + reinterpret_cast<uint64_t>(base_ptr)) * (1 + (size & canary_t::k_size_mask)) * canary_t::k_value_base_hash;
    }
    template<ct::string_holder Name = "raw_data">
    inline void check_canary(const void* base_ptr, const size_t* expected_size = nullptr)
    {
      if (!base_ptr) return;
      const canary_t* start = (const canary_t*)((const uint8_t*)base_ptr - sizeof(canary_t));
      neam::check::debug::n_assert(start->value != canary_t::k_deleted_value, "{}: use after free", Name.view());
      neam::check::debug::n_assert((start->size & ~canary_t::k_size_mask) == (start->value & ~canary_t::k_size_mask), "{}: underflow detected", Name.view());
      neam::check::debug::n_assert(start->value == compute_canari_value(base_ptr, start->size), "{}: underflow detected", Name.view());
      if (expected_size)
        neam::check::debug::n_assert((start->size & canary_t::k_size_mask) >= *expected_size, "{}: invalid size (size ({}) above allocated size ({}))", Name.view(), start->size, *expected_size);
      const canary_t* end = (const canary_t*)((const uint8_t*)base_ptr + (start->size & canary_t::k_size_mask));
      neam::check::debug::n_assert(end->size == start->size, "{}: overrun detected (size crosscheck | {} vs {})", Name.view(), start->size, end->size);
      neam::check::debug::n_assert(end->value == start->value, "{}: overflow detected", Name.view());
    }
    inline canary_t get_canary(void* data_ptr, size_t size)
    {
      const uint64_t value = compute_canari_value(data_ptr, size);
      const uint64_t size_mk = size | (value & ~canary_t::k_size_mask);
      return { .value = value, .size = size_mk };
    }
    inline void write_canary(canary_t& start, canary_t& end, void* data_ptr, size_t size)
    {
      start = get_canary(data_ptr, size);
      end = start;
    }
    // write the canary value, and return the offset pointer
    inline void* write_canary(void* ptr, size_t size)
    {
      ptr = (uint8_t*)ptr + sizeof(canary_t);

      canary_t* start = (canary_t*)((uint8_t*)ptr - sizeof(canary_t));
      canary_t* end = (canary_t*)((uint8_t*)ptr + size);

      write_canary(*start, *end, ptr, size);

      return ptr;
    }

    constexpr size_t k_canary_extra_size = 2 * sizeof(canary_t);
#endif

    inline void* allocate_memory(size_t size)
    {
      if (size == 0) return nullptr;

#if N_DISABLE_CHECKS
      return operator new (size);
#else
      return write_canary(operator new (size + k_canary_extra_size), size);
#endif
    }

    struct operator_delete_deleter
    {
      void operator()(void* ptr) const
      {
#if N_DISABLE_CHECKS
        operator delete (ptr);
#else
        if (ptr != nullptr)
        {
          check_canary(ptr);
          canary_t* start = (canary_t*)((uint8_t*)ptr - sizeof(canary_t));
          start->value = canary_t::k_deleted_value;
          start->size = 0;
          operator delete ((void*)start);
        }
#endif
      }
    };
  }
  struct raw_data
  {
    using unique_ptr = std::unique_ptr<void, internal::operator_delete_deleter>;
    unique_ptr data;
    size_t size = 0;

    void check_overruns() const
    {
#if !N_DISABLE_CHECKS
      internal::check_canary(data.get(), &size);
#endif
    }

    operator void* () { check_overruns(); return data.get(); }
    operator const void* () const { check_overruns(); return data.get(); }

    void* get() { check_overruns(); return data.get(); }
    const void* get() const { check_overruns(); return data.get(); }

    template<typename T>
    T* get_as() { check_overruns(); return (T*)data.get(); }
    template<typename T>
    const T* get_as() const { check_overruns(); return (T*)data.get(); }

    std::string_view get_as_string_view() const { check_overruns(); return { (const char*)data.get(), size }; }

    explicit operator bool () { check_overruns(); return !!data; }

    void reset() { check_overruns(); data.reset(); size = 0; }

    /// \brief duplicate the raw data / its allocation.
    /// Explicit (no operator = ) so as to avoid mistakenly duplicating this stuff
    [[nodiscard]] raw_data duplicate() const { check_overruns(); return duplicate(*this); }

    /// \brief Allocates and setup a raw_data
    [[nodiscard]] static raw_data allocate(size_t size) { return { unique_ptr(internal::allocate_memory(size)), size }; }

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
      other.check_overruns();
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

    template<typename T>
    [[nodiscard]] static raw_data duplicate(const T& stdlayout_data) requires(std::is_standard_layout_v<T>)
    {
      return duplicate(&stdlayout_data, sizeof(stdlayout_data));
    }

    [[nodiscard]] static bool is_same(const raw_data& a, const raw_data& b)
    {
      a.check_overruns();
      b.check_overruns();
      if (&a == &b) return true;
      if (a.size != b.size) return false;
      return memcmp(a.get(), b.get(), a.size) == 0;
    }

    static void* allocate_raw_memory(size_t size)
    {
      return internal::allocate_memory(size);
    }
    static void free_allocated_raw_memory(void* ptr)
    {
      internal::operator_delete_deleter{}(ptr);
    }
  };
}
