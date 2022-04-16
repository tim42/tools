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

#pragma once

#include "raw_data.hpp"
#include "rle/rle.hpp"

namespace neam::cr::log_type
{
  namespace helpers
  {
    const char* spc(uint32_t indent);
    /// \brief return the generic name for the given type
    std::string get_generic_type_name(const rle::serialization_metadata& md, const rle::type_metadata& type);

    /// \brief dispatch to the correct logger function (for recursive logs)
    void do_log_type(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name);

    using log_fnc = void (*)(const rle::serialization_metadata& md, uint32_t indent, const rle::type_metadata& type, rle::decoder& dc, const std::string& name);
    using name_fnc = std::string (*)(const rle::serialization_metadata& md, const rle::type_metadata& type);

    struct log_helper_t
    {
      rle::type_metadata type;
      log_fnc logger;
      name_fnc type_name = nullptr;
    };

    /// \brief Add a new log helper.
    /// Log helpers with a matching type hash takes precedence over the ones without one.
    void add_log_helper(const log_helper_t& h);

    /// \brief inherit from this class and:
    ///  - define log (signature of log_fnc, as static) 
    ///  - define static rle::type_metadata get_type_metadata() that returns the type to attach to
    ///  - (optional) define type_name (signature of name_fnc, as static)
    ///
    /// See string_logger for a simple example. (it matches containers of char and display them as strings).
    template<typename Child>
    class auto_register
    {
      private:
        static void register_child()
        {
          add_log_helper(
            {
              Child::get_type_metadata(),
              &Child::log,
              Child::type_name,
            });
        }
        static inline int _reg = []
        {
          register_child();
          return 0;
        } ();
        static_assert(&_reg == &_reg);

      public:
        static constexpr name_fnc type_name = nullptr;
    };
  }

  /// \brief Log a type using only the metadata and serialized info
  /// (the type might not necessarily be in the current binary)
  void log_type(const raw_data& rd, const rle::serialization_metadata& md);

  // helpers:
  template<typename Type>
  void log_type(const raw_data& rd)
  {
    log_type(rd, rle::generate_metadata<Type>());
  }

  template<typename Type>
  void log_type(const Type& value)
  {
    log_type(rle::serialize<Type>(value), rle::generate_metadata<Type>());
  }
}
