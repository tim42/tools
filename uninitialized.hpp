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
#include <memory>

namespace neam
{
  namespace cr
  {
    /// \brief Create an uninitialized storage that yields a given type.
    /// This allows to create uninitialized objects on the stack or as class/struct members
    /// \warning The default behavior is to not call the destructor at the end of the life of uninitialized<>
    template<typename ObjectType>
    class uninitialized
    {
      private:
        static constexpr uint8_t is_constructed_bit = 1 << 0;
        static constexpr uint8_t destructor_call_bit = 1 << 1;
        static constexpr uint8_t is_constructed_mask = ~is_constructed_bit;
        static constexpr uint8_t destructor_call_mask = ~destructor_call_bit;

      public:
        ObjectType *operator ->() noexcept { return (ObjectType *)(storage); }

        const ObjectType *operator ->() const noexcept { return (ObjectType *)(storage); }

        ObjectType *operator &() { return (ObjectType *)(storage); }

        const ObjectType *operator &() const noexcept { return (ObjectType *)(storage); }

        operator ObjectType &() & noexcept { return *(ObjectType *)(storage); }
        operator ObjectType &&() && noexcept { return *(ObjectType *)(storage); }

        operator const ObjectType &() const & noexcept { return (ObjectType *)(storage); }
        operator const ObjectType &&() const && noexcept { return (ObjectType *)(storage); }

        ObjectType &get() & noexcept { return *(ObjectType *)(storage); }
        ObjectType &&get() && noexcept { return *(ObjectType *)(storage); }
        const ObjectType &get() const & noexcept { return *(ObjectType *)(storage); }
        const ObjectType &&get() const && noexcept { return *(ObjectType *)(storage); }

        /// \brief if call_it is true, it will call the destructor of the object
        /// when the life of the instance ends. The default is to NOT call the destructor
        void schedule_destructor_call(bool call_it = true) noexcept
        {
          storage[sizeof(ObjectType)] =
              (storage[sizeof(ObjectType)] & destructor_call_mask)
            | (call_it ? destructor_call_bit : 0);
        }

        /// \brief Return whether or not the object is constructed
        bool is_constructed() const noexcept
        {
          return storage[sizeof(ObjectType)] & is_constructed_bit;
        }

        /// \brief construct the object
        /// \warning Does not call the destructor if the object has previously been constructed
        template<typename... Args>
        void construct(Args &&... args) noexcept(noexcept(ObjectType(std::forward<Args>(args)...)))
        {
          new (&this->get()) ObjectType(std::forward<Args>(args)...);
          storage[sizeof(ObjectType)] |= is_constructed_bit;
        }

        /// \brief Change the constructed flag of the object
        void mark_as_constructed(bool constructed) noexcept
        {
          if (constructed)
            storage[sizeof(ObjectType)] |= is_constructed_bit;
          else
            storage[sizeof(ObjectType)] &= is_constructed_mask;
        }

        /// \brief call the destructor
        /// \param force Force a call to the destructor, even if the object is marked as not-created
        void destruct(bool force = false) noexcept(noexcept(~ObjectType()))
        {
          if (force || is_constructed())
          {
            this->get().~ObjectType();
            storage[sizeof(ObjectType)] &= is_constructed_mask;
          }
        }

        ~uninitialized() noexcept(noexcept(~ObjectType()))
        {
          if ((storage[sizeof(ObjectType)] & (is_constructed_bit | destructor_call_bit)) == (is_constructed_bit | destructor_call_bit))
            destruct();
        }

        uninitialized() noexcept = default;

        uninitialized(ObjectType&& o) noexcept(noexcept(construct(std::move(o)))) { construct(std::move(o)); }
        uninitialized(const ObjectType& o) noexcept(noexcept(construct(o))) { construct(o); }

        template<typename Arg>
        uninitialized& operator = (Arg&& a) noexcept(noexcept(get() = std::forward<Arg>(a)) && noexcept(construct(std::forward<Arg>(a))))
        {
          if (is_constructed())
            get() = std::forward<Arg>(a);
          else
            construct(std::forward<Arg>(a));
        }

        using object_type = ObjectType;
      private:
        alignas(ObjectType) uint8_t storage[1 + sizeof(ObjectType)];
    };
  } // namespace cr
} // namespace neam

#endif // __N_177204756238143312_155459372_UNINITIALIZED_HPP__

