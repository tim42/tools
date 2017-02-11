//
// file : uninitialized.hpp
// in : file:///home/tim/projects/nblockchain/nblockchain/persistence/persistence/tools/uninitialized.hpp
//
// created by : Timothée Feuillet
// date: Fri Jul 08 2016 23:31:39 GMT+0200 (CEST)
//
//
// Copyright (c) 2016 Timothée Feuillet
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

#ifndef __N_177204756238143312_155459372_UNINITIALIZED_HPP__
#define __N_177204756238143312_155459372_UNINITIALIZED_HPP__

#include <cstdint>

namespace neam
{
  namespace cr
  {
    /// \brief Create an uninitialized storage that yields a given type.
    /// This allows to create uninitialized objects on the stack or as class/struct members
    template<typename ObjectType>
    class uninitialized
    {
      public:
        ObjectType *operator ->()
        {
          return ref;
        }

        const ObjectType *operator ->() const
        {
          return ref;
        }

        ObjectType *operator &()
        {
          return ref;
        }

        const ObjectType *operator &() const
        {
          return ref;
        }

        operator ObjectType &()
        {
          return *ref;
        }

        operator const ObjectType &() const
        {
          return ref;
        }

        ObjectType &get()
        {
          return *ref;
        }

        const ObjectType &get() const
        {
          return *ref;
        }

        /// \brief if call_it is true, it will call the destructor of the object
        /// when the life of the instance ends. The default is to NOT call the destructor
        void call_destructor(bool call_it = true)
        {
          do_call_destructor = call_it;
        }

        /// \brief construct the object
        template<typename... Args>
        void construct(Args... args)
        {
          new (&this->get()) ObjectType(std::forward<Args>(args)...);
          do_call_destructor = true;
        }

        ~uninitialized()
        {
          if (do_call_destructor)
            this->get().~ObjectType();
        }

        using object_type = ObjectType;
      private:
        bool do_call_destructor = false;
        uint8_t storage[sizeof(ObjectType)];
        ObjectType *const ref = (ObjectType *)(storage);
    };
  } // namespace cr
} // namespace neam

#endif // __N_177204756238143312_155459372_UNINITIALIZED_HPP__

