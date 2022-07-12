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

#include <fmt/format.h>
#include "type_to_string.hpp"
#include "../rle/walker.hpp"

namespace neam::metadata
{
  static const char* spc(uint32_t indent)
  {
    static constexpr uint32_t k_size = 64;
    static char data[k_size + 1] = {0};
    if (data[0] == 0)
    {
      memset(data, ' ', k_size);
      data[k_size] = 0;
    }
    return data + (k_size - std::min(k_size, indent));
  }

  template<typename T>
  static void raw_type_helper(const rle::serialization_metadata& md, const rle::type_metadata& type, helpers::payload_arg_t payload, const uint8_t* addr, size_t size)
  {
    payload.result = fmt::format("{}{}{}{} = {}\n", payload.result, spc(payload.indent), type.name, payload.member_name, *(const T*)(addr));
  }
  template<typename T>
  static void raw_type_helper_char(const rle::serialization_metadata& md, const rle::type_metadata& type, helpers::payload_arg_t payload, const uint8_t* addr, size_t size)
  {
    payload.result = fmt::format("{}{}{}{} = '{}'\n", payload.result, spc(payload.indent), type.name, payload.member_name, *(const T*)(addr));
  }

#define N_TTS_RAW_TYPE(Type) {rle::serialization_metadata::hash_of<Type>(), &raw_type_helper<Type>}
#define N_TTS_RAW_TYPE_CHAR(Type) {rle::serialization_metadata::hash_of<Type>(), &raw_type_helper_char<Type>}


  static std::vector<helpers::type_helper_t> type_helpers;

  static std::map<rle::type_hash_t, helpers::on_type_raw_fnc_t> raw_type_helpers =
  {
    N_TTS_RAW_TYPE(uint8_t), N_TTS_RAW_TYPE(int8_t),
    N_TTS_RAW_TYPE(uint16_t), N_TTS_RAW_TYPE(int16_t),
    N_TTS_RAW_TYPE(uint32_t), N_TTS_RAW_TYPE(int32_t),
    N_TTS_RAW_TYPE(uint64_t), N_TTS_RAW_TYPE(int64_t),

    N_TTS_RAW_TYPE_CHAR(unsigned char), N_TTS_RAW_TYPE_CHAR(char),

    N_TTS_RAW_TYPE(unsigned short), N_TTS_RAW_TYPE(short),
    N_TTS_RAW_TYPE(unsigned int), N_TTS_RAW_TYPE(int),
    N_TTS_RAW_TYPE(unsigned long), N_TTS_RAW_TYPE(long),
    N_TTS_RAW_TYPE(unsigned long long), N_TTS_RAW_TYPE(long long),

    N_TTS_RAW_TYPE(float), N_TTS_RAW_TYPE(double),
  };

#undef N_TTS_RAW_TYPE
#undef N_TTS_RAW_TYPE_CHAR


  std::string helpers::get_generic_type_name(const rle::serialization_metadata& md, const rle::type_metadata& type)
  {
    // search for a specific logger:
    const rle::type_metadata gen_type = type.to_generic();
    const helpers::type_helper_t* helper = nullptr;
    for (const auto& it : type_helpers)
    {
      if (it.type_name == nullptr) continue;
      if ((helper == nullptr && rle::are_equivalent(md, it.target_type, gen_type)))
      {
        helper = &it;
      }
      else if (it.target_type.hash != 0 && it.target_type == type)
      {
        helper = &it;
        break;
      }
    }
    if (helper != nullptr)
    {
      return helper->type_name(md, type);
    }

    // fallback to the generic stuff:
    if (type.mode == rle::type_mode::raw)
      return type.name;
    if (type.mode == rle::type_mode::invalid)
      return "#invalid";
    if (type.mode == rle::type_mode::container)
    {
      if (type.contained_types.size() != 1)
        return "#invalid[]";
      return fmt::format("{}[]", get_generic_type_name(md, md.type(type.contained_types[0].hash)));
    }
    if (type.mode == rle::type_mode::tuple || type.mode == rle::type_mode::versioned_tuple)
    {
      if (type.contained_types.size() == 0)
        return "struct";
      if (!type.contained_types[0].name.empty())
        return "struct";
      std::string ret = "tuple:(";
      bool first = true;
      for (const auto& it : type.contained_types)
      {
        if (!first)
          ret = fmt::format("{}, ", ret);
        first = false;
        ret = fmt::format("{}{}", ret, get_generic_type_name(md, md.type(it.hash)));
      }
      return ret + ")";
    }

    if (type.mode == rle::type_mode::variant)
    {
      if (type.contained_types.size() == 0)
        return "#invalid";
      if (type.contained_types.size() == 1)
        return fmt::format("optional:({})", get_generic_type_name(md, md.type(type.contained_types[0].hash)));
      std::string ret = "union:(";
      bool first = true;
      for (const auto& it : type.contained_types)
      {
        if (!first)
          ret = fmt::format("{}, ", ret);
        first = false;
        ret = fmt::format("{}{}", ret, get_generic_type_name(md, md.type(it.hash)));
      }
      return ret + ")";
    }
    return "#unknown";
  }

  void helpers::add_to_string_type_helper(const type_helper_t& h)
  {
    type_helpers.push_back(h);
  }
  void helpers::add_to_string_raw_type_helper(const raw_type_helper_t& h)
  {
    raw_type_helpers.emplace(h.target_type, h.on_type_raw);
  }

  struct to_string_walker : public rle::empty_walker<helpers::payload_arg_t>
  {
    static uint32_t get_type_helper_count() { return (uint32_t)type_helpers.size(); }
    static helpers::type_helper_t* get_type_helper(uint32_t index) { return &type_helpers[index]; }

    static void on_type_invalid(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload)
    {
      payload.result = fmt::format("{}{}{}{} = {{/* ?? */}} /* unknown type mode for type */\n", payload.result, spc(payload.indent), helpers::get_generic_type_name(md, type), payload.member_name);
    }
    static void on_type_empty(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload)
    {
      payload.result = fmt::format("{}{}{}{} = {{}} /* empty */\n", payload.result, spc(payload.indent), helpers::get_generic_type_name(md, type), payload.member_name);
    }

    static void on_type_raw(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, const uint8_t* addr, size_t size)
    {
      if (const auto it = raw_type_helpers.find(type.hash); it != raw_type_helpers.end())
      {
        it->second(md, type, payload, addr, size);
        return;
      }

      // not found: generic printing:
      switch (size)
      {
        case 1:   payload.result = fmt::format("{}{}{}{} = {:#x}\n", payload.result,
                                                 spc(payload.indent), type.name, payload.member_name, *(const uint8_t*)addr);
          break;
        case 2:   payload.result = fmt::format("{}{}{}{} = {:#x}\n", payload.result,
                                                 spc(payload.indent), type.name, payload.member_name, *(const uint16_t*)addr);
          break;
        case 4:   payload.result = fmt::format("{}{}{}{} = {:#x}\n", payload.result,
                                                 spc(payload.indent), type.name, payload.member_name, *(const uint32_t*)addr);
          break;
        case 8:   payload.result = fmt::format("{}{}{}{} = {:#x}\n", payload.result,
                                                 spc(payload.indent), type.name, payload.member_name, *(const uint64_t*)addr);
          break;
        default:  payload.result = fmt::format("{}{}{}{} = {{/* unknown {} byte data */}}\n", payload.result,
                                                 spc(payload.indent), type.name, payload.member_name, size);
          break;
      }
    }



    static void on_type_container_pre(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload,
                                      uint32_t count, const rle::type_metadata& sub_type)
    {
      if (count == 0)
      {
        payload.result = fmt::format("{}{}{}[0] {} = {{}} /* empty */\n", payload.result,
                                     spc(payload.indent), helpers::get_generic_type_name(md, sub_type), payload.member_name);
        return;
      }
      payload.result = fmt::format("{}{}{}[{}]{} =\n", payload.result,
                                   spc(payload.indent), helpers::get_generic_type_name(md, sub_type), count, payload.member_name);
      payload.result = fmt::format("{}{}{{\n", payload.result, spc(payload.indent));
      payload.indent += 2;

    }
    static void on_type_container_post(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload,
                                       uint32_t count, const rle::type_metadata& sub_type)
    {
      if (count == 0)
        return;
      payload.indent -= 2;
      payload.result = fmt::format("{}{}}}\n", payload.result, spc(payload.indent));
    }



    static void on_type_variant_empty(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload)
    {
      payload.result = fmt::format("{}{}{}{} = {{}} /* empty */\n", payload.result,
                                   spc(payload.indent), helpers::get_generic_type_name(md, type), payload.member_name);
    }



    static void on_type_tuple_pre(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count)
    {
      payload.result = fmt::format("{}{}{}{} =\n", payload.result,
                                   spc(payload.indent), helpers::get_generic_type_name(md, type), payload.member_name);
      payload.result = fmt::format("{}{}{{\n", payload.result, spc(payload.indent));

      payload.indent += 2;
    }
    static void on_type_tuple_post(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, uint32_t count)
    {
      payload.indent -= 2;
      payload.result = fmt::format("{}{}}}\n", payload.result, spc(payload.indent));
    }
    static void on_type_tuple_pre_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload,
                                        uint32_t index, uint32_t count, const rle::type_metadata& sub_type)
    {
      payload.member_name = type.contained_types[index].name.empty() ? std::string{} : (" " + type.contained_types[index].name);
    }
    static void on_type_tuple_post_entry(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload,
                                         uint32_t index, uint32_t count, const rle::type_metadata& sub_type)
    {
      payload.member_name = {};
    }
  };

  void helpers::walk_type(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
  {
    return rle::walker<to_string_walker>::walk_type(md, type, dc, payload);
  }

  void helpers::walk_type_generic(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload)
  {
    return rle::walker<to_string_walker>::walk_type_generic(md, type, dc, payload);
  }

  std::string type_to_string(const raw_data& rd, const rle::serialization_metadata& md)
  {
    helpers::payload_t payload;
    return rle::walker<to_string_walker>::walk(rd, md, payload).result;
  }


  // builtin helpers:


  struct string_to_string : helpers::auto_register_to_string_type_helper<string_to_string>
  {
    static rle::type_metadata get_type_metadata()
    {
      return rle::type_metadata::from(rle::type_mode::container, {{rle::serialization_metadata::hash_of<char>()}});
    }

    static void walk_type(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, helpers::payload_arg_t payload)
    {
      const uint32_t count = dc.decode<uint32_t>().first;
      payload.result = fmt::format("{}{}{}[{}]{} = \"{}\"\n", payload.result,
                  spc(payload.indent), helpers::get_generic_type_name(md, md.type(type.contained_types[0].hash)), count, payload.member_name, std::string_view(dc.get_address<char>(), count));
      dc.skip(count);
    }
  };
}

