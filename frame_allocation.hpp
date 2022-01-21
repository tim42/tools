//
// created by : Timothée Feuillet
// date: 2022-1-14
//
//
// Copyright (c) 2022 Timothée Feuillet
//
// Permission is hereby granted, free of charge,  to any person obtaining  a copy
// of this software and associated documentation files (the "Software"),  to deal
// in the Software without restriction,  including without  limitation the rights
// to use,  copy,  modify,  merge,  publish, distribute, sublicense,  and/or sell
// copies  of  the  Software,  and  to  permit persons  to  whom  the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED  "AS IS",  WITHOUT WARRANTY OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING  BUT  NOT  LIMITED TO THE WARRANTIES  OF  MERCHANTABILITY,
// FITNESS FOR  A  PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS  OR  COPYRIGHT HOLDERS  BE  LIABLE FOR  ANY CLAIM,  DAMAGES  OR  OTHER
// LIABILITY,  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include <atomic>

namespace neam::cr
{
  /// \brief fast, thread-safe allocator that has a fast-clear / fast allocation but no dealocation
  /// \brief IsArray means that all allocations will have the same type / same memory footprint
  template<size_t ChunkSize = 16 * 1024, bool IsArray = false>
  class frame_allocator
  {
    private:
      struct chunk_t
      {
        std::atomic<uint32_t> offset;
        std::atomic<chunk_t*> next;
        uint8_t data[];
      };

    public:
      /// \brief non-thread safe class that will store the current state of the allocator for postponning deallocation
      class allocator_state
      {
        public:
          explicit allocator_state(chunk_t* chunk) : first(chunk) {}
          allocator_state() = default;

          ~allocator_state() { destroy(); }
          allocator_state(allocator_state&& o) : first(o.first) { o.first = nullptr; }
          allocator_state& operator = (allocator_state&& o)
          {
            if (&o == this) return *this;
            destroy();
            first = o,first;
            o.first = nullptr;
          }

          void destroy()
          {
            while (first != nullptr)
            {
              chunk_t* next = first.next.load(std::memory_order_relaxed);
              operator delete(first);
              first = next;
            }
          }
        private:
          chunk_t* first = nullptr;
      };

    public:
      ~frame_allocator() { reset(); }

      /// \brief Allocate some memory
      void* allocate(size_t count)
      {
        // Setup the first chunk
        if (first_chunk == nullptr)
        {
          // Need a lock here, as only a thread can do that operation
          if (acquire_lock())
          {
            // allocate the first chunk
            first_chunk = allocate_chunk();
            current_chunk = first_chunk;

            release_lock();
          }
        }

        // skip allocations that will always fail
        if (count > ChunkSize)
          return nullptr;

        // skip empty allocations
        if (count == 0)
          return nullptr;

        // align the size on 8 (which force all alignments to be 8)
        // and allows to disregard alignemnts as a whole
        if (count % 8 != 0)
          count += 8 - count % 8;

        wait_lock_released();

        thread_in_allocator.fetch_add(1);
        // State is correct and the data is stable, let the sub-allocate handle the rest
        void* ret = sub_allocate(count, *current_chunk.load(std::memory_order_acquire));
        thread_in_allocator.fetch_sub(1);
        return ret;
      }

      /// \brief Allocate and construct
      template<typename Type, typename... Args>
      Type* allocate(Args&&... args)
      {
        static_assert(alignof(Type) <= 8, "Cannot allocate a type with alignment > 8");
        void* ptr = allocate(sizeof(Type));
        if (ptr != nullptr)
        {
          Type* tptr = reinterpret_cast<Type*>(ptr);
          new (tptr) Type(std::forward<Args>(args)...);
          return tptr;
        }
        return nullptr;
      }

      /// \brief thread-safe alternative to reset/fast-clear
      /// \warning the result type is not thread-safe.
      ///          It is the duty of the caller to release the memory at a correct time
      allocator_state swap_and_reset()
      {
        if (first_chunk == nullptr)
          return {};

        chunk_t* current;

        state_force_acquire_lock();
        {
          current = first_chunk;
          first_chunk = nullptr;
          allocation_count.store(0, std::memory_order_relaxed);
          current_chunk.store(nullptr, std::memory_order_relaxed);
        }
        release_lock();

        return allocator_state(current);
      }

      /// \brief Clear the state of the allocator, only free n chunks (will always keep the first chunk allocated
      /// \warning NOT THREAD SAFE. This operation modify the state of the allocation pool in a way that will make any previously done
      ///          allocation invalid. It is expected that the caller has an external way to prevent concurrency for this operation
      void fast_clear(size_t chunks_to_free = 2)
      {
        if (first_chunk == nullptr)
          return;

        allocation_count.store(0, std::memory_order_relaxed);
        current_chunk.store(first_chunk, std::memory_order_relaxed);

        chunk_t* current = first_chunk;
        chunk_t* last = first_chunk;
        while (current != nullptr)
        {
          current->offset.store(0, std::memory_order_relaxed);
          current = current->next.load(std::memory_order_relaxed);

          if (chunks_to_free == 0)
            last = last->next.load(std::memory_order_relaxed);
          else
            --chunks_to_free;
        }

        // delete the extra chunks
        current = last->next.load(std::memory_order_relaxed);
        while (current != nullptr)
        {
          chunk_t* next = current->next.load(std::memory_order_relaxed);
          operator delete(current);
          current = next;
        }
      }

      /// \brief Release all memory allocated by the allocator
      /// \warning NOT THREAD SAFE. This operation modify the state of the allocation pool in a way that will make any previously done
      ///          allocation invalid. It is expected that the caller has an external way to prevent concurrency for this operation
      void reset()
      {
        chunk_t* current = first_chunk;
        first_chunk = nullptr;
        allocation_count.store(0, std::memory_order_relaxed);
        current_chunk.store(nullptr, std::memory_order_relaxed);

        while (current != nullptr)
        {
          chunk_t* next = current->next.load(std::memory_order_relaxed);
          operator delete(current);
          current = next;
        }
      }

      uint32_t get_allocation_count() const
      {
        return allocation_count.load(std::memory_order_acquire);
      }

      /// \brief If IsArray, return the entry at a given index. Might be slow.
      template<typename Type>
      Type* get_entry(size_t index) const requires IsArray
      {
        constexpr size_t entry_per_chunk = ChunkSize / sizeof(Type);
        size_t chunk_index = index / entry_per_chunk;
        const size_t offset_in_chunk = (index % entry_per_chunk) * ChunkSize;

        chunk_t* it = first_chunk;
        while (it != nullptr && chunk_index > 0)
        {
          it = it->next.load(std::memory_order_acquire);
          --chunk_index;
        }
        if (it == nullptr)
          return nullptr;

        if (it->offset.load(std::memory_order_acquire) < offset_in_chunk + sizeof(Type))
          return nullptr;

        return (Type*)(&it->data[offset_in_chunk]);
      }

    private:
      static chunk_t* allocate_chunk()
      {
        chunk_t* chk = reinterpret_cast<chunk_t*>(operator new(sizeof(chunk_t) + ChunkSize));
        if (chk != nullptr)
        {
          // we don't even care about atomicity here:
          chk->next.store(nullptr, std::memory_order_relaxed);
          chk->offset.store(0, std::memory_order_relaxed);
        }
        return chk;
      }
      static chunk_t* deallocate_chunk(chunk_t* chk)
      {
        chunk_t* next = chk->next;
        operator delete(chk);
        return next;
      }

      /// \brief Assumes the state is safe and the count is valid
      void* sub_allocate(uint32_t count, chunk_t& current_chunk_lcl)
      {
        uint32_t old_offset;
        do
        {
          old_offset = current_chunk_lcl.offset.fetch_add(count, std::memory_order_acq_rel);

          // If we have the guarantee that all allocations have the same size, there's no need to have the loop
          if constexpr (IsArray)
            break;

          // Check that we are not fooled by another thread trying to allocate a big chunk
          // This avoid doing too many chunk allocation and reduces a bit memory waste at the cost of cpu-time
          if (old_offset <= ChunkSize)
            break;

          // revert the operation
          current_chunk_lcl.offset.fetch_sub(count, std::memory_order_release);
        }
        while (true);

        if (old_offset + count > ChunkSize)
        {
          // revert the offset
          current_chunk_lcl.offset.fetch_sub(count, std::memory_order_release);

          // maybe a following chunk will have something:
          chunk_t* next_chunk = current_chunk_lcl.next.load(std::memory_order_acquire);
          chunk_t* current = &current_chunk_lcl;
          if (next_chunk != nullptr)
          {
            // move the current chunk to the next (if possible, just to maintain state)
            current_chunk.compare_exchange_strong(current, next_chunk, std::memory_order_release);

            // iterate on the next chunk:
            // (note, we don't jump chunks as it is not probable to happen (again, some memory compaction at the cost of cpu time)
            return sub_allocate(count, *next_chunk);
          }

          // there is no next chunk. Allocate one.
          // Multiple threads can go here, it's fine, all allocated chunk will be added to the chunk list
          chunk_t* new_chunk = allocate_chunk();
          // Add the chunk at the end of the list
          while (!current->next.compare_exchange_strong(next_chunk, new_chunk, std::memory_order_release, std::memory_order_relaxed))
          {
            current = next_chunk;
            next_chunk = nullptr;
          }

          // iterate over the real next chunk (can be a chunk we did not allocate)
          return sub_allocate(count, *current_chunk_lcl.next.load(std::memory_order_acquire));
        }

        // found a place !
        allocation_count.fetch_add(1, std::memory_order_relaxed);
        return &current_chunk_lcl.data[old_offset];
      }

      /// \brief acquire the lock, if the current thread has the lock, returns immediatly.
      ///        otherwise wait for the lock to go back to the default value (if do_wait is true)
      bool acquire_lock(bool do_wait = true)
      {
        uint32_t has_lock = 0;
        if (lock.compare_exchange_strong(has_lock, 1, std::memory_order_acq_rel))
        {
          return true;
        }
        else
        {
          if (do_wait)
            wait_lock_released();
          return false;
        }
      }

      void release_lock()
      {
        lock.store(0, std::memory_order_release);
      }

      void wait_lock_released()
      {
        while (lock.load(std::memory_order_relaxed) == 1);
//         lock.wait(1, std::memory_order_acquire);
      }

      /// \brief Force acquire the lock, wait untill no thread are in allocation and return
      ///        Return when the lock has been acquired
      void state_force_acquire_lock()
      {
        while (lock.load(std::memory_order_relaxed) == 0);
//         lock.wait(0, std::memory_order_acquire);

        // force acquire the lock: (will prevent new threads to enter the allocator)
        uint32_t has_lock = 0;
        while (!lock.compare_exchange_strong(has_lock, 1, std::memory_order_acq_rel))
        {
          has_lock = 0;
          while (lock.load(std::memory_order_relaxed) == 0);
//           lock.wait(0, std::memory_order_acquire);
        }

        // wait for threads in the allocator to do their stuff:
        while (thread_in_allocator.load(std::memory_order_relaxed) > 0);
      }

    private:
      std::atomic<uint32_t> lock;
      std::atomic<uint32_t> thread_in_allocator = 0;
      std::atomic<uint32_t> allocation_count = 0;

      chunk_t* first_chunk = nullptr;
      std::atomic<chunk_t*> current_chunk = nullptr;
  };
}
