//
// created by : Timothée Feuillet
// date: 2023-7-16
//
//
// Copyright (c) 2023 Timothée Feuillet
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

#include <utility>

#include "debug/assert.hpp"
#include "type_id.hpp"

namespace neam::cr
{
  /// \brief Like unique PTR, but does not destroy the pointer. Only there to help default construct moving constructors.
  /// \note It will error out if it still contains a valid pointer when destructed
  template<typename Type>
  class raw_ptr
  {
    public:
      using pointer = std::conditional_t<std::is_pointer_v<Type>, Type, Type*>;

      constexpr ~raw_ptr()
      {
        assert_empty();
        ptr = nullptr;
      }

      constexpr raw_ptr() = default;
      constexpr raw_ptr(std::nullptr_t) {}
      constexpr raw_ptr(pointer p) : ptr(p) {}
      constexpr raw_ptr(raw_ptr&& o) : ptr(o.release()) {}
      template<typename U>
      constexpr raw_ptr(raw_ptr<U>&& o) : ptr(o.release()) {}

      constexpr raw_ptr& operator = (std::nullptr_t)
      {
        assert_empty();
        ptr = nullptr;
        return *this;
      }
      constexpr raw_ptr& operator = (pointer p)
      {
        assert_empty();
        ptr = p;
        return *this;
      }
      constexpr raw_ptr& operator = (raw_ptr&& o)
      {
        assert_empty();
        ptr = o.release();
        return *this;
      }
      template<typename U>
      constexpr raw_ptr& operator=(raw_ptr<U>&& r)
      {
        assert_empty();
        ptr = r.release();
        return *this;
      }

      [[nodiscard]] constexpr pointer release()
      {
        pointer ret = ptr;
        ptr = nullptr;
        return ret;
      }

      [[nodiscard]] constexpr pointer reset(pointer nptr = pointer())
      {
        pointer ret = ptr;
        ptr = nptr;
        return ret;
      }
      template<typename U>
      [[nodiscard]] constexpr pointer reset(U nptr)
      {
        pointer ret = ptr;
        ptr = nptr;
        return ret;
      }

      [[nodiscard]] constexpr pointer reset(std::nullptr_t nptr)
      {
        pointer ret = ptr;
        ptr = nptr;
        return ret;
      }

      constexpr void _drop()
      {
        ptr = nullptr;
      }

      constexpr void swap(raw_ptr&& o)
      {
        std::swap(ptr, o.ptr);
      }

      [[nodiscard]] constexpr pointer get() const { return ptr; }

      constexpr explicit operator bool() const { return ptr != nullptr; }
      constexpr operator pointer() const { return ptr; }

      constexpr std::add_lvalue_reference_t<std::remove_pointer_t<pointer>> operator*() const
      {
        assert_not_empty("operator *");
        return *ptr;
      }

      constexpr pointer operator->() const
      {
        assert_not_empty("operator ->");
        return ptr;
      }

      constexpr std::strong_ordering operator <=> (const raw_ptr& t) const { return ptr <=> t.ptr; }
      constexpr std::strong_ordering operator <=> (std::nullptr_t t) const { return ptr <=> t; }

      constexpr bool operator == (const raw_ptr& t) const = default;
      constexpr bool operator != (const raw_ptr& t) const = default;
      constexpr bool operator <  (const raw_ptr& t) const = default;
      constexpr bool operator <= (const raw_ptr& t) const = default;
      constexpr bool operator >  (const raw_ptr& t) const = default;
      constexpr bool operator >= (const raw_ptr& t) const = default;

      constexpr bool operator != (std::nullptr_t t) const { return ptr != t; }
      constexpr bool operator == (std::nullptr_t t) const { return ptr == t; }
    private:
      void assert_empty() const
      {
        check::debug::n_assert(ptr == nullptr, "raw_ptr<{}> still contains a pointer at destruction", ct::type_name<Type>.view());
      }
      void assert_not_empty(std::string_view op) const
      {
        check::debug::n_assert(ptr != nullptr, "raw_ptr<{}> does not contains a pointer for this operation: {}", ct::type_name<Type>.view(), op);
      }

    private:
      pointer ptr = nullptr;
  };
}

