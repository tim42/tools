//
// file : memory_pool.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/memory_pool.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 14/02/2014 11:31:05
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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

#ifndef __N_2080156092666096177_2010861593__MEMORY_POOL_HPP__
# define __N_2080156092666096177_2010861593__MEMORY_POOL_HPP__

#include <cstdint>
#include <memory>

#include "tracy.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief this class provide a fast and easy way to deal with 
    /// \tparam ObjectType is the type of object stored in the pool
    /// \tparam ObjectCount is the number of object per chunk.
    ///
    /// \attention This pool is oversimplified. Some allocation schemes won't be optimal at all...
    /// an optimal use would be that after a short initialization phase, the number
    /// of elements allocated remain almost always the same
    ///
    /// \note the memory is held until someone call \e clear() or until the pool is destructed.
    template<typename ObjectType, size_t ObjectCount = 1024>
    class memory_pool
    {
      private: // helpers
      private: // types
        struct chunk;

        // free memory
        struct alignas(8) allocation_slot
        {
          union
          {
            allocation_slot *next;
            ObjectType obj;
          };
          uint64_t chunk_data;

          // get allocation_slot for pointer
          static allocation_slot *get_slot(void *ptr)
          {
            return reinterpret_cast<allocation_slot *>(reinterpret_cast<size_t>(ptr));
          }

          bool is_free() const { return (chunk_data & 1) == 0; }
          chunk* get_chunk() const { return reinterpret_cast<chunk*>(chunk_data & ~1); }

          void set_free(bool free) { chunk_data = (chunk_data & ~1) | (free ? 0 : 1); }
          void set_chunk(chunk* chk) { chunk_data |= reinterpret_cast<uint64_t>(chk) & ~1; }
        };

        static_assert(alignof(allocation_slot) == 8, "Invalid object type: requested alignment greater than 8");

        struct chunk
        {
          uint32_t object_count = 0;
          uint32_t direct_alloc_offset = 0;
          chunk *next = nullptr;
          chunk *prev = nullptr;

          allocation_slot data[0]; // clang doesn't likes the '[]' array.
        };

        static constexpr size_t k_allocation_size = sizeof(chunk) + ObjectCount * sizeof(allocation_slot);

      public:
        memory_pool()
          : object_count(0), chunk_count(0), first_chunk(nullptr), first_free(nullptr)
        {
        }
        memory_pool(memory_pool &&mp)
          : object_count(mp.object_count), chunk_count(mp.chunk_count), first_chunk(mp.first_chunk), first_free(mp.first_free)
        {
          mp.object_count = 0;
          mp.chunk_count = 0;
          mp.first_chunk = nullptr;
          mp.first_free = nullptr;
        }

        memory_pool &operator = (memory_pool &&mp)
        {
          if (&mp == this)
            return *this;

          clear();
          object_count = mp.object_count;
          chunk_count = mp.chunk_count;
          first_chunk = mp.first_chunk;
          first_free = mp.first_free;
          mp.object_count = 0;
          mp.chunk_count = 0;
          mp.first_chunk = nullptr;
          mp.first_free = nullptr;
          return *this;
        }

        ~memory_pool()
        {
          clear();
        }

        // no copy.
        memory_pool(const memory_pool &) = delete;
        memory_pool &operator = (const memory_pool &) = delete;

        // clear the pool, remove every chunks, every element slots.
        // NOTE: could be slow.
        void clear()
        {
          for (chunk *chk = first_chunk; chk != nullptr;)
          {
            // for each slots, call the destructor if needed
            for (size_t i = 0; i < ObjectCount && chk->object_count > 0; ++i)
            {
              if (!chk->data[i].is_free())
                chk->data[i].obj.~ObjectType();
            }
            chunk *next = chk->next;
            operator delete ((void*)chk);
            chk = next;
          }

          object_count = 0;
          TRACY_PLOT(pool_debug_name.data(), (int64_t)object_count);
          TRACY_PLOT_CONFIG(pool_debug_name.data(), tracy::PlotFormatType::Memory);
          chunk_count = 0;
          first_chunk = nullptr;
          first_free = nullptr;
        }

        void fast_clear(size_t chunk_to_remove = 1)
        {
          // destroy remaining objects (can be slow)
          if (object_count > 0)
          {
            for (chunk* chk = first_chunk; chk != nullptr; chk = chk->next)
            {
              if (chk->object_count == 0)
                continue;
              // for each slots, call the destructor if needed
              for (size_t i = 0; i < ObjectCount; ++i)
              {
                if (!chk->data[i].is_free())
                  chk->data[i].obj.~ObjectType();
              }
            }
          }

          // reset the state:
          object_count = 0;
          TRACY_PLOT(pool_debug_name.data(), (int64_t)object_count);
          for (chunk* chk = first_chunk; chk != nullptr; chk = chk->next)
          {
            chk->object_count = 0;
            chk->direct_alloc_offset = 0;
            memset(chk->data, 0, ObjectCount * sizeof(allocation_slot));
          }
        }

        // construct an allocated object
        template<typename... Args>
        ObjectType *construct(ObjectType *p, Args &&... args)
        {
          new (p) ObjectType (std::forward<Args>(args)...);
          return p;
        }

        void destroy(ObjectType *p)
        {
          p->~ObjectType();
        }

        // allocate an object (do not call the constructor)
        ObjectType *allocate()
        {
          if (!first_chunk)
          {
            TRACY_PLOT_CONFIG(pool_debug_name.data(), tracy::PlotFormatType::Memory);
            if (!alloc_chunk())
              return nullptr;
          }

          allocation_slot *slot = nullptr;

          // first: try to re-use previous allocation in the free list
          if (first_free)
          {
            allocation_slot *slot = first_free;
            first_free = slot->next;
          }
          else // try to grab something in the first chunk
          {
            if (first_chunk->direct_alloc_offset == ObjectCount)
            {
              if (!alloc_chunk())
                return nullptr;
            }
            if (first_chunk->direct_alloc_offset < ObjectCount)
            {
              slot = &first_chunk->data[first_chunk->direct_alloc_offset];
              first_chunk->direct_alloc_offset++;

              // setup the slot for first usage:
              slot->set_chunk(first_chunk);
            }
          }


          slot->set_free(false);
          ++(slot->get_chunk()->object_count);
          ++object_count;
          TRACY_PLOT(pool_debug_name.data(), (int64_t)object_count);
          return &slot->obj;
        }

        // deallocate an object
        // the object **MUST** be allocated by the pool (or nullptr)
        void deallocate(ObjectType *p)
        {
          if (p)
          {
            // get slot and chunk
            allocation_slot *slot = allocation_slot::get_slot(p);

            slot->set_free(true);
            --slot->get_chunk()->object_count;
            --object_count;
            TRACY_PLOT(pool_debug_name.data(), (int64_t)object_count);

            slot->next = first_free;
            first_free = slot;
          }
        }

        // gp getters
        size_t get_number_of_chunks() const
        {
          return chunk_count;
        }

        size_t get_number_of_object() const
        {
          return object_count;
        }

      public: // debug
        std::string pool_debug_name;

      private:
        bool alloc_chunk()
        {
          chunk* chk = reinterpret_cast<chunk*>(operator new (k_allocation_size));

          if (!chk)
            return false;

          // setup the chunk
          memset((void*)chk, 0, k_allocation_size);
          chk->next = first_chunk;
          chk->prev = nullptr;

          // push free slots
          first_chunk = chk;
          ++chunk_count;
          return true;
        }

      private:
        size_t object_count;
        size_t chunk_count;

        chunk *first_chunk;
        allocation_slot *first_free;
    };

  } // namespace cr
} // namespace neam

template<typename ObjectType, size_t ObjectCount>
void *operator new(size_t, neam::cr::memory_pool<ObjectType, ObjectCount> &pool)
{
  return pool.allocate();
}
template<typename ObjectType, size_t ObjectCount>
void operator delete(void *ptr, neam::cr::memory_pool<ObjectType, ObjectCount> &pool)
{
  pool.deallocate(reinterpret_cast<ObjectType *>(ptr));
}

#endif /*__N_2080156092666096177_2010861593__MEMORY_POOL_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

