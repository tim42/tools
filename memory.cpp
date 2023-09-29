//
// created by : Timothée Feuillet
// date: 2022-3-18
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

#include "memory.hpp"

#include <atomic>
#include <cstring>

#ifdef __unix__
  #include <unistd.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <sys/mman.h>
  #ifdef HAS_LIBHUGETLBFS
    #include <hugetlbfs.h>
  #endif
#elif defined(_WIN32)
  #include <windows.h>
  #include <memoryapi.h>

  #warning "untested and uncompiled code"
#endif

namespace neam::memory
{
  static uint64_t get_page_size_direct()
  {
  #ifdef __unix__
    return sysconf(_SC_PAGE_SIZE);
  #elif defined(_WIN32)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwPageSize;
  #else
    return 4096; // unsupported
  #endif
  }

  namespace cache
  {
    static constexpr bool k_enable_cache = false;
    static constexpr uint32_t k_max_page_count = 32;
    static constexpr uint32_t k_max_page_per_allocation = 6;

    struct cache_entry_t
    {
      std::atomic<void*> pages[k_max_page_count] = {nullptr};
    };

    struct cache_t
    {
      cache_entry_t caches[k_max_page_per_allocation];
    };

    static cache_t& get_caches()
    {
      static cache_t cache;
      return cache;
    }

    static bool add_to_cache(void* page, uint32_t page_count)
    {
      if constexpr (!k_enable_cache) return false;

      [[likely]] if (page_count < cache::k_max_page_per_allocation)
      {
        cache_entry_t& cache = get_caches().caches[page_count];
        for (uint32_t i = 0; i < k_max_page_count; ++i)
        {
          void* rqpage = nullptr;
          if (cache.pages[i].compare_exchange_weak(rqpage, page, std::memory_order_acq_rel))
            return true;
        }
      }
      return false;
    }

    static void* get_from_cache(uint32_t page_count)
    {
      if constexpr (!k_enable_cache) return nullptr;

      [[likely]] if (page_count < cache::k_max_page_per_allocation)
      {
        cache_entry_t& cache = get_caches().caches[page_count];
        for (uint32_t i = 0; i < k_max_page_count; ++i)
        {
          void* page_ptr = cache.pages[i].exchange(nullptr, std::memory_order_acq_rel);
          if (page_ptr)
          {
            memset(page_ptr, 0, page_count * get_page_size());
            return page_ptr;
          }
        }
      }
      return nullptr;
    }
  }

  uint64_t get_page_size()
  {
    // avoid making multiple calls (syscalls?) when we can hold the result and re-use it instead
    static const uint64_t size = get_page_size_direct();
    return size;
  }


  void* allocate_page(uint32_t page_count, bool use_pool)
  {
    [[likely]] if (use_pool)
    {
      void* ptr = cache::get_from_cache(page_count);
      if (ptr != nullptr)
        return ptr;
    }

  #ifdef __unix__
    void *ptr = nullptr;

    ptr = mmap(nullptr, get_page_size() * page_count, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
                  -1, 0);

    [[unlikely]] if (ptr == MAP_FAILED)
    {
      perror("mmap");
      printf("trying to allocate: %lu bytes\n", get_page_size()*page_count);
      return nullptr;
    }

    return ptr;
  #elif defined(_WIN32)
    void* ptr = nullptr;
    return VirtualAlloc(nullptr, get_page_size() * page_count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  #else
    return aligned_alloc(get_page_size(), get_page_size() * page_count);
  #endif
  }

  void free_page(void* page_ptr, uint32_t page_count, bool use_pool)
  {
    [[unlikely]] if (page_ptr == nullptr) return;

    [[likely]] if (use_pool)
    {
      if (cache::add_to_cache(page_ptr, page_count))
        return;
    }

  #ifdef __unix__
    munmap(page_ptr, get_page_size() * page_count);
  #elif defined(_WIN32)
    VirtualFree(page_ptr, 0, MEM_RELEASE);
  #else
    free(page_ptr);
  #endif
  }
}

