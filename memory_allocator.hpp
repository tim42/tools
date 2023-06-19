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

        /// \brief a sub-memory_allocator: manage an allocated area of another memory_allocator
        /// \param[in,out] fallback is used when the allocated memory is greater than the \p size of the \p area
        ///                is nullptr, when the user request an allocation of size greater than the remaining area space, the allocation will fail.
        memory_allocator(void *area, size_t size, memory_allocator *fallback, bool unstack_fallbacks = true)

        {
          if (unstack_fallbacks && fallback)
          {
            while (fallback->fallback_allocator)
              fallback = fallback->fallback_allocator;
          }
          fallback_allocator = fallback;

          first = new memory_chunk;
          last = first;
          first->data = reinterpret_cast<uint8_t *>(area);
          first->size = size;
          first->do_not_destroy = true;

          use_fallback = true;
          failed = false;
        }

        /// \brief Move constructor
        memory_allocator(memory_allocator &&o)
         : first(o.first), last(o.last), pool_size(o.pool_size), failed(o.failed),
           use_fallback(o.use_fallback), fallback_allocator(o.fallback_allocator),
           full_allocator(o.full_allocator)
        {
          o.first = nullptr;
          o.last = nullptr;
          o.pool_size = 0;
          o.failed = true; // Set the failed flag. Just in case.
          o.fallback_allocator = nullptr;
        }

        /// \brief Move/affectation operator
        memory_allocator &operator = (memory_allocator &&o)
        {
          if (&o == this)
            return *this;

          clear();
          first = o.first;
          last = o.last;
          pool_size = o.pool_size;
          failed = o.failed;
          use_fallback = o.use_fallback;
          fallback_allocator = o.fallback_allocator;
          full_allocator = o.full_allocator;

          o.first = nullptr;
          o.last = nullptr;
          o.pool_size = 0;
          o.failed = true; // Set the failed flag. Just in case.
          o.fallback_allocator = nullptr;
          return *this;
        }

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
        void *allocate(size_t count)
        {
          if (!first || last->end_offset + count > last->size) // allocate a chunk (no chunk or no place in the last chunk)
          {
            if (use_fallback) // shortcut here when we have to use the fallback
            {
              last->end_offset = last->size; // make sure we won't allocate anything else.
              if (!fallback_allocator)
              {
                failed = true;
                if (count <= sizeof(fallback_small))
                  return &fallback_small;
                return nullptr;
              }

              void *ptr = fallback_allocator->allocate(count);
              if (!ptr)
              {
                failed = true;
                return nullptr;
              }
              return ptr;
            }

            memory_chunk *nchk = new(std::nothrow) memory_chunk;
            if (nchk)
            {
              nchk->size = count > chunk_size ? count : chunk_size;
              nchk->data = reinterpret_cast<uint8_t *>(operator new(nchk->size, std::nothrow));
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
              nchk->prev = last;
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
          void *data = last->data + last->end_offset;
          last->end_offset += count;
          pool_size += count;
          return data;
        }

        /// \brief allocate \e count bytes at the front of the pool
        /// \return \b nullptr if allocation failed.
        void *allocate_front(size_t count)
        {
          if (!first || first->start_offset < count) // allocate a chunk (no chunk or no place left in the first chunk)
          {
            if (use_fallback) // shortcut here when we have to use the fallback
            {
              failed = false;
              return nullptr;
            }

            memory_chunk *nchk = new(std::nothrow) memory_chunk;
            if (nchk)
            {
              nchk->size = count > chunk_size ? count : chunk_size;
              nchk->data = reinterpret_cast<uint8_t *>(operator new(nchk->size, std::nothrow));
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

            nchk->end_offset = nchk->size;
            nchk->start_offset = count;
            if (first)
            {
              first->prev = nchk;
              nchk->next = first;
              first = nchk;
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
          first->start_offset -= count;
          pool_size += count;
          return first->data + first->start_offset;
        }

        /// \brief remove \e count bytes at the end of the pool
        void pop(size_t count)
        {
          if (!last)
            return;
          if (count >= pool_size)
            return clear();

          while (count)
          {
            if (last != first && (last->end_offset - last->start_offset) < count)
            {
              pool_size -= last->end_offset - last->start_offset;
              count -= last->end_offset - last->start_offset;
              last->prev->next = nullptr;
              memory_chunk *lst = last;
              last = last->prev;
              delete lst;
            }
            else if (last == first && (last->end_offset - last->start_offset) < count)
            {
              if (use_fallback)
              {
                full_allocator = false;
                first->end_offset = 0;
              }
              else
              {
                delete first;
                first = nullptr;
                last = nullptr;
              }
              pool_size = 0;
              return;
            }
            else
            {
              last->end_offset -= count;
              pool_size -= count;
              return;
            }
          }
        }

        /// \brief remove \e count bytes at the start of the pool
        void pop_front(size_t count)
        {
          if (!first || !pool_size)
            return;
          if (count >= pool_size)
            return clear();

          while (count)
          {
            if (last != first && (first->end_offset - first->start_offset) < count)
            {
              pool_size -= first->end_offset - first->start_offset;
              count -= first->end_offset - first->start_offset;
              first->prev->next = nullptr;
              memory_chunk *lst = first;
              first = first->prev;
              delete lst;
            }
            else if (last == first && (last->end_offset - last->start_offset) < count)
            {
              delete first;
              first = nullptr;
              last = nullptr;
              pool_size = 0;
              return;
            }
            else
            {
              first->start_offset += count;
              pool_size -= count;
              return;
            }
          }
        }

        /// \brief makes sure there will be enough contiguous space.
        /// Don't mark memory as allocated, but can skip the end of a chunk if there isn't enough space.
        /// \note doesn't set the failed flag is any memory allocation failure occurs.
        bool preallocate_contiguous(size_t count)
        {
          if (use_fallback)
          {
            if (last->end_offset + count <= last->size) // already preallocated, already contiguous
              return true;
            else // use the fallback, mark the buffer as full, _here will return fallback's _here.
            {
              if (!fallback_allocator)
                return false;
              last->end_offset = last->size; // make it full (no further allocations will takes place on this allocator)
              full_allocator = true;
              return fallback_allocator->preallocate_contiguous(count);
            }
          }

          if (!first || last->end_offset + count > last->size) // allocate a chunk (no chunk or no place in the last chunk)
          {
            memory_chunk *nchk = new(std::nothrow) memory_chunk;
            if (nchk)
            {
              nchk->size = count > chunk_size ? count : chunk_size;
              nchk->data = reinterpret_cast<uint8_t *>(operator new(nchk->size, std::nothrow));
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
              nchk->prev = last;
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
          if (!first->next && first->start_offset == 0) // already contiguous
            return true;
          return false;
        }

        /// \brief make the data contiguous.
        /// \note don't free the memory !!!
        void *get_contiguous_data()
        {
          // easy cases:
          if (!first)
            return nullptr;
          if (!first->next && first->start_offset == 0) // already contiguous
            return first->data;

          memory_chunk *new_chk = new memory_chunk;
          new_chk->data = reinterpret_cast<uint8_t *>(operator new(pool_size, std::nothrow));
          if (!new_chk->data)
          {
            delete new_chk;
            return nullptr;
          }

          new_chk->size = pool_size;
          new_chk->start_offset = 0;
          new_chk->end_offset = pool_size;

          size_t idx = 0;
          memory_chunk *next = nullptr;
          for (memory_chunk *chr = first; chr; chr = next)
          {
            memcpy(new_chk->data + idx, chr->data + chr->start_offset, chr->end_offset - chr->start_offset);
            idx += chr->end_offset - chr->start_offset;
            next = chr->next;
            delete chr;
          }

          first = new_chk;
          last = new_chk;
          return new_chk->data;
        }

        /// \brief Like get_contiguous_data() but does not clear the memory_allocator
        /// \note As you have the ownership of the memory, it's your duty to free it
        void *get_contiguous_data_copy() const
        {
          uint8_t *data = reinterpret_cast<uint8_t *>(operator new(pool_size, std::nothrow));
          if (!data)
            return nullptr;

          size_t idx = 0;
          memory_chunk *next = nullptr;
          for (memory_chunk *chr = first; chr; chr = next)
          {
            memcpy(data + idx, chr->data + chr->start_offset, chr->end_offset - chr->start_offset);
            idx += chr->end_offset - chr->start_offset;
            next = chr->next;
            delete chr;
          }

          return data;
        }

        /// \brief give the the data ownership, return the pointer to the data and clear the pool
        /// \note it's up to the user to delete this pointer.
        /// \see get_contiguous_data()
        /// \see clear()
        void *give_up_data()
        {
          void *ret = get_contiguous_data();
          if (first)
            first->data = nullptr;
          clear();
          return ret;
        }

        /// \brief empty the memory pool, delete every allocated memory.
        void clear()
        {
          memory_chunk *next = nullptr;
          for (memory_chunk *chr = first; chr; chr = next)
          {
            next = chr->next;
            delete chr;
          }
          first = nullptr;
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
        void *_here()
        {
          if (full_allocator)
          {
            if (fallback_allocator)
              return fallback_allocator->_here();
          }

          if (!last)
            return nullptr;
          return last->data + last->end_offset;
        }

      private:
        struct memory_chunk
        {
          uint8_t *data = nullptr;

          size_t start_offset = 0;
          size_t end_offset = 0;
          size_t size = 0;

          bool do_not_destroy = false;

          memory_chunk *next = nullptr;
          memory_chunk *prev = nullptr;

          ~memory_chunk()
          {
            if (!do_not_destroy)
              delete data;
          }
        };

      private:
        constexpr static size_t chunk_size = 8192 * 10; // 10 * 8Kio per-chunk

      private:
        memory_chunk *first = nullptr;
        memory_chunk *last = nullptr;
        size_t pool_size = 0;
        bool failed = false;
        uint64_t fallback_small; ///< \brief used when running out of memory and if the allocated size is lower than

        bool use_fallback = false;
        memory_allocator *fallback_allocator = nullptr;
        bool full_allocator = false; ///< \brief _here will return fallback_allocator->_here() or nullptr.
    };
  } // namespace cr
} // namespace neam



// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

