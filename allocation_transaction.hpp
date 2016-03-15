//
// file : allocation_transaction.hpp
// in : file:///home/tim/projects/persistence/persistence/tools/allocation_transaction.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: 08/01/2016 17:26:11
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

#ifndef __N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__
# define __N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__

#include <list>
#include <type_traits>
#include "memory_pool.hpp"

//#define ALLOCATION_TRANSACTION_USE_POOL

namespace neam
{
  namespace cr
  {
    /// \brief Holds some memory. Useful for transactions and complete rollbacks on allocations.
    /// \note this is mostly done for neam::persistence and most of the methods find their utilities in persistence.
    class allocation_transaction
    {
      public:
        allocation_transaction() {}
        ~allocation_transaction() {rollback();}

        /// \brief Free all previously allocated memory in the reverse order (last allocations first)
        void rollback()
        {
          for (auto &it: allocation_list)
          {
            it->rollback();
#ifndef ALLOCATION_TRANSACTION_USE_POOL
            delete it;
#endif
          }
#ifdef ALLOCATION_TRANSACTION_USE_POOL
          pool.clear();
#endif
          allocation_list.clear();
        }

        /// \brief Let the memory allocated by the transactions live its life
        /// This have to be called when the transaction is done with success
        void complete()
        {
#ifndef ALLOCATION_TRANSACTION_USE_POOL
          for (auto &it: allocation_list)
            delete it;
#else // defined(ALLOCATION_TRANSACTION_USE_POOL)
          pool.clear();
#endif
          allocation_list.clear();
        }

        /// \brief Allocate some raw memory, return nullptr on failure
        void *allocate_raw(size_t size)
        {
          void *ptr = operator new(size, std::nothrow);
          if (ptr)
          {
#ifdef ALLOCATION_TRANSACTION_USE_POOL
            single_allocation_spec<uint8_t> *aptr = pool.allocate();
            allocation_list.push_front(pool.construct(aptr, reinterpret_cast<uint8_t *>(ptr), false));
#else
            allocation_list.push_front(new single_allocation_spec<uint8_t>(reinterpret_cast<uint8_t *>(ptr), false));
#endif
          }
          return ptr;
        }

        /// \brief Allocate some raw memory, return nullptr on failure.
        /// This is simply an heler for the other allocate_raw() method that allocate directly the size of an object
        /// \note unlike allocate() the destructor won't be called
        template<typename Type>
        Type *allocate_raw()
        {
          return reinterpret_cast<Type *>(allocate_raw(sizeof(Type)));
        }

        /// \brief Allocate some memory for an object, return nullptr on failure
        /// \note DOES NOT CALL THE CONSTRUCTOR. (rollback \e WILL call the destructor)
        template<typename Type>
        Type *allocate()
        {
          Type *ptr = reinterpret_cast<Type *>(operator new(sizeof(Type), std::nothrow));
          if (ptr)
          {
#ifdef ALLOCATION_TRANSACTION_USE_POOL
            single_allocation_spec<Type> *aptr = reinterpret_cast<single_allocation_spec<Type> *>(pool.allocate());
            new (aptr) single_allocation_spec<Type>(ptr, std::is_array<Type>::value);
            allocation_list.push_front(aptr);
#else
            allocation_list.push_front(new single_allocation_spec<Type>(ptr, std::is_array<Type>::value));
#endif
          }
          return ptr;
        }

        /// \brief Register a destructor to be called in case of rollback
        template<typename Type>
        void register_destructor_call_on_failure(Type *ptr)
        {
#ifdef ALLOCATION_TRANSACTION_USE_POOL
          single_allocation_spec<Type> *aptr = reinterpret_cast<single_allocation_spec<Type> *>(pool.allocate());
          new(aptr) single_allocation_spec<Type>(ptr, std::is_array<Type>::value, false);
          allocation_list.push_front(aptr);
#else
            allocation_list.push_front(new single_allocation_spec<Type>(ptr, std::is_array<Type>::value, false));
#endif
        }

      private:
        class single_allocation
        {
          public:
            virtual ~single_allocation() {}
            virtual void rollback() = 0;
        };

        template<typename Type>
        class single_allocation_spec : public single_allocation
        {
          public:
            single_allocation_spec(Type *_ptr, bool _is_array = false, bool _do_delte = true) : ptr(_ptr), is_array(_is_array), do_delete(_do_delte) {}
            virtual ~single_allocation_spec() {}

            virtual void rollback()
            {
              if (do_delete)
              {
                if (is_array)
                  delete [] ptr;
                else
                  delete ptr;
              }
              else
              {
                ptr->~Type();
              }
            }
          private:
            Type *ptr;
            bool is_array;
            bool do_delete;
        };

      private:
        std::list<single_allocation *> allocation_list;
#ifdef ALLOCATION_TRANSACTION_USE_POOL
        memory_pool<single_allocation_spec<uint8_t>> pool;
#endif
    };
  } // namespace cr
} // namespace neam

#endif /*__N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

