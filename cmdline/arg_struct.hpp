//
// created by : Timothée Feuillet
// date: 2021-12-11
//
//
// Copyright (c) 2021 Timothée Feuillet
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

#include <string_view>
#include <vector>
#include "../logger/logger.hpp"
#include "../type_id.hpp"
#include "../ct_list.hpp"
#include "../container_utils.hpp"

#include "../struct_metadata/struct_metadata.hpp"

#include "conversion_helpers.hpp"

namespace neam::cmdline
{
  namespace concepts
  {
    template<typename T>
    concept HasShorthandMap = requires(T v)
    {
      v.shorthands.find('c');
      v.shorthands.find('c') != v.shorthands.end();
    };

    template<typename T>
    concept HasParameterList = requires(T v)
    {
      v.parameters.push_back(std::string_view());
    };
  }

  /// \brief helper converting options/params strings to C++ struct data
  template<metadata::concepts::StructWithMetadata Struct>
  class arg_struct
  {
    public:
      arg_struct() = default;

      /// \brief handles --params and --no-params
      /// (will expand to --params=true and --params=false respectively)
      /// \note Can only work for boolean args. Will fail otherwise.
      /// \note expects arg_name to be transformed to match C++ names (- -> _)
      bool process_option(std::string_view arg_name)
      {
        // early out: check if option exists and is a boolean option
        // this handles options that starts with a no-
        if (check_opt_exists(arg_name))
        {
          if (check_opt_type<bool>(arg_name))
          {
            return process_option(arg_name, "true");
          }
          cr::out().warn("option {} is not a boolean option, you must provide a value", arg_name);
          return false;
        }

        // handles the case of --no-arg:
        if (arg_name.starts_with("no_"))
        {
          arg_name = arg_name.substr(3);

          if (check_opt_exists(arg_name))
          {
            if (check_opt_type<bool>(arg_name))
            {
              return process_option(arg_name, "false");
            }
            cr::out().warn("option {0} (from --no-{0}) is not a boolean option, you must provide a value (and cannot use --no-{0})", arg_name);
            return false;
          }
        }

        // arg not found:
        cr::out().warn("option {} does not exists", arg_name);
        return false;
      }

      /// \brief handles --params=value
      /// \note expects arg_name to be transformed to match C++ names (- -> _)
      bool process_option(std::string_view arg_name, std::string_view arg_value)
      {
        if (!check_opt_exists(arg_name))
        {
          cr::out().warn("option {} does not exists", arg_name);
          return false;
        }

        bool valid = false;
        [&valid, arg_name, arg_value, this]<size_t... Indices>(std::index_sequence<Indices...>)
        {
          ([&valid, arg_name, arg_value, this]<size_t Index>()
          {
            using member = ct::list::get_type<n_metadata_member_list<Struct>, Index>;
            using member_type = typename member::type;

            if (arg_name == member::name.string)
            {
              if constexpr (helper::from_string<member_type>::is_valid_type)
              {
                member_at<member_type, member::offset>(data).~member_type();
                bool success = true;
                new (&member_at<member_type, member::offset>(data))  member_type(helper::from_string<member_type>::convert(arg_value, success));

                if (success)
                  valid = true;
                else
                  cr::out().warn("option {} could not be correctly decoded", arg_name);
              }
              else
              {
                cr::out().warn("option {} cannot be converted from string: unknown type: {}", arg_name, ct::type_name<member_type>.str);
              }
              valid = true;
            }
          } .template operator()<Indices>(), ...);
        } (std::make_index_sequence<ct::list::size<n_metadata_member_list<Struct>>> {});
        return valid;
      }

      /// \brief expands shorthands to their full 
      /// \note Can only work for boolean args.
      /// \note shorthands requires a `std::map< char, std::string_view > shorthands` member
      bool process_shorthands(std::string_view list) requires concepts::HasShorthandMap<Struct>
      {
        bool valid = true;
        for (char c : list)
        {
          if (auto it = data.shorthands.find(c); it != data.shorthands.end())
          {
            if (!process_option(it->second))
            {
              valid = false;
              cr::out().warn("-{} : see previous message", c);
            }
          }
          else
          {
            valid = false;
            cr::out().warn("-{} : unknown shorthand", c);
          }
        }
        return valid;
      }
      bool process_shorthands(std::string_view list)
      {
        cr::out().warn("{}: shorthands are not supported for this type", list);
        return false;
      }

      /// \brief handles everything that does not match --abc / -abc or comes after a "--"
      /// \note require a `std::vector< std::string_view > parameters;` member in the struct
      bool process_parameter(std::string_view arg_value)
      {
        if constexpr (requires {requires concepts::HasParameterList<Struct>;})
        {
          data.parameters.push_back(arg_value);
          return true;
        }
        else
        {
          cr::out().warn("{}: type does not support parameters. Only options are accepted.", arg_value);
          return false;
        }
      }

      static void print_options()
      {
        Struct v;
        [&v]<size_t... Indices>(std::index_sequence<Indices...>)
        {
          ([&v]<size_t Index>()
          {
            using member = ct::list::get_type<n_metadata_member_list<Struct>, Index>;
            using member_type = typename member::type;

            std::string name = member::name.string;
            for (char& c : name) { if (c == '-') c = '_'; }

            // print the name:
            cr::out().log(" --{} (default value: `{}`, type: {})", name, member_at<member_type, member::offset>(v), ct::type_name<member_type>.str);
            // print some debug info if availlable:
            if constexpr (ct::list::has_type<std::remove_cvref_t<decltype(member::metadata_tuple())>, metadata::info::metadata>)
            {
              const metadata::info::metadata mt_info = std::get<metadata::info::metadata>(member::metadata_tuple());
              if (!mt_info.description.empty())
              {
                auto desc = neam::cr::split_string(mt_info.description, "\n");
                for (auto& it : desc)
                  cr::out().log("    {}", it);
              }
              if (!mt_info.doc_url.empty())
                cr::out().log("    documentation url: {}", mt_info.doc_url);

              if (!mt_info.description.empty() || !mt_info.doc_url.empty())
                cr::out().log(""); // empty line
            }
          } .template operator()<Indices>(), ...);
        } (std::make_index_sequence<ct::list::size<n_metadata_member_list<Struct>>> {});
      }

    private:
      bool check_opt_exists(std::string_view name)
      {
        bool found = false;
        [&found, name]<size_t... Indices>(std::index_sequence<Indices...>)
        {
          ([&found, name]<size_t Index>()
          {
            using member = ct::list::get_type<n_metadata_member_list<Struct>, Index>;

            if (name == member::name.string)
            {
              found = true;
            }
          } .template operator()<Indices>(), ...);
        } (std::make_index_sequence<ct::list::size<n_metadata_member_list<Struct>>> {});
        return found;
      }

      template<typename Type>
      bool check_opt_type(std::string_view name)
      {
        bool correct_type = false;
        [&correct_type, name]<size_t... Indices>(std::index_sequence<Indices...>)
        {
          ([&correct_type, name]<size_t Index>()
          {
            using member = ct::list::get_type<n_metadata_member_list<Struct>, Index>;
            if (name == member::name.string)
            {
              if constexpr (std::is_same_v<Type, typename member::type>)
              {
                correct_type = true;
              }
            }
          } .template operator()<Indices>(), ...);
        } (std::make_index_sequence<ct::list::size<n_metadata_member_list<Struct>>> {});
        return correct_type;
      }

      template<typename MT, size_t Offset>
      static MT& member_at(Struct& v)
      {
        uint8_t* ptr = reinterpret_cast<uint8_t*>(&v);
        return *reinterpret_cast<MT*>(ptr + Offset);
      }
      template<typename MT, size_t Offset>
      static const MT& member_at(const Struct& v)
      {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
        return *reinterpret_cast<const MT*>(ptr + Offset);
      }

    public:
      Struct data;
  };
}

