//
// created by : Timothée Feuillet
// date: 2022-4-15
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

#include "log_type.hpp"

#include "logger/logger.hpp"

namespace neam::cr::log_type
{
  static std::vector<helpers::log_helper_t> log_helpers;

  const char* helpers::spc(uint32_t indent)
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

  std::string helpers::get_generic_type_name(const rle::serialization_metadata& md, const rle::type_metadata& type)
  {
    // search for a specific logger:
    const rle::type_metadata gen_type = type.to_generic();
    const helpers::log_helper_t* helper = nullptr;
    for (const auto& it : log_helpers)
    {
      if (it.type_name == nullptr) continue;
      if ((helper == nullptr && rle::are_equivalent(md, it.type, gen_type)))
      {
        helper = &it;
      }
      else if (it.type.hash != 0 && it.type == type)
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

  void helpers::add_log_helper(const helpers::log_helper_t& h)
  {
    log_helpers.push_back(h);
  }

  template<typename T>
  void raw_type_helper(const rle::serialization_metadata& /*md*/, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    cr::out().log("{}{}{} = {}", helpers::spc(indent), type.name, name, *dc.get_address<T>());
    dc.skip(sizeof(T));
  }
  template<typename T>
  void raw_type_helper_char(const rle::serialization_metadata& /*md*/, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    cr::out().log("{}{}{} = '{}'", helpers::spc(indent), type.name, name, *dc.get_address<T>());
    dc.skip(sizeof(T));
  }

  // we have a map of raw type to avoid cluttering the log_helpers.
  // Moreover, this makes the default raw helpers have a lower priority compared to those.
#define N_LT_RAW_TYPE(Type) {rle::serialization_metadata::hash_of<Type>(), &raw_type_helper<Type>}
#define N_LT_RAW_TYPE_CHAR(Type) {rle::serialization_metadata::hash_of<Type>(), &raw_type_helper_char<Type>}
  static const std::map<rle::type_hash_t, helpers::log_fnc> raw_log_helpers =
  {
    N_LT_RAW_TYPE(uint8_t), N_LT_RAW_TYPE(int8_t),
    N_LT_RAW_TYPE(uint16_t), N_LT_RAW_TYPE(int16_t),
    N_LT_RAW_TYPE(uint32_t), N_LT_RAW_TYPE(int32_t),
    N_LT_RAW_TYPE(uint64_t), N_LT_RAW_TYPE(int64_t),

    N_LT_RAW_TYPE_CHAR(unsigned char), N_LT_RAW_TYPE_CHAR(char),

    N_LT_RAW_TYPE(unsigned short), N_LT_RAW_TYPE(short),
    N_LT_RAW_TYPE(unsigned int), N_LT_RAW_TYPE(int),
    N_LT_RAW_TYPE(unsigned long), N_LT_RAW_TYPE(long),
    N_LT_RAW_TYPE(unsigned long long), N_LT_RAW_TYPE(long long),

    N_LT_RAW_TYPE(float), N_LT_RAW_TYPE(double),
  };

  template<rle::type_mode Mode>
  static void log_type_mode(const rle::serialization_metadata& /*md*/, uint32_t indent, const rle::type_metadata& type, rle::decoder& /*dc*/, const std::string& name)
  {
    cr::out().log("{}{}; // {}: unknown type mode for type", helpers::spc(indent), type.name, name);
  }

  template<>
  void log_type_mode<rle::type_mode::raw>(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    if (const auto it = raw_log_helpers.find(type.hash); it != raw_log_helpers.end())
    {
      it->second(md, indent, type, dc, name);
      return;
    }

    // not found: generic printing:
    switch (type.size)
    {
      case 1: cr::out().log("{}{}{} = {:#x}", helpers::spc(indent), type.name, name, *dc.get_address<uint8_t>()); break;
      case 2: cr::out().log("{}{}{} = {:#x}", helpers::spc(indent), type.name, name, *dc.get_address<uint16_t>()); break;
      case 4: cr::out().log("{}{}{} = {:#x}", helpers::spc(indent), type.name, name, *dc.get_address<uint32_t>()); break;
      case 8: cr::out().log("{}{}{} = {:#x}", helpers::spc(indent), type.name, name, *dc.get_address<uint64_t>()); break;
      default: cr::out().log("{}{}{} = /* unknown {} byte data*/", helpers::spc(indent), type.name, name, type.size); break;
    }
    dc.skip(type.size);
  }

  template<>
  void log_type_mode<rle::type_mode::variant>(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    const uint32_t index = dc.decode<uint32_t>().first;
    if (index == 0 || index >= type.contained_types.size())
    {
      cr::out().log("{}{}{} = {{}} /* empty */", helpers::spc(indent), helpers::get_generic_type_name(md, type), name);
      return;
    }
    const rle::type_metadata& st = md.type(type.contained_types[index].hash);
    helpers::do_log_type(md, indent, st, dc, name);
  }

  template<>
  void log_type_mode<rle::type_mode::container>(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    if (type.contained_types.size() != 1)
    {
      cr::out().log("{}{}{} = {{}} /* invalid */", helpers::spc(indent), helpers::get_generic_type_name(md, type), name);
      return;
    }
    const uint32_t count = dc.decode<uint32_t>().first;
    const rle::type_metadata& st = md.type(type.contained_types[0].hash);
    if (count == 0)
    {
      cr::out().log("{}{}[0] {} = {{}} /* empty */", helpers::spc(indent), helpers::get_generic_type_name(md, st), name);
      return;
    }
    cr::out().log("{}{}[{}]{} =", helpers::spc(indent), helpers::get_generic_type_name(md, st), count, name);
    cr::out().log("{}{{", helpers::spc(indent));
    for (uint32_t i = 0; i < count; ++i)
    {
      helpers::do_log_type(md, indent + 2, st, dc, {});
    }
    cr::out().log("{}}}", helpers::spc(indent));
  }

  template<>
  void log_type_mode<rle::type_mode::tuple>(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    if (type.contained_types.size() == 0)
    {
      cr::out().log("{}{}{} = {{}} /* empty */", helpers::spc(indent), helpers::get_generic_type_name(md, type), name);
      return;
    }
    cr::out().log("{}{}{} =", helpers::spc(indent), helpers::get_generic_type_name(md, type), name);
    cr::out().log("{}{{", helpers::spc(indent));
    for (uint32_t i = 0; i < type.contained_types.size(); ++i)
    {
      const rle::type_metadata& st = md.type(type.contained_types[i].hash);
      helpers::do_log_type(md, indent + 2, st, dc, type.contained_types[i].name.empty() ? std::string{} : (" " + type.contained_types[i].name));
    }
    cr::out().log("{}}}", helpers::spc(indent));
  }

  template<>
  void log_type_mode<rle::type_mode::versioned_tuple>(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    dc.skip(sizeof(uint32_t));
    log_type_mode<rle::type_mode::tuple>(md, indent, type, dc, name);
  }

  void helpers::do_log_type(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
  {
    // search for a specific logger:
    const rle::type_metadata gen_type = type.to_generic();
    const helpers::log_helper_t* helper = nullptr;
    for (const auto& it : log_helpers)
    {
      if ((helper == nullptr && rle::are_equivalent(md, it.type, gen_type)))
      {
        helper = &it;
      }
      else if (it.type.hash != 0 && it.type == type)
      {
        helper = &it;
        break;
      }
    }
    if (helper != nullptr)
    {
      return helper->logger(md, indent, type, dc, name);
    }

    // fallback to generic loggers:
    switch (type.mode)
    {
      case rle::type_mode::raw: return log_type_mode<rle::type_mode::raw>(md, indent, type, dc, name);
      case rle::type_mode::container: return log_type_mode<rle::type_mode::container>(md, indent, type, dc, name);
      case rle::type_mode::variant: return log_type_mode<rle::type_mode::variant>(md, indent, type, dc, name);
      case rle::type_mode::tuple: return log_type_mode<rle::type_mode::tuple>(md, indent, type, dc, name);
      case rle::type_mode::versioned_tuple: return log_type_mode<rle::type_mode::versioned_tuple>(md, indent, type, dc, name);
      default: return log_type_mode<rle::type_mode::invalid>(md, indent, type, dc, name);
    }
  }

  void log_type(const raw_data& rd, const rle::serialization_metadata& md)
  {
    const rle::type_metadata& root = md.type(md.root);

    rle::decoder dc(rd);
    helpers::do_log_type(md, 0, root, dc, {});
  }


  // A helper for strings (to avoid having them displayed as an array of char)
  struct string_logger : helpers::auto_register<string_logger>
  {
    static rle::type_metadata get_type_metadata()
    {
      return rle::type_metadata::from(rle::type_mode::container, {{rle::serialization_metadata::hash_of<char>()}});
    }

    static void log(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name)
    {
      const uint32_t count = dc.decode<uint32_t>().first;
      cr::out().log("{}{}[{}]{} = \"{}\"", helpers::spc(indent), helpers::get_generic_type_name(md, md.type(type.contained_types[0].hash)), count, name, std::string_view(dc.get_address<char>(), count));
      dc.skip(count);
    }
  };
}
