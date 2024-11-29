//
// created by : Timothée Feuillet
// date: 2024-6-24
//
//
// Copyright (c) 2024 Timothée Feuillet
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

#include "type_id.hpp"
#include "raw_data.hpp" // for canary stuff

#include <variant>

namespace neam::cr
{
  /// \brief Array, mainly used as a wrapper around a C array on a stack (or a overrun-safe-ish alternative to C array)
  /// \note The main difference with std::array is that this one has checks for overruns
  template<typename T, size_t Size>
  class c_array
  {
    public:
#if !N_DISABLE_CHECKS
      constexpr ~c_array()
      {
        check_overruns();
      }

      constexpr c_array() = default;

      template<typename... Args> constexpr c_array(Args&& ...args) : storage { std::forward<Args>(args)... } {}
      template<size_t N> constexpr c_array(const T(&_o)[N])
      {
        static_assert(N <= Size, "Trying to construct an array from a source array that is too big");
        std::copy_n(_o, N, begin());
      }
      template<size_t N> constexpr  c_array(T(&&_o)[N])
      {
        static_assert(N <= Size, "Trying to construct an array from a source array that is too big");
        std::move(_o, _o + N, begin());
      }

      constexpr c_array(const c_array& o)
      {
        o.check_overruns();
        std::copy_n(o.cbegin(), Size, begin());
      }
      constexpr c_array(c_array&& o)
      {
        o.check_overruns();
        std::move(o.begin(), o.end(), begin());
      }

      constexpr c_array& operator = (const c_array& o)
      {
        check_overruns();
        o.check_overruns();
        std::copy_n(o.cbegin(), Size, begin());
        return *this;
      }
      constexpr c_array& operator = (c_array&& o)
      {
        check_overruns();
        o.check_overruns();
        std::move(o.begin(), o.end(), begin());
        return *this;
      }
#endif

      constexpr T& operator [] (size_t index)
      {
        check_access(index);
        return storage[index];
      }
      constexpr const T& operator [] (size_t index) const
      {
        check_access(index);
        return storage[index];
      }

      constexpr T& front() { check_access(0); return storage[0]; }
      constexpr const T& front() const { check_access(0); return storage[0]; }

      constexpr T& back() { check_access(Size - 1); return storage[Size - 1]; }
      constexpr const T& back() const { check_access(Size - 1); return storage[Size - 1]; }

      constexpr T* begin() { check_overruns(); return storage; }
      constexpr const T* begin() const { check_overruns(); return storage; }
      constexpr const T* cbegin() const { check_overruns(); return storage; }
      constexpr T* end() { check_overruns(); return storage + Size; }
      constexpr const T* end() const { check_overruns(); return storage + Size; }
      constexpr const T* cend() const { check_overruns(); return storage + Size; }

      constexpr T* data() { check_overruns(); return storage; }
      constexpr const T* data() const { check_overruns(); return storage; }

      // for general C compatibility
      constexpr T* operator + (size_t offset) { check_access(offset); return storage + offset; }
      constexpr const T* operator + (size_t offset) const { check_access(offset); return storage + offset; }

      using array_ref = T(&)[Size];
      constexpr operator array_ref () { check_overruns(); return storage; }
      using const_array_ref = const T(&)[Size];
      constexpr operator const_array_ref () const { check_overruns(); return storage; }

      consteval static size_t size() { return Size; }

    public:
      constexpr void check_overruns() const
      {
#if !N_DISABLE_CHECKS
        // checks to validate that everything is fine, and that we can init the canaries in a consteval constructor:
        static_assert(offsetof(c_array, storage) == sizeof(::neam::internal::canary_t));
        static_assert(offsetof(c_array, storage) + sizeof(T) * Size == offsetof(c_array, __b));

        if !consteval
        {
          // only check for non-constant evaluated contextes, as the compiler will complain for array overruns
          const size_t expected_size = Size;
          ::neam::internal::check_canary<ct::type_name<c_array<T, Size>>>(storage, &expected_size);
        }
#endif
      }
      constexpr void check_access(size_t index)
      {
#if !N_DISABLE_CHECKS
        check_overruns();
        if !consteval
        {
          neam::check::debug::n_assert(index < Size, "{}: index {} is >= than the array size of {}", ct::type_name<c_array>, index, Size);
        }
#endif
      }

    private:
#if !N_DISABLE_CHECKS
      const ::neam::internal::canary_t __a = ::neam::internal::get_canary(storage, Size * sizeof(T));
#endif

      T storage[Size];

#if !N_DISABLE_CHECKS
      const ::neam::internal::canary_t __b = __a;
#endif
  };

  /// \brief Like c_array, but if the specified size is > MaxInternalSize, will allocate in the heap instead
  /// \note Size is no changeable
  template<typename T, size_t MaxInternalSize>
  class soft_c_array
  {
    private:
      using array_t = std::array<T, MaxInternalSize>;

      struct size_init_t
      {
        size_t requested_size;
      };

    public:
      static soft_c_array create_with_size(size_t size)
      {
        return { size_init_t { size } };
      }

    public:
#if !N_DISABLE_CHECKS
      ~soft_c_array()
      {
        check_overruns();
      }

      soft_c_array() = default;

      soft_c_array(const soft_c_array& o)
      {
        o.check_overruns();
        std::copy_n(o.cbegin(), size(), begin());
      }
      soft_c_array(soft_c_array&& o)
      {
        o.check_overruns();
        std::move(o.begin(), o.end(), begin());
      }

      soft_c_array& operator = (const soft_c_array& o)
      {
        check_overruns();
        o.check_overruns();
        std::copy_n(o.cbegin(), size(), begin());
        return *this;
      }
      soft_c_array& operator = (soft_c_array&& o)
      {
        check_overruns();
        o.check_overruns();
        std::move(o.begin(), o.end(), begin());
        return *this;
      }
#endif

      T& operator [] (size_t index)
      {
        check_access(index);
        return data()[index];
      }
      const T& operator [] (size_t index) const
      {
        check_access(index);
        return data()[index];
      }

      T& front() { check_access(0); return data()[0]; }
      const T& front() const { check_access(0); return data()[0]; }

      T& back() { check_access(size() - 1); return data()[size() - 1]; }
      const T& back() const { check_access(size() - 1); return data()[size() - 1]; }

      T* begin() { check_overruns(); return data(); }
      const T* begin() const { check_overruns(); return data(); }
      const T* cbegin() const { check_overruns(); return data(); }
      T* end() { check_overruns(); return data() + size(); }
      const T* end() const { check_overruns(); return data() + size(); }
      const T* cend() const { check_overruns(); return data() + size(); }

      T* data()
      {
        check_overruns();
        if (size() <= MaxInternalSize)
          return std::get<c_array<T, MaxInternalSize>>(storage).data();
        else
          return std::get<raw_data>(storage).template get_as<T>();
      }
      const T* data() const
      {
        check_overruns();
        if (size() <= MaxInternalSize)
          return std::get<c_array<T, MaxInternalSize>>(storage).data();
        else
          return std::get<raw_data>(storage).template get_as<T>();
      }

      // for general C compatibility
      T* operator + (size_t offset) { check_access(offset); return data() + offset; }
      const T* operator + (size_t offset) const { check_access(offset); return data() + offset; }

      size_t size() const { check_overruns(); return storage_size; }

    private:
      soft_c_array(size_init_t szi) : storage_size { szi.requested_size }
      {
        if (storage_size <= MaxInternalSize)
          storage.template emplace<c_array<T, MaxInternalSize>>();
        else
          storage.template emplace<raw_data>(raw_data::allocate(szi.requested_size));
      }

    private:
      void check_overruns() const
      {
#if !N_DISABLE_CHECKS
        std::visit([](auto & a)
        {
          if constexpr (!std::is_same_v<std::decay_t<decltype(a)>, std::monostate>)
            a.check_overruns();
        }, storage);
#endif
      }
      void check_access(size_t index)
      {
#if !N_DISABLE_CHECKS
        check_overruns();
        neam::check::debug::n_assert(index < size(), "{}: index {} is >= than the array size of {}", ct::type_name<soft_c_array>, index, size());
#endif
      }

    private:

      std::variant<std::monostate, c_array<T, MaxInternalSize>, raw_data> storage { c_array<T, MaxInternalSize>{} };

      const size_t storage_size = MaxInternalSize;

    private:
  };
}
