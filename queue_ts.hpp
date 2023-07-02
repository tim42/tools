//
// created by : Timothée Feuillet
// date: 2023-6-24
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
#include <cstring>

#include "debug/debug.h"
#include "memory.hpp"
#include "integer_tools.hpp"

namespace neam::cr
{
  /// \brief Generic wrapper for queue_ts types. Will cost an additional spinlock in memory.
  template<typename InnerType>
  struct queue_ts_wrapper
  {
    static constexpr bool k_need_preinit = true;
    static constexpr bool k_need_destruct = std::is_trivially_destructible_v<InnerType>;
    using type_t = InnerType;

    static void preinit(queue_ts_wrapper* uninit_memory)
    {
      // construct the lock, and just the lock
      new (&uninit_memory->lock) spinlock;
      uninit_memory->lock.lock();
    }

    static void construct_at(queue_ts_wrapper* uninit_ptr, InnerType&& v)
    {
      new (&uninit_ptr->value) InnerType{std::move(v)};
      uninit_ptr->lock._unlock();
    }

    bool is_constructed() const
    {
      return !lock._get_state();
    }
    void wait_constructed() const
    {
      lock._wait_for_lock();
    }

    void get(InnerType& v)
    {
      v = std::move(value);
    }

    spinlock lock;
    InnerType value;
  };

  /// \brief Atomic wrapper for queue_ts types. If invalidValue is 0, will provide fast init. No extra memory.
  template<typename InnerType, auto InvalidValue = (InnerType)0>
  struct queue_ts_atomic_wrapper
  {
    // static_assert(std::is_scalar_v<InnerType>, "queue_ts_atomic_wrapper only support scalar types");
    static_assert(std::atomic<InnerType>::is_always_lock_free, "queue_ts_atomic_wrapper only support types which are always lock free");
    static constexpr bool k_need_preinit = (InvalidValue) != 0;
    static constexpr bool k_need_destruct = std::is_trivially_destructible_v<InnerType>;
    using type_t = InnerType;

    static void preinit(queue_ts_atomic_wrapper* uninit_memory)
    {
      memcpy(uninit_memory, &InvalidValue, sizeof(InnerType));
    }

    static void construct_at(queue_ts_atomic_wrapper* uninit_memory, InnerType&& v)
    {
      check::debug::n_assert(v != InvalidValue, "Cannot assign InvalidValue to a queue_ts_atomic_wrapper. Will deadlock.");
      new (uninit_memory) queue_ts_atomic_wrapper { std::move(v) };
    }

    bool is_constructed() const
    {
      return value.load(std::memory_order_acquire) != InvalidValue;
    }
    void wait_constructed() const
    {
      while (value.load(std::memory_order_acquire) == InvalidValue)
      {
        while (value.load(std::memory_order_relaxed) == InvalidValue);
      }
    }

    void get(InnerType& v) { v = value.load(std::memory_order_relaxed); }

    std::atomic<InnerType> value;
  };

  /// \brief Task queue, where multiple threads pushes data to multiple consumer threads.
  /// \note InnerType must be movable, Type must respect queue_ts_wrapper interface
  template<typename Type>
  class queue_ts
  {
    private:
      // NOTE: the lack of contention directly comes form this number being high enough
      //       and threads not spinning/adding tasks.
      //       that number is chosen so that 8byte entries take one 4k page
      static constexpr uint32_t k_min_entries_per_page = 510;

      using arg_t = Type::type_t;
    public:
      queue_ts()
      {
        check::debug::n_assert(entry_count_per_page >= k_min_entries_per_page - 1, "queue_ts: invalid entry-count per page: {}, min should be {}", entry_count_per_page, k_min_entries_per_page - 1);

        read_page.store((page_t*)allocate_page());
        write_page.store(read_page.load());
      }

      ~queue_ts()
      {
        page_t* ptf = page_to_free.exchange(nullptr, std::memory_order_acq_rel);
        if (ptf != nullptr)
          free_page(ptf);

        page_t* page = read_page.load();
        while (page != nullptr)
        {
          page_t* next = page->header.next();
          free_page(page);
          page = next;
        }
      }

      void push_back(arg_t&& t)
      {
        // write_page is assumed to be always valid
        page_t* page = write_page.load(std::memory_order::acquire);

        uint32_t index = ~0u;
        while (true)
        {
          index = page->header.insertion_index.fetch_add(1, std::memory_order_acq_rel);
          if (index >= entry_count_per_page)
          {
            // we should almost never go here, unless the allocation for the next page got stuck somewhere
            page->header.insertion_index.fetch_sub(1, std::memory_order_release);
            page_t* old_page = page;
            // reload the page:
            page = write_page.load(std::memory_order::acquire);
            // if we got a different page, we simply retry with the new page.
            // otherwise, we grab the next page.
            if (old_page == page)
            {
              page_t* next_page = page->header.next();
              if (next_page != nullptr)
                page = next_page;
              // no change, yield and reload the page
              if (page == old_page)
              {
                std::this_thread::yield();
                page = write_page.load(std::memory_order::acquire);
              }
            }
            continue;
          }

          break;
        }

        if (index == 0)
          write_page.store(page, std::memory_order_release);

        // construct the object:
        // NOTE: this will unlock any thread that was waiting on this operation
        Type::construct_at(&page->data[index], std::move(t));

        // Do that now, as we want to minimize time spent waiting on the try_pop_front function.
        // Incrementing the entry-count here (instead of before the construct_at) makes the class conservative
        // This counter is the number of object that are fully constructed and not being grabbed by try_pop_front
        entry_count.fetch_add(1, std::memory_order_release);

        // we got the first index, allocate the next page
        // this does increase consumed memory, but prevent almost all contention/waits caused by memory allocation
        if (index == 0)
        {
          page_t* next_page = (page_t*)allocate_page();
          page->header.set_as_next(next_page);
        }
      }

      /// \brief Try removing the first value from the queue.
      /// \note May return false even if there is a value, depending on the contention.
      bool try_pop_front(arg_t& t)
      {
        // Early out, doesn't cost much. Avoid costly checks.
        if (entry_count.load(std::memory_order_acquire) <= 0)
          return false;

        // We fetch-sub here, so as to keep the whole counter a conservative value of "free" object (fully constructed and not being removed)
        int32_t current_count = entry_count.fetch_sub(1, std::memory_order_acq_rel);
        if (current_count <= 0)
        {
          // Contention, give-up. There might be a new value being added, but this function is conservative.
          // We only continue if there is guarantee that we have a value to retrive
          entry_count.fetch_add(1, std::memory_order_release);
          return false;
        }

        // We have guarantee that there is an object to retrive for the current thread, and that we will never go over the write index
        page_t* page = read_page.load(std::memory_order_acquire);
        page_t* const original_page = page;
        uint32_t index = ~0u;
        while (true)
        {
          index = page->header.read_index.fetch_add(1, std::memory_order_acq_rel);
          if (index >= entry_count_per_page)
          {
            page->header.read_index.fetch_sub(1, std::memory_order_release);
            page_t* old_page = page;
            page_t* next_page = page->header.next();
            if (next_page != nullptr)
              page = next_page;
            check::debug::n_assert(page != old_page, "queue_ts::try_pop_front: impossible state found (no next page, but entry_count indicate that there is a value to get)");
            continue;
          }
          break;
        }

        [[maybe_unused]] uint32_t windex = page->header.insertion_index.load(std::memory_order_acquire);
        check::debug::n_assert(index < windex, "queue_ts::try_pop_front: impossible state found: read-index ({}) is >= than the write-index ({}). Assertion that entry-count ({}) is conservative is invalid.", index, windex, current_count);

        if (index == 0 && page != original_page) // first entry in a new page, update the read-page:
        {
          read_page.store(page, std::memory_order_release);

          page_t* ptf = page_to_free.exchange(original_page, std::memory_order_acq_rel);
          if (ptf != nullptr)
            free_page(ptf);
        }

        // Wait for the object to be constructed (should not happen in most cases)
        page->data[index].wait_constructed();

        page->data[index].get(t);

        if constexpr(Type::k_need_destruct)
        {
          page->data[index].~Type();
        }
        return true;
      }

      bool empty() const { return (entry_count.load(std::memory_order_acquire) <= 0); }
      size_t size() const
      {
        const int32_t cnt = entry_count.load(std::memory_order_acquire);
        if (cnt < 0) return 0;
        return cnt;
      }

    private:
      static void* allocate_page()
      {
        void* mem = memory::allocate_page(page_count);

        if constexpr (Type::k_need_preinit)
        {
          memset(mem, 0, sizeof(page_header_t));
          Type* const data = (Type*)((uint8_t*)mem + offsetof(page_t, data));
          for (uint32_t i = 0; i < entry_count_per_page; ++i)
            Type::preinit(&data[i]);
        }
        else
        {
          // Only necessary if we have to init our page
          memset(mem, 0, page_count * memory::get_page_size());
        }
        return mem;
      }

      static void free_page(void* page_ptr)
      {
        if constexpr (Type::k_need_destruct)
        {
          Type* const data = (Type*)((uint8_t*)page_ptr + offsetof(page_t, data));
          const page_header_t* const header = (page_header_t*)page_ptr;
          const uint32_t last = header->insertion_index.load(std::memory_order_relaxed);
          for (uint32_t i = header->read_index.load(std::memory_order_relaxed); i < last && i < entry_count_per_page; ++i)
            data[i].~Type();
        }
        memory::free_page(page_ptr, page_count);
      }

    private:
      struct page_t;
      struct page_header_t
      {
        std::atomic<void*> next_page = nullptr;
        // Index (multiple of Type) where to insert in the current page.
        std::atomic<uint16_t> insertion_index = 0;
        std::atomic<uint16_t> read_index = 0;

        page_t* next() const { return reinterpret_cast<page_t*>(next_page.load(std::memory_order_acquire)); }
        void set_as_next(page_t* next_ptr) { next_page.store(next_ptr, std::memory_order_release); }
      };

      struct page_t
      {
        page_header_t header = {};

        Type data[];
      };
      static_assert(offsetof(page_t, header) == 0);

      static constexpr uint32_t k_min_page_size = offsetof(page_t, data) + k_min_entries_per_page * sizeof(Type);
      static const inline uint32_t page_count = (uint32_t)(k_min_page_size + memory::get_page_size() - 1) / memory::get_page_size();
      static const inline uint32_t entry_count_per_page = (page_count * memory::get_page_size() - offsetof(page_t, data)) / sizeof(Type);

      std::atomic<page_t*> read_page = nullptr;
      std::atomic<page_t*> write_page = nullptr;
      std::atomic<page_t*> page_to_free = nullptr;
      std::atomic<int32_t> entry_count = 0; // conservative, always <= to the real entry count
  };
}

