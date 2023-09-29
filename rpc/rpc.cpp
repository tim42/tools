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

#include "../debug/assert.hpp"
#include "rpc.hpp"

namespace neam::rpc
{
  namespace internal
  {
    struct global_data_t
    {
      spinlock lock;
      std::map<string_id, function_t> procedures;
    };
    static global_data_t& get_global_data() { static global_data_t data; return data; }

    struct thread_data_t
    {
      id_t current_rpc_call = id_t::none;
      adapter_base* adapter = nullptr;
    };
    static thread_data_t& get_thread_data() { static thread_data_t data; return data; }
  }

  struct protocol1_footer_t
  {
    uint32_t protocol_key;
    uint32_t protocol_version;
    id_t procedure_id;
  };

  static constexpr uint32_t k_protocol_key = 0x6370726E;
  static constexpr uint32_t k_protocol_version = 1;

  size_t dispatcher::get_footer_size()
  {
    return sizeof(protocol1_footer_t);
  }

  void dispatcher::local_call(raw_data&& data, uint64_t offset)
  {
    if (data.size <= offset)
      return internal::on_error({}, "invalid data: no data provided");

    // we encode the data in the following form:
    // [---- RLE DATA ----][ protocol<k_protocol_version>_footer_t ]
    // This avoid doing any memory operation and simply forwarding the data as-is to RLE (while simply changing the size)
    if (data.size - offset < sizeof(protocol1_footer_t))
      return internal::on_error({}, "invalid data (not an rpc call)");

    const protocol1_footer_t* const footer1 = (protocol1_footer_t*)((uint8_t*)(data.get()) + data.size - sizeof(protocol1_footer_t));
    if (footer1->protocol_key != k_protocol_key)
      return internal::on_error({}, "invalid protocol");

    if (footer1->protocol_version > k_protocol_version)
      return internal::on_error({}, "invalid protocol version");

    data.size -= sizeof(protocol1_footer_t);

    // NOTE: If we have more versions, unstack the footers there

    auto& procedures = internal::get_global_data().procedures;

    auto id = string_id::_from_id_t(footer1->procedure_id);
    if (auto it = procedures.find(id); it != procedures.end())
    {
      id_t previous_call = id;
      std::swap(internal::get_thread_data().current_rpc_call, previous_call);
      it->second(std::move(data), offset);
      internal::get_thread_data().current_rpc_call = previous_call;
      return;
    }

    // BAD CALL: unknown procedure
    internal::on_error(id, "unknown procedure being called");
  }

  raw_data dispatcher::prepare_for_empty_call(id_t rpc_id, adapter_base& adapter)
  {
    raw_data ret = raw_data::allocate(adapter.get_header_size() + sizeof(protocol1_footer_t) + adapter.get_footer_size());
    uint8_t* data = (uint8_t*)ret.get();
    protocol1_footer_t* footer1 = (protocol1_footer_t*)(data + adapter.get_header_size());
    footer1->protocol_key = k_protocol_key;
    footer1->protocol_version = k_protocol_version;
    footer1->procedure_id = rpc_id;
    return ret;
  }

  raw_data dispatcher::prepare_for_rle_call(id_t rpc_id, rle::encoder& ec, adapter_base& adapter)
  {
    protocol1_footer_t* footer1 = (protocol1_footer_t*)ec.allocate(sizeof(protocol1_footer_t));
    footer1->protocol_key = k_protocol_key;
    footer1->protocol_version = k_protocol_version;
    footer1->procedure_id = rpc_id;

    // allocate the footer if there's any:
    ec.allocate(adapter.get_footer_size());

    return ec.to_raw_data();
  }

  namespace internal
  {
    void register_function(string_id id, function_t&& function)
    {
      auto& data = get_global_data();
      std::lock_guard _lg(data.lock);

      data.procedures.emplace(id, std::move(function));
    }

    void unregister_function(string_id id)
    {
      auto& data = get_global_data();
      std::lock_guard _lg(data.lock);
      data.procedures.erase(id);
    }

    void log_functions()
    {
      auto& data = get_global_data();
      std::lock_guard _lg(data.lock);
      for (auto& it : data.procedures)
      {
        cr::out().debug("> {}", it.first);
      }
    }


    void on_error(string_id id, std::string&& message)
    {
      if (id == id_t::none)
      {
        check::debug::n_check(false, "NRPC: {}", message);
        return;
      }
      check::debug::n_check(false, "NRPC: {}: {}", id, message);
    }

    id_t get_current_rpc_call() { return get_thread_data().current_rpc_call; }

    void set_current_adapter(adapter_base* ab)
    {
      get_thread_data().adapter = ab;
    }

    adapter_base* get_current_adapter()
    {
      return get_thread_data().adapter;
    }
  }
}

