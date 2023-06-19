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
    /// \brief this class provide a fast and easy way to deal with memories allocation of the same size
    /// \note thread-safe version of the raw_memory_pool. A bit more complex and slower on single-threaded contexts,
    ///       but way faster when multi-threaded contention is high
    ///
    /// \attention This pool is oversimplified. Some allocation schemes won't be optimal at all.
    /// an optimal use would be that after a short initialization phase, the number
    /// of elements allocated remain almost always the same
    ///
    /// \note there is no memory defragmentation
    /// \note there is no possibility to clear as we don't have tracking of allocated memory.
    ///       (we only store in-progress pages to avoid contention on pushing fully allocated stuff in lists)
    ///       tho I assume it can be done without much contention
    ///
    /// \warning Chunks are not recycled once they are fully allocated, as there's no list of free entries.
    ///       This is too to avoid additional contention. To avoid a very bad case of memory consumption, please
    ///       have specific pools for objects with the same (or similar) life time
    ///       (like a pool for transient objects and a pool with objects living a bit longer)
    class raw_memory_pool_ts
    {
      private: // helpers
      private: // types
        struct alignas(8) chunk_t
        {
          uint64_t marker;
          chunk_t *next = nullptr;

          std::atomic<uint32_t> allocation_count = 0;
          uint32_t direct_alloc_offset = 0; // in byte

          std::atomic<bool> is_in_queue = false;

          uint16_t end_marker_1;
          uint32_t end_marker_2;

          void init_markers(const raw_memory_pool_ts& owner_pool)
          {
            marker = reinterpret_cast<uint64_t>(&owner_pool);
            end_marker_1 = owner_pool.page_count;
            end_marker_2 = (uint32_t)(reinterpret_cast<uint64_t>(get_start_addr(owner_pool.page_count)) >> 12);
          }

          bool check_markers(const raw_memory_pool_ts& owner_pool) const
          {
            bool valid = true;
            valid = valid && (marker == reinterpret_cast<uint64_t>(&owner_pool));
            valid = valid && (end_marker_1 == owner_pool.page_count);
            valid = valid && (end_marker_2 == (uint32_t)(reinterpret_cast<uint64_t>(get_start_addr(owner_pool.page_count)) >> 12));
            return valid;
          }

          uint8_t* get_start_addr(uint32_t page_count) const
          {
            // the object is placed at the end of the last page, so we add our size to be at the very end of that page
            uint8_t* end_ptr = (uint8_t*)(this) + sizeof(*this);
            return end_ptr - memory::get_page_size() * page_count;
          }

          static uint8_t* get_start_addr(const raw_memory_pool_ts& owner_pool, const void* ptr, uint32_t page_count)
          {
            const uint64_t uptr = reinterpret_cast<uint64_t>(ptr);
            const uint64_t page_size = memory::get_page_size();
            const uint64_t offset_in_page = page_size - sizeof(chunk_t);

            // got our starting page:
            uint64_t page_ptr = uptr & ~(page_size - 1); 
            // check if the chunk is in this page:
            for (uint32_t i = 0; i < page_count; ++i)
            {
              const chunk_t* chk = reinterpret_cast<chunk_t*>(page_ptr + offset_in_page);
              [[likely]] if (chk->check_markers(owner_pool))
                return chk->get_start_addr(page_count);
              page_ptr += page_size;
            }

            check::debug::n_assert(false, "Unable to find chunk for address (corruption / invalid pointer ?)");

            return nullptr;
          }
        };
        static_assert(sizeof(chunk_t) % 8 == 0, "invalid chunk size (must be a multiple of 8)");

        struct queue_t
        {
          queue_t() = default;
          queue_t(const queue_t& o) : root(o.root) {};
          queue_t& operator = (const queue_t& o) { root = o.root; return *this; }

          spinlock lock;
          chunk_t* root = nullptr;

          // no protected by lock
          std::atomic<uint32_t> chunk_count = 0;
        };

      public:
        raw_memory_pool_ts() = default;
        raw_memory_pool_ts(size_t _object_size, size_t object_alignment, uint32_t _page_count = 4)
        {
          init(_object_size, object_alignment, _page_count);
        }

        ~raw_memory_pool_ts()
        {
          flush_queues();
          check::debug::n_assert(is_cleared(), "Destructing a non-cleared pool");
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
          flush_queues();
          check::debug::n_assert(is_cleared(), "Re-initializing a non-cleared pool");

          page_count = _page_count;
          const uint64_t page_size = memory::get_page_size();
          const uint64_t area_size = page_size * page_count;

          object_size = (_object_size + object_alignment - 1) & ~(object_alignment - 1);

          const uint64_t required_data_size = sizeof(chunk_t);
          object_count_per_chunk = (area_size - required_data_size) / object_size;

          check::debug::n_assert(object_count_per_chunk > 0, "Cannot store those object in a memory pool (area size: {}, obj size: {}, pool data size: {})", area_size, object_size, required_data_size);

          chunk_offset = area_size - required_data_size;

          // how most of the contention is avoided:
          queues.resize(std::thread::hardware_concurrency() * 2 + 1);
        }

        /// \brief allocate an element
        void* allocate()
        {
          check::debug::n_assert(is_init(), "Trying to allocate on a non-initialized pool.");

          const uint32_t start_index = get_queue_start_index();
          const uint32_t index = (start_index) % queues.size();

          // we got a queue, perform an allocation in here:
          queue_t& queue = queues[index];

          // should never block.
          // Is there in the eventuality that a thread is scheduled while another is in here.
          std::lock_guard _lg(queue.lock);

          if (queue.root == nullptr) // no free space, allocate a chunk
          {
            queue.root = alloc_chunk(nullptr);
            if (!queue.root)
              return nullptr;
            queue.root->is_in_queue.store(true, std::memory_order_release);
            queue.chunk_count.fetch_add(1, std::memory_order_release);
            chunk_with_free_space_count.fetch_add(1, std::memory_order_relaxed);
          }

          object_count.fetch_add(1, std::memory_order_release);

          void* ptr = nullptr;
          if (queue.root->direct_alloc_offset < object_count_per_chunk * object_size)
          {
            ptr = queue.root->get_start_addr(page_count) + queue.root->direct_alloc_offset;
            queue.root->direct_alloc_offset += object_size;
          }
          else {check::debug::n_assert(false, "Bad chunk: chunk flagged as partially free yet does not contain free entries (entry count: {} vs max: {})", queue.root->allocation_count.load(std::memory_order_relaxed), object_count_per_chunk);}

          queue.root->allocation_count.fetch_add(1, std::memory_order_release);

          if (queue.root->direct_alloc_offset == object_count_per_chunk * object_size)
          {
            // we got a full chunk, unlink it
            chunk_t* old_root = queue.root;
            queue.root = queue.root->next;

            old_root->next = nullptr;
            old_root->is_in_queue.store(false, std::memory_order_release);
            queue.chunk_count.fetch_sub(1, std::memory_order_release);
            chunk_with_free_space_count.fetch_sub(1, std::memory_order_relaxed);
          }

          return ptr;
        }

        /// \brief deallocate an object
        /// \warning the object **MUST** be allocated by the pool (or nullptr)
        void deallocate(void* p)
        {
          [[likely]] if (p)
          {
            chunk_t* const chk = get_chunk_for_ptr(p);

            const uint32_t total_count = object_count.fetch_sub(1, std::memory_order_release);
            check::debug::n_assert(total_count > 0, "Double free/corruption (global|pool object)");

            const uint32_t count = chk->allocation_count.fetch_sub(1, std::memory_order_release);
            check::debug::n_assert(count > 0, "Double free/corruption (chunk)");
            check::debug::n_assert(count <= object_count_per_chunk, "Double free/corruption (chunk)");

            // last allocation of the chunk, free the chunk, if allowed
            // (only self-linked chunks can be freed as they are "lost" or "to be deleted" chunks)
//             bool is_in_queue = chk->is_in_queue.load(std::memory_order_acquire);
            [[unlikely]] if (count == 1/* && (is_in_queue == false)*/)
            {
              if (chk->is_in_queue.exchange(true, std::memory_order_acquire) == false)
                  free_chunk(chk);
//               {
//                 chk->allocation_count = 0;
//                 chk->direct_alloc_offset = 0;
// 
//                 if (!push_chunk_to_queue(chk))
//                 {
//                   free_chunk(chk);
//                 }
//               }
              return;
            }
          }
        }

        // gp getters
        uint32_t get_number_of_chunks() const { return chunk_count.load(std::memory_order::relaxed); }
        uint32_t get_number_of_object() const { return object_count.load(std::memory_order::relaxed); }

        bool is_init() const { return queues.size() != 0; }
        bool is_cleared() const { return object_count.load(std::memory_order::relaxed) == 0; }

      public: // debug
        std::string pool_debug_name;

      private: // list stuff
//         template<typename T>
//         static void list_push_front(std::atomic<T*>& list, T* entry)
//         {
//           entry->next = list.load(std::memory_order_relaxed);
//           while (!list.compare_exchange_weak(entry->next, entry, std::memory_order_release, std::memory_order_relaxed));
//         }
// 
//         template<typename T>
//         static T* list_pop_front(std::atomic<T*>& list)
//         {
//           T* entry = list.load(std::memory_order_relaxed);
//           if (entry == nullptr) return nullptr;
//           while (!list.compare_exchange_weak(entry, entry->next, std::memory_order_release, std::memory_order_relaxed))
//           {
//             [[unlikely]] if (entry == nullptr) return nullptr;
//           }
//           return entry;
//         }

      private: // chunk stuff
        chunk_t* alloc_chunk(chunk_t* next_chunk = nullptr) const
        {
          void* page = memory::allocate_page(page_count);
          if (!page) return nullptr;
          chunk_t* chk = (chunk_t*)((uint8_t*)page + chunk_offset);

          // setup the chunk
          chk->init_markers(*this);
          chk->next = next_chunk;
          chk->allocation_count = 0;
          chk->direct_alloc_offset = 0;
          chk->is_in_queue = false;

          return chk;
        }

        void free_chunk(chunk_t* ptr) const
        {
          memory::free_page(ptr->get_start_addr(page_count), page_count);
        }

        chunk_t* get_chunk_for_ptr(const void* ptr) const
        {
          uint8_t* const page_ptr = chunk_t::get_start_addr(*this, ptr, page_count);
          return reinterpret_cast<chunk_t*>(page_ptr + chunk_offset);
        }

        uint8_t* get_page_for_ptr(const void* ptr) const
        {
          return chunk_t::get_start_addr(*this, ptr, page_count);
        }

      private: // queue stuff
        uint32_t get_queue_start_index() const
        {
          static std::atomic<uint32_t> gbl_start_index = 0;
          thread_local uint32_t start_index = gbl_start_index.fetch_add(1, std::memory_order_relaxed);
          return (start_index);
        }

        bool push_chunk_to_queue(chunk_t* chunk)
        {
          const uint32_t start_index = get_queue_start_index();
          for (uint32_t i = 0; i < queues.size(); ++i)
          {
            const uint32_t index = (start_index + i) % queues.size();
            if (queues[index].chunk_count.load(std::memory_order_relaxed) <= 2)
            {
              queues[index].chunk_count.fetch_add(1, std::memory_order_release);

              std::lock_guard _lg(queues[index].lock);

              chunk_with_free_space_count.fetch_add(1, std::memory_order_relaxed);

              [[likely]] if (queues[index].root)
              {
                // insert after the first chunk
                chunk->next = queues[index].root->next;
                queues[index].root->next = chunk;
              }
              else
              {
                queues[index].root = chunk;
              }
              return true;
            }
          }

          // could not insert the chunk
          chunk->next = nullptr;
          chunk->is_in_queue.store(false, std::memory_order_release);
          return false;
        }

        // assumes the queue is locked
        // perform queue compaction
        void flush_queue(uint32_t index)
        {
          uint32_t removed = 0;
          queue_t& queue = queues[index];

          [[unlikely]] if (queue.root == nullptr)
            return;

          // do not touch the first chunk, but free other empty chunks
          chunk_t* chk = queue.root;
          while (chk != nullptr && chk->next != nullptr)
          {
            if (chk->next->allocation_count.load(std::memory_order_relaxed) == 0)
            {
              chunk_t* e = chk->next;
              chk->next = chk->next->next;
              free_chunk(e);
              ++removed;
            }
            chk = chk->next;
          }

          queue.chunk_count.fetch_sub(removed, std::memory_order_release);
        }

        void flush_queues()
        {
          for (uint32_t i = 0; i < queues.size(); ++i)
          {
            std::lock_guard _lg(queues[i].lock);
            flush_queue(i);
            if (queues[i].root && queues[i].root->allocation_count.load(std::memory_order_relaxed) == 0)
            {
              free_chunk(queues[i].root);
              queues[i].root = nullptr;
            }
          }
        }

      private:
        // global, readonly data: (writen in init())
        uint32_t page_count = 4;
        size_t object_count_per_chunk = 0;
        size_t object_size = 0; // also contains alignment
        size_t chunk_offset = 0; // in byte, offset of the chunk struct in the page

        // atomic stuff: (rw, non-correlated data)
        std::atomic<uint32_t> object_count = 0;
        std::atomic<uint32_t> chunk_with_free_space_count = 0;
        std::atomic<uint32_t> chunk_count = 0;

        // queue stuff:
        std::vector<queue_t> queues; // the vector is only modified in init()
    };
  } // namespace cr
} // namespace neam
