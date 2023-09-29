//
// created by : Timothée Feuillet
// date: 2023-9-16
//
//
// Copyright (c) 2023 Timothée Feuillet
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

#include "../raw_data.hpp"

namespace neam::rpc
{
  class adapter_base
  {
    public:
      virtual ~adapter_base() = default;

      /// \brief Return the size of the header to reserve in the raw-data (avoid data duplication and copies)
      virtual size_t get_header_size() { return 0; }
      /// \brief Return the size of the footer to reserve in the raw-data (avoid data duplication and copies)
      virtual size_t get_footer_size() { return 0; }

      /// \brief Called when a remote call is performed. Can be called on multiple threads (no synchronisation is done).
      /// \note This call should initialize the header / footer, as the data is allocated but not initialized.
      virtual void dispatch_call(raw_data&& rd) = 0;
  };

  template<typename Child>
  class basic_adapter : public adapter_base
  {
    protected: // default values, override in child if needed
      using footer_t = void;
      using header_t = void;
      static constexpr bool default_initialize_header = false;
      static constexpr bool default_initialize_footer = false;

      // API:
      // void init_rpc_header(header_t& ft, const raw_data&) {}
      // void init_rpc_footer(footer_t& ft, const raw_data&) {}
      // void send_rpc(raw_data&& rd) {}

    public:
      size_t get_header_size() final override
      {
        if constexpr (std::is_same_v<void, typename Child::header_t>)
          return 0;
        else
          return sizeof(typename Child::header_t);
      }

      size_t get_footer_size() final override
      {
        if constexpr (std::is_same_v<void, typename Child::footer_t>)
          return 0;
        else
          return sizeof(typename Child::footer_t);
      }

      void dispatch_call(raw_data&& rd) final override
      {
        if constexpr (!std::is_same_v<void, typename Child::header_t> && !Child::default_initialize_header)
          static_cast<Child*>(this)->init_rpc_header(*(typename Child::header_t*)rd.get(), rd);
        else if constexpr (!std::is_same_v<void, typename Child::header_t> && Child::default_initialize_header)
          new((typename Child::header_t*)rd.get()) typename Child::header_t{};

        if constexpr (!std::is_same_v<void, typename Child::footer_t> && !Child::default_initialize_footer)
          static_cast<Child*>(this)->init_rpc_footer(*(typename Child::footer_t*)((uint8_t*)rd.get() + rd.size - sizeof(typename Child::footer_t)), rd);
        else if constexpr (!std::is_same_v<void, typename Child::footer_t> && Child::default_initialize_footer)
          new((typename Child::footer_t*)rd.get()) typename Child::footer_t{};

        static_cast<Child*>(this)->send_rpc(std::move(rd));
      }
  };

  namespace internal
  {
    adapter_base* get_current_adapter();
    void set_current_adapter(adapter_base* ab);
  }

  struct scoped_adapter
  {
    scoped_adapter(adapter_base& ad)
    {
      previous = internal::get_current_adapter();
      internal::set_current_adapter(&ad);
    }
    ~scoped_adapter() { internal::set_current_adapter(previous); }
    adapter_base* previous;
  };
}

