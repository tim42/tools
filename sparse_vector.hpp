//
// created by : Timothée Feuillet
// date: 2023-7-17
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

#include <deque>
#include <set>

namespace neam::cr
{
  /// \brief sparse vector
  template<typename Type>
  class sparse_vector
  {
    private:
      struct alignas(alignof(Type)) storage
      {
        storage() = default;
        storage(storage&&) = delete;
        storage& operator = (storage&&) = delete;

        uint8_t data[std::max(sizeof(uint32_t), sizeof(Type))];

        uint32_t& as_index() { return reinterpret_cast<uint32_t&>(*data); }
        const uint32_t& as_index() const { return reinterpret_cast<const uint32_t&>(*data); }

        Type& as_data() { return reinterpret_cast<Type&>(*data); }
        const Type& as_data() const { return reinterpret_cast<const Type&>(*data); }

        void destroy() { as_data().~Type(); }
      };

    public:
      sparse_vector() = default;
      sparse_vector(sparse_vector&& o)
       : elements(std::move(o.elements))
       , first_free_index(o.first_free_index)
       , free_entry_count(o.free_entry_count)
      {
        o.free_entry_count = 0;
        o.first_free_index = k_no_free_index;
      }

      sparse_vector& operator = (sparse_vector&& o)
      {
        if (&o == this) return *this;

        clear();

        elements = std::move(o.elements);
        first_free_index = o.first_free_index;
        free_entry_count = o.free_entry_count;

        o.free_entry_count = 0;
        o.first_free_index = k_no_free_index;
        return *this;
      }

      ~sparse_vector() { clear(); }

      Type& operator [] (uint32_t index) { return elements[index].as_data(); }
      const Type& operator [] (uint32_t index) const { return elements[index].as_data(); }

      template<typename... Args>
      uint32_t emplace(Args&&... args)
      {
        if (first_free_index == k_no_free_index)
        {
          elements.emplace_back(std::forward<Args>(args)...);
          return (uint32_t)elements.size() - 1;
        }

        const uint32_t index = first_free_index;
        first_free_index = elements[index].as_index();
        free_entry_count -= 1;

        new (&elements[index].as_data()) Type(std::forward<Args>(args)...);
        return index;
      }

      void remove(uint32_t index)
      {
        elements[index].destroy();
        elements[index].as_index() = first_free_index;
        first_free_index = index;
        free_entry_count += 1;
      }

      uint32_t size() const { return (uint32_t)elements.size() - free_entry_count; }
      uint32_t total_size() const { return (uint32_t)elements.size(); }

      void clear()
      {
        if constexpr (!std::is_trivially_destructible_v<Type>())
        {
          const std::set<uint32_t> free_indices = compute_free_indices_list();

          for (uint32_t i = 0; i < (uint32_t)elements.size(); ++i)
          {
            if (free_indices.contains(i)) continue;
            elements[i].destroy();
          }
        }

        elements.clear();
        first_free_index = k_no_free_index;
        free_entry_count = 0;
      }

    private:
      std::set<uint32_t> compute_free_indices_list() const
      {
        std::set<uint32_t> ret;
        uint32_t it = first_free_index;
        for (; it != k_no_free_index; it = elements[it].as_index())
          ret.emplace(it);
        return ret;
      }

    private:
      static constexpr uint32_t k_no_free_index = ~0u;

      std::deque<storage> elements;
      uint32_t first_free_index = k_no_free_index;
      uint32_t free_entry_count = 0;
  };
  using sparse_vector_uint = sparse_vector<uint32_t>;
}

