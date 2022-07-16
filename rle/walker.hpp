//
// created by : Timothée Feuillet
// date: 2022-7-9
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

#pragma once

#include "../rle/rle.hpp"

namespace neam::rle
{
  /// \brief Example of a type-helper (needs a target type and a walk_type function)
  template<typename PayloadArgT>
  struct type_helper_info
  {
    using payload_arg_t = PayloadArgT;

    rle::type_metadata target_type;

    using walk_type_func_t = void(*)(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload);
    walk_type_func_t walk_type;
  };

  /// \brief Empty formatter. Inherit from it so you only have to define functions you'll actually use.
  /// \note You can also override the type-helper type
  template<typename PayloadArgT>
  struct empty_walker
  {
    using payload_arg_t = PayloadArgT;

    static constexpr uint32_t get_type_helper_count() { return 0; }
    static type_helper_info<payload_arg_t>* get_type_helper(uint32_t index) { return nullptr; }

    static void on_type_invalid(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload) {}
    static void on_type_empty(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload) {}


    static void on_type_raw(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, const uint8_t* addr, size_t size) {}


    static void on_type_container_pre(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count, const rle::type_metadata& sub_type) {}
    static void on_type_container_post(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count, const rle::type_metadata& sub_type) {}
    static void on_type_container_pre_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, uint32_t count, const rle::type_metadata& sub_type) {}
    static void on_type_container_post_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, uint32_t count, const rle::type_metadata& sub_type) {}


    static void on_type_variant_empty(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload) {}
    static void on_type_variant_pre_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, const rle::type_metadata& sub_type) {}
    static void on_type_variant_post_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, const rle::type_metadata& sub_type) {}


    static void on_type_tuple_version(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t version) {}

    static void on_type_tuple_pre(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count) {}
    static void on_type_tuple_post(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count) {}
    static void on_type_tuple_pre_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, uint32_t count, const rle::type_metadata& sub_type) {}
    static void on_type_tuple_post_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t index, uint32_t count, const rle::type_metadata& sub_type) {}
  };

  /// \brief Walk a RLE serialized data
  template<typename Formatter>
  class walker
  {
    public:
      using payload_arg_t = typename Formatter::payload_arg_t;

    private: // utility
      struct void_parameter_t {};
      template<typename Fnc, typename... Args>
      static auto safe_call_pre(Fnc&& fnc, Args&&... args)
      {
        constexpr bool k_is_void = std::is_same_v<void, std::invoke_result_t<Fnc, Args...>>;
        if constexpr (k_is_void)
        {
          fnc(std::forward<Args>(args)...);
          return void_parameter_t{};
        }
        else
        {
          return fnc(std::forward<Args>(args)...);
        }
      }
      template<typename Fnc, typename R, typename... Args>
      static auto safe_call_post(Fnc&& fnc, R&& r, Args&&... args)
      {
        constexpr bool k_is_void = std::is_same_v<void_parameter_t, std::remove_cvref_t<R>>;
        if constexpr (k_is_void)
          fnc(std::forward<Args>(args)...);
        else
          fnc(std::forward<Args>(args)..., std::forward<R>(r));
      }

    public:
      /// \brief walk a full serialized data:
      static payload_arg_t walk(const raw_data& rd, const rle::serialization_metadata& md, payload_arg_t payload)
      {
        return walk(rd, 0, rd.size, md, payload);
      }

      /// \brief walk a full serialized data:
      static payload_arg_t walk(const raw_data& rd, uint64_t offset, uint64_t size, const rle::serialization_metadata& md, payload_arg_t payload)
      {
        const rle::type_metadata& root = md.type(md.root);

        rle::decoder dc(rd, offset, size);
        walk_type(md, root, dc, payload);
        return payload;
      }

      /// \brief recurse in a type
      static void walk_type(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        const uint32_t helper_index = search_for_type_helper(md, type, type.to_generic());
        if (helper_index != ~0u)
        {
          auto* helper = Formatter::get_type_helper(helper_index);
          if (helper != nullptr)
            return helper->walk_type(md, type, dc, payload);
        }

        return walk_type_generic(md, type, dc, payload);
      }

      /// \brief recurse in a type, but do not search for an override
      static void walk_type_generic(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        switch (type.mode)
        {
          case rle::type_mode::raw:             return walk_type_raw(md, type, dc, payload);
          case rle::type_mode::container:       return walk_type_container(md, type, dc, payload);
          case rle::type_mode::variant:         return walk_type_variant(md, type, dc, payload);
          case rle::type_mode::tuple:           return walk_type_tuple(md, type, dc, payload);
          case rle::type_mode::versioned_tuple: return walk_type_versioned_tuple(md, type, dc, payload);
          default:                              return walk_type_invalid(md, type, dc, payload);
        }
      }

      /// \brief Search for a type helper, and return the index (or ~0u if not found)
      static uint32_t search_for_type_helper(const rle::serialization_metadata& md, const rle::type_metadata& type, const rle::type_metadata& gen_type)
      {
        const uint32_t helper_count = Formatter::get_type_helper_count();
        uint32_t found = ~0u;
        for (uint32_t i = 0; i < helper_count; ++i)
        {
          auto* helper = Formatter::get_type_helper(i);
          if (helper == nullptr)
            continue;
          if ((found == ~0u && rle::are_equivalent(md, helper->target_type, gen_type)))
          {
            found = i;
          }
          else if (helper->target_type.hash != 0 && helper->target_type == type)
          {
            found = i;
            return i;
          }
        }
        return found;
      }

    public: // generic walkers:
      static void walk_type_raw(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        const uint8_t* data = dc.get_address<uint8_t>();
        Formatter::on_type_raw(md, type, payload, data, type.size);
        dc.skip(type.size);
      }
      static void walk_type_container(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        if (type.contained_types.size() != 1)
          return walk_type_empty(md, type, dc, payload);

        const uint32_t count = dc.decode<uint32_t>().first;
        const rle::type_metadata& sub_type = md.type(type.contained_types[0].hash);
        auto ret = safe_call_pre(Formatter::on_type_container_pre, md, type, payload, count, sub_type);

        for (uint32_t i = 0; i < count; ++i)
        {
          safe_call_post(Formatter::on_type_container_pre_entry, ret, md, type, payload, i, count, sub_type);
          walk_type(md, sub_type, dc, payload);
          safe_call_post(Formatter::on_type_container_post_entry, ret, md, type, payload, i, count, sub_type);
        }

        safe_call_post(Formatter::on_type_container_post, ret, md, type, payload, count, sub_type);
      }
      static void walk_type_variant(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        const uint32_t index = dc.decode<uint32_t>().first;
        if (index >= type.contained_types.size() + 1)
          return walk_type_invalid(md, type, dc, payload);
        if (index == 0)
          return Formatter::on_type_variant_empty(md, type, payload);

        const rle::type_metadata& sub_type = md.type(type.contained_types[index - 1].hash);
        auto ret = safe_call_pre(Formatter::on_type_variant_pre_entry, md, type, payload, index, sub_type);
        walk_type(md, sub_type, dc, payload);
        safe_call_post(Formatter::on_type_variant_post_entry, ret, md, type, payload, index, sub_type);
      }
      static void walk_type_tuple(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        const uint32_t count = type.contained_types.size();
        if (count == 0)
          return walk_type_empty(md, type, dc, payload);

        auto ret = safe_call_pre(Formatter::on_type_tuple_pre, md, type, payload, count);
        for (uint32_t i = 0; i < count; ++i)
        {
          const rle::type_metadata& sub_type = md.type(type.contained_types[i].hash);
          safe_call_post(Formatter::on_type_tuple_pre_entry, ret, md, type, payload, i, count, sub_type);
          walk_type(md, sub_type, dc, payload);
          safe_call_post(Formatter::on_type_tuple_post_entry, ret, md, type, payload, i, count, sub_type);
        }
        safe_call_post(Formatter::on_type_tuple_post, ret, md, type, payload, count);
      }

      static void walk_type_versioned_tuple(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        Formatter::on_type_tuple_version(md, type, payload, dc.decode<uint32_t>().first);
        return walk_type_tuple(md, type, dc, payload);
      }
      static void walk_type_invalid(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        return Formatter::on_type_invalid(md, type, payload);
      }
      static void walk_type_empty(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
      {
        return Formatter::on_type_empty(md, type, payload);
      }
  };
}

