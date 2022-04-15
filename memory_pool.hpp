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
#include "raw_memory_pool_ts.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief this class provide a fast and easy way to deal with 
    /// \tparam ObjectType is the type of object stored in the pool
    /// \tparam ObjectCount is the number of object per chunk.
    template<typename ObjectType, typename UnderlyingPool = raw_memory_pool_ts, size_t PageCount = 4>
    class memory_pool
    {
      public:
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

        // allocate an object (the constructor is not called)
        ObjectType *allocate()
        {
          void* ptr = pool.allocate();
          return (ObjectType*)ptr;
        }

        // deallocate an object
        // the object **MUST** be allocated by the pool (or nullptr)
        void deallocate(ObjectType *p)
        {
          return pool.deallocate(p);
        }

        // gp getters
        size_t get_number_of_chunks() const
        {
          return pool.get_number_of_chunks();
        }

        size_t get_number_of_object() const
        {
          return pool.get_number_of_object();
        }

      public: // debug
        std::string pool_debug_name;

      private:
        UnderlyingPool pool {sizeof(ObjectType), alignof(ObjectType), PageCount};
    };

  } // namespace cr
} // namespace neam

template<typename ObjectType, typename UnderlyingPool, size_t PageCount>
void *operator new(size_t, neam::cr::memory_pool<ObjectType, UnderlyingPool, PageCount> &pool)
{
  return pool.allocate();
}
template<typename ObjectType, typename UnderlyingPool, size_t PageCount>
void operator delete(void *ptr, neam::cr::memory_pool<ObjectType, UnderlyingPool, PageCount> &pool)
{
  pool.deallocate(reinterpret_cast<ObjectType *>(ptr));
}

#endif /*__N_2080156092666096177_2010861593__MEMORY_POOL_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

