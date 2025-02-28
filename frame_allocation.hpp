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
#include "memory.hpp"
#include "raw_ptr.hpp"
#include "mt_check/mt_check_base.hpp"

namespace neam::cr
{
  /// \brief fast, thread-safe allocator that has a fast-clear / fast allocation but no dealocation
  /// \brief IsArray means that all allocations will have the same type / same memory footprint
  /// \note Contention makes it slow
  /// \todo Do a better multi-threading-aware implem
  template<size_t PageCount = 4, bool IsArray = false, uint32_t Alignment = 8>
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
      class allocator_state : public cr::mt_checked<allocator_state>
      {
        public:
          explicit allocator_state(chunk_t* chunk, uint32_t count) : entry_count(count), first(chunk) {}
          allocator_state() = default;

          ~allocator_state() { destroy(); }
          allocator_state(allocator_state&& o) = default;
          allocator_state& operator = (allocator_state&& o)
          {
            N_MTC_WRITER_SCOPE;

            if (&o == this) return *this;
            destroy();
            first = std::move(o.first);
            chunk_array = std::move(o.chunk_array);

            o.first = nullptr;
            return *this;
          }

          /// \brief Construct a fast access table for get_entry
          /// \warning Not multi-thread safe
          void build_array_access_accelerator() requires IsArray
          {
            N_MTC_WRITER_SCOPE;

            chunk_t* it = first;
            while (it != nullptr)
            {
              chunk_array.push_back(it);
              it = it->next;
            }
          }

          /// \brief Index the in the allocated memory. Requires IsArray to be true.
          /// \warning Slow if build_array_access_accelerator wasn't called
          template<typename Type>
          Type* get_entry(size_t index) const requires IsArray
          {
            N_MTC_READER_SCOPE;

            if (!first) return nullptr;

            const uint64_t data_size = memory::get_page_size() * PageCount - sizeof(chunk_t);
            const size_t entry_per_chunk = data_size / sizeof(Type);
            size_t chunk_index = index / entry_per_chunk;
            const size_t offset_in_chunk = (index % entry_per_chunk) * sizeof(Type);
            chunk_t* it = first;

            if (chunk_index < chunk_array.size())
            {
              [[likely]];
              it = chunk_array[chunk_index];
            }
            else
            {
              while (it != nullptr && chunk_index > 0)
              {
                it = it->next;
                --chunk_index;
              }
            }

            if (it == nullptr)
              return nullptr;

            if (it->offset < offset_in_chunk + sizeof(Type))
              return nullptr;

            return (Type*)(&it->data[offset_in_chunk]);
          }

          /// \brief Explicitly destroy the state (and clear all the allocations)
          void destroy()
          {
            N_MTC_WRITER_SCOPE;

            while (first != nullptr)
            {
              chunk_t* next = first->next;
              memory::free_page(first.release(), PageCount);
              first = next;
            }
            chunk_array.clear();
          }

          uint32_t size() const { return entry_count; }

        private:
          uint32_t entry_count;
          raw_ptr<chunk_t> first;
          std::vector<chunk_t*> chunk_array;
      };

    public:
      ~frame_allocator() { reset(); }

      /// \brief Allocate some memory
      void* allocate(size_t count)
      {
        // skip allocations that will always fail
        if (count > data_size)
          return nullptr;

        // skip empty allocations
        if (count == 0)
          return nullptr;

        // align the size on Alignment (which force all alignments to be Alignment)
        // and allows to disregard alignemnts as a whole
        if (count % Alignment != 0)
          count += Alignment - count % Alignment;

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
        if (current_chunk->offset + count > data_size)
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
        allocation_count += 1;
        TRACY_PLOT(pool_debug_name.data(), (int64_t)total_memory);
        return &current_chunk->data[offset];
      }

      /// \brief Allocate and construct
      template<typename Type, typename... Args>
      Type* allocate(Args&&... args)
      {
        static_assert(alignof(Type) <= Alignment, "Cannot allocate a type with alignment > Alignment");
        void* ptr = allocate(sizeof(Type));
        if (ptr != nullptr)
        {
          Type* tptr = reinterpret_cast<Type*>(ptr);
          new (tptr) Type{std::forward<Args>(args)...};
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

        chunk_t* const current = first_chunk;
        const uint32_t current_count = allocation_count;

        first_chunk = nullptr;
        allocation_count = 0;
        total_memory = 0;
        current_chunk = nullptr;

        return allocator_state(current, current_count);
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
          current = deallocate_chunk(current);
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
          current = deallocate_chunk(current);
        }
      }

      uint32_t get_allocation_count() const
      {
        std::lock_guard<spinlock> _lg(lock);
        return allocation_count;
      }

      /// \brief If IsArray, return the entry at a given index. Might be slow.
      /// \warning Slow
      template<typename Type>
      Type* get_entry(size_t index) const requires IsArray
      {
        std::lock_guard<spinlock> _lg(lock);
        constexpr size_t entry_per_chunk = data_size / sizeof(Type);
        size_t chunk_index = index / entry_per_chunk;
        const size_t offset_in_chunk = (index % entry_per_chunk) * sizeof(Type);

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
        void* ptr = memory::allocate_page(PageCount);
        chunk_t* chk = reinterpret_cast<chunk_t*>(ptr);
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
        memory::free_page(chk, PageCount);
        return next;
      }

    private:
      mutable spinlock lock;
      uint32_t allocation_count = 0;
      uint64_t total_memory = 0;
      const uint64_t data_size = memory::get_page_size() * PageCount - sizeof(chunk_t);

      chunk_t* first_chunk = nullptr;
      chunk_t* current_chunk = nullptr;
  };
}
