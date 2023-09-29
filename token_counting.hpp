//
// created by : Timothée Feuillet
// date: 2023-9-25
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

#include <atomic>
#include <cstdint>
#include <functional>

#include "raw_ptr.hpp"
#include "debug/assert.hpp"
#include "memory_pool.hpp"

namespace neam::cr
{
  /// \brief Alternative to pure reference counting, with emphasis on move semantics
  /// Usefull for implementing ref-counting as a secondary mechanism for tracking lifetime.
  class token_counter
  {
    private:
      struct ref_count_t
      {
        ~ref_count_t()
        {
          [[maybe_unused]] const uint32_t counter_value = counter.load(std::memory_order_acquire);
          check::debug::n_assert(counter_value == 0, "Destructing a token_counter::ref_count_t with {} left references", counter_value);
        }

        std::atomic<uint32_t> counter;
        std::function<void()> callback;
      };

    public:
      class ref
      {
        public:
          ref() = default;
          ~ref() { if (data_ref) { release(); }}
          ref(ref&& o) = default;
          ref& operator = (ref&& o) = default; // raw_ptr will assert if there's a value. We cannot move over a constructed object.

          void release()
          {
            const uint32_t counter = data_ref->counter.fetch_sub(1, std::memory_order_acq_rel);
            auto* ptr = data_ref.release();
            if (counter == 1 && ptr->callback)
              ptr->callback();
          }

          explicit operator bool() const { return !!data_ref; }

        private:
          ref(raw_ptr<ref_count_t>&& ptr) : data_ref{std::move(ptr)}
          {
            data_ref->counter.fetch_add(1, std::memory_order_release);
          }
        private:
          raw_ptr<ref_count_t> data_ref;
          friend token_counter;
      };
    public:
      token_counter() = default;
      token_counter(token_counter&& o) = default;
      token_counter& operator = (token_counter&& o) = default;

      [[nodiscard]] ref get_token() { return { data.get() }; }
      [[nodiscard]] uint32_t get_count() { return data->counter.load(std::memory_order_acquire); }

      /// \brief Set the callback for when the counter reaches 0
      template<typename F>
      void set_callback(F&& fnc) { data->callback = std::forward<F>(fnc); }
      void clear_callback() { data->callback = {}; }

    private:
      auto_pooled_ptr<ref_count_t> data = make_pooled_ptr<ref_count_t>();
  };
}

