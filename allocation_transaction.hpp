//
// file : allocation_transaction.hpp
// in : file:///home/tim/projects/persistence/persistence/tools/allocation_transaction.hpp
//
// created by : Timothée Feuillet on linux-vnd3.site
// date: 08/01/2016 17:26:11
//
//
// Copyright (C) 2014 Timothée Feuillet
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#ifndef __N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__
# define __N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__

#include <list>
#include <type_traits>

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
            delete it;
          }
          allocation_list.clear();
        }

        /// \brief Let the memory allocated by the transactions live its life
        /// This have to be called when the transaction is done with success
        void complete()
        {
          for (auto &it: allocation_list)
            delete it;
          allocation_list.clear();
        }

        /// \brief Allocate some raw memory, return nullptr on failure
        void *allocate_raw(size_t size)
        {
          void *ptr = operator new(size, std::nothrow);
          if (ptr)
            allocation_list.push_front(new single_allocation_spec<uint8_t>(reinterpret_cast<uint8_t *>(ptr), false));
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
            allocation_list.push_front(new single_allocation_spec<Type>(ptr, false));
          return ptr;
        }

        /// \brief Register a destructor to be called in case of rollback
        template<typename Type>
        void register_destructor_call_on_failure(Type *ptr)
        {
          allocation_list.push_front(new single_allocation_spec<Type>(ptr, std::is_array<Type>::value, false));
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
    };
  } // namespace cr
} // namespace neam

#endif /*__N_1396592720261257067_1178544939__ALLOCATION_TRANSACTION_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

