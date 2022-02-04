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
#include "tracy.hpp"

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
        uint32_t offset;
        chunk_t* next;
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
              chunk_t* next = first->next;
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


        std::lock_guard<spinlock> _lg(lock);

        // Setup the first chunk
        if (first_chunk == nullptr)
        {
          TRACY_PLOT_CONFIG(pool_debug_name.data(), tracy::PlotFormatType::Memory);
          // allocate the first chunk
          first_chunk = allocate_chunk();
          current_chunk = first_chunk;
        }

        // allocate a new chunk as we don't have enough space:
        if (current_chunk->offset + count > ChunkSize)
        {
          if (current_chunk->next != nullptr)
          {
            current_chunk = current_chunk->next;
          }
          else
          {
            chunk_t* new_current = allocate_chunk();
            current_chunk->next = new_current;
            current_chunk = new_current;
          }
        }

        uint32_t offset = current_chunk->offset;
        current_chunk->offset += count;
        total_memory += count;
        TRACY_PLOT(pool_debug_name.data(), (int64_t)total_memory);
        return &current_chunk->data[offset];
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
        std::lock_guard<spinlock> _lg(lock);
        if (first_chunk == nullptr)
          return {};

        chunk_t* current;

        current = first_chunk;
        first_chunk = nullptr;
        allocation_count = 0;
        total_memory = 0;
        current_chunk = nullptr;

        return allocator_state(current);
      }

      /// \brief Clear the state of the allocator, only free n chunks (will always keep the first chunk allocated
      void fast_clear(size_t chunks_to_free = 2)
      {
        std::lock_guard<spinlock> _lg(lock);
        if (first_chunk == nullptr)
          return;

        allocation_count = 0;
        total_memory = 0;
        current_chunk = first_chunk;

        chunk_t* current = first_chunk;
        chunk_t* last = first_chunk;
        while (current != nullptr)
        {
          current->offset = 0;
          current = current->next;

          if (chunks_to_free == 0)
            last = last->next;
          else
            --chunks_to_free;
        }

        // delete the extra chunks
        current = last->next;
        last->next = nullptr;
        while (current != nullptr)
        {
          chunk_t* next = current->next;
          operator delete(current);
          current = next;
        }
      }

      /// \brief Release all memory allocated by the allocator
      /// \warning NOT THREAD SAFE. This operation modify the state of the allocation pool in a way that will make any previously done
      ///          allocation invalid. It is expected that the caller has an external way to prevent concurrency for this operation
      void reset()
      {
        std::lock_guard<spinlock> _lg(lock);
        chunk_t* current = first_chunk;
        first_chunk = nullptr;
        allocation_count = 0;
        total_memory = 0;
        current_chunk = nullptr;

        while (current != nullptr)
        {
          chunk_t* next = current->next;
          operator delete(current);
          current = next;
        }
      }

      uint32_t get_allocation_count() const
      {
        std::lock_guard<spinlock> _lg(lock);
        return allocation_count;
      }

      /// \brief If IsArray, return the entry at a given index. Might be slow.
      template<typename Type>
      Type* get_entry(size_t index) const requires IsArray
      {
        std::lock_guard<spinlock> _lg(lock);
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

    public:
      std::string pool_debug_name;

    private:
      static chunk_t* allocate_chunk()
      {
        chunk_t* chk = reinterpret_cast<chunk_t*>(operator new(sizeof(chunk_t) + ChunkSize));
        if (chk != nullptr)
        {
          // we don't even care about atomicity here:
          chk->next = nullptr;
          chk->offset = 0;
        }
        return chk;
      }
      static chunk_t* deallocate_chunk(chunk_t* chk)
      {
        chunk_t* next = chk->next;
        operator delete(chk);
        return next;
      }

    private:
      mutable spinlock lock;
      uint32_t allocation_count = 0;
      uint64_t total_memory = 0;

      chunk_t* first_chunk = nullptr;
      chunk_t* current_chunk = nullptr;
  };
}
