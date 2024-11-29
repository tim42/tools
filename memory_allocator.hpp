//
// file : memory_allocator.hpp
// in : file:///home/tim/projects/persistence/persistence/tools/memory_allocator.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 22/09/2014 11:00:31
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

#pragma once
# define __N_2080522430493415542_1720253011__MEMORY_ALLOCATOR_HPP__

#include <cstdlib>
#include <new>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "raw_ptr.hpp"
#include "raw_data.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief a chunked memory pool that can produce contiguous output.
    /// \note if an allocation size is < sizeof(uint64_t), then you could assume the allocation will never fail:
    ///       if the allocation fails, the function return a pointer to a valid area (an internal field) and set the \e failed flag to true.
    ///       so allocation of very small size will alway give a valid pointer.
    class memory_allocator
    {
      public:
        /// \brief default constructor
        memory_allocator()
        {
          failed = false;
        }

        /// \brief Move constructor
        memory_allocator(memory_allocator &&o) = default;

        /// \brief Move/affectation operator
        memory_allocator &operator = (memory_allocator &&o) = default;

        ~memory_allocator()
        {
          clear();
        }

        /// \brief return whether or not an allocation has failed.
        bool has_failed() const
        {
          return failed;
        }

        /// \brief unset the failed flag
        void clear_failed()
        {
          failed = false;
        }

        /// \brief allocate \e count bytes at the end of the pool
        /// \return \b nullptr if allocation failed.
        void* allocate(size_t count)
        {
          if (!first || last->end_offset + count > last->data.size) // allocate a chunk (no chunk or no place in the last chunk)
          {
            memory_chunk* nchk = new (std::nothrow) memory_chunk;
            if (nchk)
            {
              size_t size = count > chunk_size ? count : chunk_size;
              nchk->data = raw_data::allocate(size);
            }
            if (!nchk || !nchk->data)
            {
              failed = true;
              delete nchk;

              // never fail for small allocations
              if (count <= sizeof(fallback_small))
                return &fallback_small;
              return nullptr;
            }

            nchk->end_offset = count;
            if (first)
            {
              last->next = nchk;
              last = nchk;
            }
            else
            {
              first = nchk;
              last = nchk;
            }
            pool_size += count;
            return nchk->data;
          }

          // there is enough room in the last chunk
          void* data = last->data.get_as<uint8_t>() + last->end_offset;
          last->end_offset += count;
          pool_size += count;
          return data;
        }

        /// \brief makes sure there will be enough contiguous space.
        /// Don't mark memory as allocated, but can skip the end of a chunk if there isn't enough space.
        /// \note doesn't set the failed flag is any memory allocation failure occurs.
        bool preallocate_contiguous(size_t count)
        {
          if (!first || last->end_offset + count > last->data.size) // allocate a chunk (no chunk or no place in the last chunk)
          {
            memory_chunk* nchk = new (std::nothrow) memory_chunk;
            if (nchk)
            {
              size_t size = count > chunk_size ? count : chunk_size;
              nchk->data = raw_data::allocate(size);
            }
            if (!nchk || !nchk->data)
            {
              delete nchk;

              return false;
            }

            nchk->end_offset = 0;
            if (first)
            {
              last->next = nchk;
              last = nchk;
            }
            else
            {
              first = nchk;
              last = nchk;
            }
          }
          return true;
        }

        /// \brief Check if the data is already contiguous
        bool is_data_contiguous() const
        {
          if (!first)
            return true;
          if (!first->next) // contiguous
            return true;
          return false;
        }

        /// \brief make the data contiguous.
        /// \note don't free the memory !!!
        const raw_data& get_contiguous_data()
        {
          // easy cases:
          if (!first)
            return fallback_data;
          if (!first->next) // already contiguous
            return first->data;

          memory_chunk* new_chk = new memory_chunk;
          new_chk->data = raw_data::allocate(pool_size);

          new_chk->end_offset = pool_size;

          size_t idx = 0;
          memory_chunk* next = nullptr;
          for (memory_chunk* chr = first.release(); chr; chr = next)
          {
            memcpy(new_chk->data.get_as<uint8_t>() + idx, chr->data.get(), chr->end_offset);
            idx += chr->end_offset;
            next = chr->next;
            delete chr;
          }

          first = new_chk;
          last = new_chk;
          return new_chk->data;
        }

        /// \brief Like get_contiguous_data() but does not clear the memory_allocator
        raw_data get_contiguous_data_copy() const
        {
          raw_data ret = raw_data::allocate(pool_size);
          uint8_t* data = ret.get_as<uint8_t>();

          size_t idx = 0;
          memory_chunk* next = nullptr;
          for (memory_chunk* chr = first; chr; chr = next)
          {
            memcpy(data + idx, chr->data.get(), chr->end_offset);
            idx += chr->end_offset;
            next = chr->next;
          }

          return ret;
        }

        /// \brief give the the data ownership, return the pointer to the data and clear the pool
        /// \see get_contiguous_data()
        /// \see clear()
        raw_data give_up_data()
        {
          get_contiguous_data();
          raw_data ret;
          if (first)
          {
            ret = std::move(first->data);
            ret.size = pool_size; // necessary as we might have single, normally allocated chunk
          }
          clear();
          return ret;
        }

        /// \brief empty the memory pool, delete every allocated memory.
        void clear()
        {
          memory_chunk* next = nullptr;
          for (memory_chunk* chr = first.release(); chr; chr = next)
          {
            next = chr->next;
            delete chr;
          }
          last = nullptr;
          pool_size = 0;
          failed = false;
        }

        /// \brief return the size of the pool
        size_t size() const
        {
          return pool_size;
        }

        /// \brief returns the current pointer.
        void* _here()
        {
          if (!last)
            return nullptr;
          return last->data.get_as<uint8_t>() + last->end_offset;
        }

      private:
        struct memory_chunk
        {
          raw_data data;

          size_t end_offset = 0;

          memory_chunk* next = nullptr;
        };

      private:
        constexpr static size_t chunk_size = 8192 * 10; // 10 * 8Kio per-chunk

      private:
        raw_ptr<memory_chunk> first = nullptr;
        memory_chunk* last = nullptr;
        size_t pool_size = 0;
        bool failed = false;
        raw_data fallback_data;
        uint64_t fallback_small = 0; ///< \brief used when running out of memory and if the allocated size is lower than
    };
  } // namespace cr
} // namespace neam



// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

