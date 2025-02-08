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

#include <map>
#include <tuple>

#include "../move_only_function/move_only_function.hpp"

#include "../logger/logger.hpp"
#include "../rle/rle.hpp"
#include "../raw_data.hpp"
#include "../id/string_id.hpp"
#include "../macro.hpp"
#include "../function.hpp"
#include "../ct_list.hpp"

#include "adapter.hpp"

// // RPC Usage:
//
// // In header:
// namespace my_namespace
// {
//   // Definition + stub + registration
//   NRPC_DECLARATION(my_namespace, my_rpc_function, int)
// }
//
// // In cpp file A:
// void my_namespace::my_rpc_function(int something) { ... }
//
// // In stub cpp file:
// #define NRPC_GENERATE_STUBS
// #include "the-rpc-header.hpp"
//
// The stub cpp file can be part of both the caller and the callee (so one cpp file for stubs for both directions)
//
// Limitations:
//  - as secure as RLE is for deserializing data. Do not use with untrusted data.
//  - you cannot have a rpc stub AND an implementation (stubs are defined as weak symbols).
//  - performing an RPC on a stub function will generate an error and not perform the call.

namespace neam::rpc
{
  namespace internal
  {
    using function_t = std::move_only_function<void(raw_data&&, uint64_t)>;
    adapter_base* get_current_adapter();
    void set_current_adapter(adapter_base* ab);

    void on_error(string_id id, std::string&& message);

    void register_function(string_id id, function_t&& function);
    void unregister_function(string_id id);
    void log_functions();

    id_t get_current_rpc_call();

    template<id_t TargetId, typename... Args>
    static void remote_call_stub(adapter_base* adapter, Args... args);

    template<ct::string_holder Id, auto Fnc>
    struct raii_register
    {
      static constexpr string_id function_id = string_id(Id);

      raii_register()
      {
        register_function(function_id, [](raw_data&& dt, uint64_t offset)
        {
          using traits = ct::function_traits<decltype(Fnc)>;
          if constexpr (ct::list::size<typename traits::arg_list> > 0)
          {
            rle::status st = rle::status::success;
            auto args = rle::deserialize<typename ct::list::extract<typename traits::arg_list>::template as<std::tuple>>(dt, &st, offset);
            if (st != rle::status::success)
              return on_error(function_id, "failed to deserialize the call arguments");
            []<typename... Args>(std::tuple<Args...>& args)
            {
              Fnc(std::get<Args>(args)...);
            }(args);
          }
          else
          {
            Fnc();
          }
        });
      }
      ~raii_register() { unregister_function(function_id); }
    };
  }

  // Macro busy work to perform both the function declaration, stub definition and registration
  // This enforces a single definition (one definition, one optional implementation)
#define _NRPC_ARG_TYPE(arg, idx, ...)  arg __VA_OPT__(,)
#define _NRPC_ARG_TYPE_UND(arg, idx, ...)  __ ## arg
#define _NRPC_ARG_VAR(arg, idx, ...)  N_GLUE(p, idx)__VA_OPT__(,)
#define _NRPC_ARG_DECL(arg, idx, ...)  arg N_GLUE(p, idx)__VA_OPT__(,)
#if !defined(NRPC_GENERATE_STUBS)
#define NRPC_DECLARATION(prefix, name, ...) \
  void name(N_FOR_EACH_CNT_VA(_NRPC_ARG_TYPE, __VA_ARGS__)); \
  /* perform automatic un/registration */ \
  namespace N_GLUE(_nrpc_internal ## name, __COUNTER__) \
  { \
    struct _raii_reg \
    { \
      static inline auto raii_reg = ::neam::rpc::internal::raii_register \
      < \
        #prefix "::" #name "(" #__VA_ARGS__ ")", (void (*)(N_FOR_EACH_CNT_VA(_NRPC_ARG_TYPE, __VA_ARGS__)))&::prefix::name \
      >{}; \
      static_assert(&raii_reg == &raii_reg); \
    }; \
  }
#else
#define NRPC_DECLARATION(prefix, name, ...) \
  __attribute__((weak)) void name(N_FOR_EACH_CNT_VA(_NRPC_ARG_DECL, __VA_ARGS__)) \
  { \
    /* This static_assert enforces that the prefix must be the correct one while also handling overloads */\
    static_assert((void (*)(N_FOR_EACH_CNT_VA(_NRPC_ARG_TYPE, __VA_ARGS__)))&name == (void (*)(N_FOR_EACH_CNT_VA(_NRPC_ARG_TYPE, __VA_ARGS__)))&::prefix::name, "Incorrect namespace prefix"); \
    ::neam::rpc::internal::remote_call_stub<#prefix "::" #name "(" #__VA_ARGS__ ")"_rid __VA_OPT__(,) N_FOR_EACH_CNT_VA(_NRPC_ARG_TYPE, __VA_ARGS__)> \
    ( \
      ::neam::rpc::internal::get_current_adapter() __VA_OPT__(,) N_FOR_EACH_CNT_VA(_NRPC_ARG_VAR, __VA_ARGS__) \
    ); \
  }
#endif

  /// \brief Dispatch / receive remote calls and call local functions
  /// \note Made to be used in conjunction with neam::io
  /// \note Needs neam::async
  class dispatcher
  {
    public:
      /// \brief Perform a local call
      static void local_call(raw_data&& data, uint64_t offset = 0);

    private:
      static size_t get_footer_size();
      [[nodiscard]] static raw_data prepare_for_empty_call(id_t rpc_id, adapter_base& adapter);
      [[nodiscard]] static raw_data prepare_for_rle_call(id_t rpc_id, rle::encoder& ec, adapter_base& adapter);

    private:
      template<id_t TargetId, typename... Args>
      friend void internal::remote_call_stub(adapter_base* adapter, Args... args);
  };

  template<id_t TargetId, typename... Args>
  static void internal::remote_call_stub(adapter_base* adapter, Args... args)
  {
    if (TargetId == get_current_rpc_call())
      return on_error(string_id::_from_id_t(TargetId), "Stub call: refusing to perform a remote call on a stub.");
    if (adapter == nullptr)
      return on_error(string_id::_from_id_t(TargetId), "Stub call: cannot perform stub call: missing adapter.");

    raw_data call_data;
    if constexpr (sizeof...(Args) > 0)
    {
      // we use RLE to encode the args in this case. Calls are a bit more expensive.
      cr::memory_allocator ma;

      // preallocate the memory (works ~well if the data is just standard-layout types)
      ma.preallocate_contiguous
      (
        adapter->get_header_size() + adapter->get_footer_size() + dispatcher::get_footer_size()
        + sizeof(uint32_t) * 2 // struct header
        + (sizeof(Args) + ...) // std layout types size
        + sizeof...(Args) * sizeof(uint32_t) * 2 // margin, in case we have structs as args
      );

      rle::encoder ec(ma);
      ec.allocate(adapter->get_header_size());
      rle::status st = rle::status::success;
      rle::coder<std::tuple<Args...>>::encode(ec, {args...}, st);
      if (st == rle::status::failure)
        return on_error(string_id::_from_id_t(TargetId), "Stub call: failed to serialize data for the call");
      call_data = dispatcher::prepare_for_rle_call(TargetId, ec, *adapter);
    }
    else
    {
      call_data = dispatcher::prepare_for_empty_call(TargetId, *adapter);
    }

    // send the call data:
    adapter->dispatch_call(std::move(call_data));
  }

  /*NRPC_DEFINITION(neam::rpc, my_rpc_function, int, double)
  namespace test
  {
    NRPC_DEFINITION(neam::rpc::test, my_rpc_function)
    NRPC_DEFINITION(neam::rpc::test, my_rpc_function, int, double)
  }*/
}
