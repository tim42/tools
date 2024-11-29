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

#include "debug/assert.hpp"
#include "spinlock.hpp"
#include "memory.hpp"
#include "integer_tools.hpp"
#include "spinlock.hpp"

namespace neam::cr
{
  /// \brief Generic wrapper for queue_ts types. Will cost an additional spinlock in memory.
  template<typename InnerType>
  struct queue_ts_wrapper
  {
    static constexpr bool k_need_preinit = true;
    static constexpr bool k_need_destruct = !std::is_trivially_destructible_v<InnerType>;
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
    static constexpr bool k_need_destruct = !std::is_trivially_destructible_v<InnerType>;
    using type_t = InnerType;

    static void preinit(queue_ts_atomic_wrapper* uninit_memory)
    {
      memcpy(uninit_memory, &InvalidValue, sizeof(InnerType));
    }

    static void construct_at(queue_ts_atomic_wrapper* uninit_memory, InnerType&& v)
    {
      check::debug::n_assert(v != InvalidValue, "Cannot assign InvalidValue to a queue_ts_atomic_wrapper. Will deadlock.");
      // new (uninit_memory) queue_ts_atomic_wrapper { std::move(v) };
      uninit_memory->value.store(v, std::memory_order_release);
    }

    void wait_constructed() const
    {
      do
      {
        while (value.load(std::memory_order_relaxed) == InvalidValue);
      }
      while (value.load(std::memory_order_acquire) == InvalidValue);
    }

    void get(InnerType& v) { v = value.load(std::memory_order_relaxed); }

    std::atomic<InnerType> value;
  };

  /// \brief Task queue, where multiple threads pushes data to multiple consumer threads.
  /// \note InnerType must be movable, Type must respect queue_ts_wrapper interface
  template<typename Type, uint32_t MinEntryCountPerPage = 511>
  class queue_ts
  {
    private:
      // NOTE: the lack of contention directly comes form this number being high enough
      //       and threads not spinning/adding tasks.
      //       that number is chosen so that 8byte entries take one 4k page
      static constexpr uint32_t k_min_entries_per_page = MinEntryCountPerPage;

      using arg_t = Type::type_t;
    public:
      queue_ts()
      {
        check::debug::n_assert(entry_count_per_page >= k_min_entries_per_page - 1, "queue_ts: invalid entry-count per page: {}, min should be {}", entry_count_per_page, k_min_entries_per_page - 1);
        check::debug::n_assert(entry_count_per_page < k_page_can_be_freed_marker, "queue_ts: invalid entry-count per page: {}, max should be {}", entry_count_per_page, k_page_can_be_freed_marker - 1);
        check::debug::n_assert((index_mod % entry_count_per_page) != 0, "queue_ts: invalid index mod: {}", index_mod);

        read_page.store((page_t*)allocate_page());
        write_page.store(read_page.load());
        next_write_page.store((page_t*)allocate_page());
      }

      ~queue_ts()
      {
        write_page.store(nullptr, std::memory_order_release);
        free_page(next_write_page.load());
        // free_page(page_to_free.load());
        page_t* page = read_page.exchange(nullptr, std::memory_order_acq_rel);
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
        page_t* page;
        uint32_t index;
        {
          // Get the page
          while (true)
          {
            {
              page = write_page.load(std::memory_order::acquire);
              index = page->header.insertion_index.fetch_add(1, std::memory_order_acq_rel);
              [[likely]] if (index < entry_count_per_page)
                break;
            }

            // FIXME: Use umwait
            do
            {
              while (write_page.load(std::memory_order_relaxed) == page);
            }
            while (write_page.load(std::memory_order_acquire) == page);
          }
        }

        if (index == entry_count_per_page - 1)
        {
          page_t* const next_page = next_write_page.exchange((page_t*)allocate_page(), std::memory_order_acq_rel);
          check::debug::n_assert(next_page != nullptr, "queue_ts::push_back: impossible state found: next_write_page was expected to not be null, but is instead null");
          page->header.set_as_next(next_page); // for readers
          write_page.store(next_page, std::memory_order_release);
        }

        // construct the object:
        // NOTE: this will unlock any thread that was waiting on this operation
        Type::construct_at(&page->at(index), std::move(t));

        // Do that now, as we want to minimize time spent waiting on the try_pop_front function.
        // Incrementing the entry-count here (instead of before the construct_at) makes the class conservative
        // This counter is the number of object that are fully constructed and not being grabbed by try_pop_front
        entry_count.fetch_add(1, std::memory_order_release);

        // we got the first index, allocate the next page
        // this does increase consumed memory, but prevent almost all contention/waits caused by memory allocation
        // if (index == entry_count_per_page - 1)
        // {
        //   page_t* const null_page = next_write_page.exchange((page_t*)allocate_page(), std::memory_order_release);
        //   check::debug::n_assert(null_page == nullptr, "queue_ts::push_back: impossible state found: next_write_page was expected to be null, but is instead non-null");
        // }
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
        page_t* page;
        uint32_t index = ~0u;
        uint32_t cons_index = ~0u;
        {
          // Get the page
          while (true)
          {
            {
              std::lock_guard _lg(spinlock_shared_adapter::adapt(read_page_in_use_lock));
              page = read_page.load(std::memory_order::acquire);
              index = /*page->header.*/read_index.fetch_add(1, std::memory_order_acq_rel);
              cons_index = data_index.load(std::memory_order::acquire);

              [[likely]] if (index < entry_count_per_page)
                break;
              // else
              //   /*page->header.*/read_index.store(entry_count_per_page, std::memory_order_release);
            }

            // FIXME: Use umwait
            while (read_page.load(std::memory_order_relaxed) == page);
          }
        }

        check::debug::n_assert(index < entry_count_per_page, "queue_ts::try_pop_front: impossible state found: read-index ({}) is >= entry_count_per_page ({}). ", index, entry_count_per_page);
#if !N_DISABLE_CHECKS
        [[maybe_unused]] uint32_t windex = page->header.insertion_index.load(std::memory_order_acquire);
        check::debug::n_assert(index < windex, "queue_ts::try_pop_front: impossible state found: read-index ({}) is >= than the write-index ({}). Assertion that entry-count ({}) is conservative is invalid.", index, windex, current_count);
#endif

        [[unlikely]] if (index == entry_count_per_page - 1)
        {
          {
            // we simply want a barrier, making sure that any thread that were in the loop have exited before freeing a page they might have used
            // read page has already been changed, it's just to make sure no other thread has refs to this page
            // It might not be necessary at all, it's here just in case things are a bit slower than usual
            std::lock_guard _lg(spinlock_exclusive_adapter::adapt(read_page_in_use_lock));

            read_index.store(0, std::memory_order_relaxed);
            read_page.store(page->header.next(), std::memory_order_relaxed);
            data_index.store((data_index.load(std::memory_order_acquire) + 1) % k_consumed_elem_count, std::memory_order_relaxed);
            consumed_elem_count[data_index.load(std::memory_order_relaxed)] = 0;
          }
          // read_page.store(page->header.next(), std::memory_order_release);
          // page->header.consumed_elem_count.fetch_or(k_page_can_be_freed_marker, std::memory_order_release);
          consumed_elem_count[cons_index].fetch_or(k_page_can_be_freed_marker, std::memory_order_release);
        }

        // Wait for the object to be constructed (should not happen in most cases)
        page->at(index).wait_constructed();

        page->at(index).get(t);

        if constexpr(Type::k_need_destruct)
        {
          page->at(index).~Type();
        }

        // this operation is done last, so that the page is never referenced after
        const uint16_t res = 1 + /*page->header.*/consumed_elem_count[cons_index].fetch_add(1, std::memory_order_acq_rel);
        [[unlikely]] if (res == (k_page_can_be_freed_marker | entry_count_per_page))
        // [[unlikely]] if (index == entry_count_per_page - 1)
        {
          free_page(page_to_free.exchange(page, std::memory_order_acq_rel));
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
          Type* const data = &(((page_t*)(mem))->_data[0]);
          for (uint32_t i = 0; i < entry_count_per_page; ++i)
            Type::preinit(&data[i]);
        }

        return mem;
      }

      static void free_page(void* page_ptr)
      {
        if (!page_ptr) return;

        if constexpr (Type::k_need_destruct)
        {
          // Type* const data = (Type*)((uint8_t*)page_ptr + offsetof(page_t, _data));
          // const page_header_t* const header = (page_header_t*)page_ptr;
          // const uint32_t last = header->insertion_index.load(std::memory_order_relaxed);
          // for (uint32_t i = header->read_index.load(std::memory_order_relaxed); i < last && i < entry_count_per_page; ++i)
            // data[i].~Type();
        }
        memory::free_page(page_ptr, page_count);
      }

    private:
      struct page_t;
      struct page_header_t
      {
        std::atomic<void*> next_page = nullptr;
        // Index (multiple of Type) where to insert in the current page.
        std::atomic<uint32_t> insertion_index = 0;
        // std::atomic<uint16_t> read_index = 0;
        // std::atomic<uint16_t> consumed_elem_count = 0;

        page_t* next() const { return reinterpret_cast<page_t*>(next_page.load(std::memory_order_acquire)); }
        void set_as_next(page_t* next_ptr) { next_page.store(next_ptr, std::memory_order_release); }
      };

      struct page_t
      {
        page_header_t header = {};

        Type& at(uint32_t index)
        {
          return _data[index];
        }

        Type _data[];
      };
      static_assert(offsetof(page_t, header) == 0);

      static constexpr uint32_t k_min_page_size = offsetof(page_t, _data) + k_min_entries_per_page * sizeof(Type);
      static const inline uint32_t page_count = (uint32_t)(k_min_page_size + memory::get_page_size() - 1) / memory::get_page_size();
      static const inline uint32_t entry_count_per_page = (page_count * memory::get_page_size() - offsetof(page_t, _data)) / sizeof(Type);
      static const inline uint32_t index_mod = (8);
      static constexpr uint16_t k_page_can_be_freed_marker = 0x8000;

      alignas(64) std::atomic<page_t*> read_page = nullptr;
      std::atomic<uint32_t> data_index = 0;
      alignas(64) std::atomic<uint32_t> read_index = 0;
      static constexpr uint32_t k_consumed_elem_count = 4;
      std::atomic<uint32_t> consumed_elem_count[k_consumed_elem_count] = {0};

      alignas(64) std::atomic<page_t*> write_page = nullptr;
      std::atomic<page_t*> next_write_page = nullptr;

      std::atomic<page_t*> page_to_free = nullptr;

      alignas(64) std::atomic<int32_t> entry_count = 0; // conservative, always <= to the real entry count

      // so we can properly release pages
      alignas(64) shared_spinlock read_page_in_use_lock;
  };
}

