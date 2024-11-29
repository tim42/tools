//
// created by : Timothée Feuillet
// date: 2022-3-11
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

#include <cstdint>
#include <memory>
#include <cstring>
#include <atomic>

#include "tracy.hpp"
#include "memory.hpp"
#include "spinlock.hpp"
#include "debug/assert.hpp"
#include "ring_buffer.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief this class provide a fast and easy way to deal with memory allocations of the same size
    /// \note thread-safe version of the raw_memory_pool. A bit more complex and slower on single-threaded contexts,
    ///       but thread-safe
    ///
    /// \attention This pool is oversimplified. Some allocation schemes won't be optimal at all.
    ///       The optimal scheme is that all objects have a similar lifespan (all are very short duration, or all are long duration/all persistent)
    ///       If you have 75% of very short duration and 25% of long/persistent objects, this will result in a 75% memory waste.
    ///
    /// \note there is no memory defragmentation
    /// \note there is no possibility to clear as we don't have tracking of allocated memory.
    ///       (we only store in-progress pages to avoid contention on pushing fully allocated stuff in lists)
    class raw_memory_pool_ts
    {
      public:
        raw_memory_pool_ts() = default;
        raw_memory_pool_ts(size_t _object_size, size_t object_alignment, uint32_t _page_count = 4)
        {
          init(_object_size, object_alignment, _page_count);
        }

        ~raw_memory_pool_ts()
        {
          free_page(write_page);
          free_page(next_write_page);
          check::debug::n_assert(is_cleared(), "Destructing a non-cleared pool (remaining: {} objects | object size: {})", get_number_of_object(), object_size);
        }

        // no copy.
        raw_memory_pool_ts(const raw_memory_pool_ts &) = delete;
        raw_memory_pool_ts &operator = (const raw_memory_pool_ts &) = delete;

        /// \brief Init the pool with the following parameters.
        /// \warning MUST be called before using the pool
        /// \warning The pool MUST be cleared (all previous allocation must be freed)
        /// \note max alignment depends on the page size
        void init(size_t _object_size, size_t object_alignment, uint32_t _page_count = 4)
        {
          check::debug::n_assert(is_cleared(), "Re-initializing a non-cleared pool");

          page_count = _page_count;
          const uint64_t page_size = memory::get_page_size();
          const uint64_t area_size = page_size * page_count;

          object_size = (_object_size + object_alignment - 1) & ~(object_alignment - 1);

          const uint64_t page_header_in_object_count = (sizeof(page_header_t) + object_size - 1) / object_size;
          const uint64_t required_data_size = page_header_in_object_count * object_size;
          object_offset = required_data_size;
          object_count_per_page = (area_size - required_data_size) / object_size;
          check::debug::n_assert(object_alignment < 0x6000, "Too many objects per page");

          write_page = allocate_page();
          next_write_page = allocate_page();
        }

        /// \brief allocate an element
        void* allocate()
        {
          check::debug::n_assert(is_init(), "Trying to allocate on a non-initialized pool.");

          // write_page/next_write_page are assumed to be always valid
          page_header_t* page;
          uint32_t index = ~0u;
          {
            // Get the page
            while (true)
            {
              {
                // std::lock_guard _lg(spinlock_shared_adapter::adapt(write_page_in_use_lock));
                page = write_page.load(std::memory_order::acquire);
                index = page->write_offset.fetch_add(1, std::memory_order_acq_rel);
                [[likely]] if (index < object_count_per_page)
                  break;
                else
                  page->write_offset.store(object_count_per_page, std::memory_order_release);
              }

              // FIXME: Use umwait
              while (write_page.load(std::memory_order_relaxed) == page);
            }
          }

          // Before storing k_page_can_be_freed_marker, so no-one can delete the page from under us
          page->allocation_count.fetch_add(1, std::memory_order_release);

          if (index == object_count_per_page - 1)
          {
            // We can only really swap current/next page at this point. This means some threads might wait a bit on page-swap
            page_header_t* const next_page = next_write_page.exchange(allocate_page(), std::memory_order_acq_rel);
            write_page.store(next_page, std::memory_order_release);

            {
              // std::lock_guard _lg(spinlock_exclusive_adapter::adapt(write_page_in_use_lock));
            }
            // mark the page as ok for release
            page->allocation_count.fetch_or(k_page_can_be_freed_marker, std::memory_order_release);
          }

          const uint32_t offset = index * object_size + object_offset;


          object_count.fetch_add(1, std::memory_order_release);

          return ((uint8_t*)page + offset);
        }

        /// \brief deallocate an object
        /// \warning the object **MUST** be allocated by the pool (or nullptr)
        void deallocate(void* p)
        {
          [[likely]] if (p)
          {
            page_header_t* const chk = get_page_for_ptr(p);

            [[maybe_unused]] const uint32_t total_count = object_count.fetch_sub(1, std::memory_order_release);
            check::debug::n_assert(total_count > 0, "Double free/corruption (global|pool object)");

            const uint32_t count = chk->allocation_count.fetch_sub(1, std::memory_order_release);
            check::debug::n_assert((count & ~k_page_can_be_freed_marker) <= object_count_per_page, "Double free/corruption (page-header)");

            // last allocation of the page, free the page (if it cannot receive more allocations
            [[unlikely]] if (count == (k_page_can_be_freed_marker | 1))
            {
              free_page(chk);
              return;
            }
          }
        }

        // gp getters
        uint32_t get_number_of_object() const { return object_count.load(std::memory_order::relaxed); }

        bool is_init() const { return write_page.load(std::memory_order_relaxed) != nullptr; }
        bool is_cleared() const { return object_count.load(std::memory_order::relaxed) == 0; }

      public: // debug
        std::string pool_debug_name;

      private: // page stuff
        struct page_header_t;
        page_header_t* allocate_page() const
        {
          page_header_t* page = (page_header_t*)memory::allocate_page(page_count);
          check::debug::n_check(page != nullptr, "Could not allocate {} memory pages", page_count);
          if (!page) return nullptr;

          // setup the chunk
          page->init_markers(*this);

          return page;
        }

        void free_page(page_header_t* ptr) const
        {
          memory::free_page(ptr, page_count);
        }

        page_header_t* get_page_for_ptr(const void* ptr) const
        {
          return page_header_t::get_start_addr(*this, ptr, page_count);
        }


      private: // structs
        struct alignas(8) page_header_t
        {
          uint64_t marker;

          std::atomic<uint16_t> allocation_count = 0;
          std::atomic<uint16_t> write_offset = 0; // in object

          uint32_t end_marker;

          void init_markers(const raw_memory_pool_ts& owner_pool)
          {
            marker = reinterpret_cast<uint64_t>(&owner_pool) ^ reinterpret_cast<uint64_t>(this) >> 12;
            end_marker = (uint32_t)(owner_pool.page_count * owner_pool.object_count_per_page * owner_pool.object_size * owner_pool.object_offset);
          }

          bool check_markers(const raw_memory_pool_ts& owner_pool) const
          {
            bool valid = true;
            valid = valid && (marker == (reinterpret_cast<uint64_t>(&owner_pool) ^ reinterpret_cast<uint64_t>(this) >> 12));
            valid = valid && (end_marker == (uint32_t)(owner_pool.page_count * owner_pool.object_count_per_page * owner_pool.object_size * owner_pool.object_offset));
            return valid;
          }

          static page_header_t* get_start_addr(const raw_memory_pool_ts& owner_pool, const void* ptr, uint32_t page_count)
          {
            const uint64_t uptr = reinterpret_cast<uint64_t>(ptr);
            const uint64_t page_size = memory::get_page_size();

            // got our starting page:
            uint64_t page_ptr = uptr & ~(page_size - 1);
            // check if the page header is in this page:
            for (uint32_t i = 0; i < page_count; ++i)
            {
              page_header_t* page = reinterpret_cast<page_header_t*>(page_ptr);
              [[likely]] if (page->check_markers(owner_pool))
                return page;
              page_ptr -= page_size;
            }

            check::debug::n_assert(false, "Unable to find page header for address (corruption / invalid pointer ?)");

            return nullptr;
          }
        };
        static_assert(sizeof(page_header_t) % 8 == 0, "invalid page header size (must be a multiple of 8)");


      private:
        // global, readonly data: (writen in init())
        uint32_t page_count = 4;
        size_t object_count_per_page = 0;
        size_t object_size = 0; // also contains alignment
        size_t object_offset = 0; // in byte, offset from the start of the page

        static constexpr uint16_t k_page_can_be_freed_marker = 0x8000;

        // atomic stuff: (rw, non-correlated data)
        std::atomic<uint32_t> object_count = 0;

        std::atomic<page_header_t*> write_page;
        std::atomic<page_header_t*> next_write_page;

        // Extra safety. Probably not needed as the condition for the race condition to trigger is either not possible or very difficult
        // (a thread would have to go to sleep right in the page-check loop, and sleep until the last object is initialized and freed)
        // Uncomment if this happens.
        // shared_spinlock write_page_in_use_lock;
    };
  } // namespace cr
} // namespace neam
