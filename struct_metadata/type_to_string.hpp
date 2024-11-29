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

#include "../raw_data.hpp"
#include "../rle/rle.hpp"

namespace neam::metadata
{
  /// \brief stringify a type using only the metadata and serialized info
  /// (the type might not necessarily be in the current executable)
  std::string type_to_string(const raw_data& rd, const rle::serialization_metadata& md);

  // helpers:
  template<typename Type>
  std::string type_to_string(const raw_data& rd)
  {
    return type_to_string(rd, rle::generate_metadata<Type>());
  }

  template<typename Type>
  std::string type_to_string(const Type& value)
  {
    return type_to_string(rle::serialize<Type>(value), rle::generate_metadata<Type>());
  }

  namespace helpers
  {
    struct payload_t
    {
      std::string result;
      std::string member_name;
      unsigned indent = 0;
    };
    using payload_arg_t = payload_t&;

    /// \brief return a generic name for the given type
    std::string get_generic_type_name(const rle::serialization_metadata& md, const rle::type_metadata& type);

    /// \brief Allow the type-helpers to go-back to the standard way to recurse over types
    /// \note This alternative \e will call type-helpers (beware of stack-verflows)
    void walk_type(const rle::serialization_metadata& md, const rle::type_metadata& type, const rle::type_reference& type_ref, rle::decoder& dc, payload_arg_t payload);

    /// \brief Allow the type-helpers to go-back to the standard way to recurse over types
    /// \note This alternative \e will \e not call type-helpers
    void walk_type_generic(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload);


    using walk_type_fnc_t = void (*)(const rle::serialization_metadata& md, const rle::type_metadata& type, rle::decoder& dc, payload_arg_t payload);
    using get_type_name_fnc_t = std::string (*)(const rle::serialization_metadata& md, const rle::type_metadata& type);
    using on_type_raw_fnc_t = void(*)(const rle::serialization_metadata& md, const rle::type_metadata& type, payload_arg_t payload, const uint8_t* addr, size_t size);


    struct type_helper_t
    {
      rle::type_metadata target_type;
      id_t helper_custom_id = id_t::none;
      walk_type_fnc_t walk_type = walk_type_generic;
      get_type_name_fnc_t type_name = nullptr;
    };

    struct raw_type_helper_t
    {
      rle::type_hash_t target_type;
      on_type_raw_fnc_t on_type_raw = nullptr;
    };

    /// \brief Add a new to-string helper.
    void add_to_string_type_helper(const type_helper_t& h);

    /// \brief Add a new to-string raw-type helper.
    void add_to_string_raw_type_helper(const raw_type_helper_t& h);

    /// \brief inherit from this class and:
    ///  - define walk_type (signature of walk_type_fnc_t, as static)
    ///  - define static rle::type_metadata get_type_metadata() that returns the type to attach to
    ///  - (optional) define get_type_name (signature of get_type_name_fnc_t, as static)
    ///
    /// See string_logger for a simple example. (it matches containers of char and display them as strings).
    template<typename Child>
    class auto_register_to_string_type_helper
    {
      public:
        static constexpr id_t get_custom_helper_id() { return id_t::none; }

      private:
        static void register_child()
        {
          add_to_string_type_helper(
            {
              Child::get_type_metadata(),
              Child::get_custom_helper_id(),
              &Child::walk_type,
              Child::get_type_name,
            });
        }
        static inline int _reg = [] { register_child(); return 0; } ();
        static_assert(&_reg == &_reg);
      public:
        static constexpr get_type_name_fnc_t get_type_name = nullptr;
    };

    template<typename Child>
    class auto_register_to_string_raw_type_helper
    {
      private:
        static void register_child()
        {
          add_to_string_raw_type_helper(
            {
              Child::get_type_hash(),
              &Child::on_type_raw,
            });
        }
        static inline int _reg = [] { register_child(); return 0; } ();
        static_assert(&_reg == &_reg);
    };
  }
}

