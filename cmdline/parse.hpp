//
// created by : Timothée Feuillet
// date: 2021-12-12
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

#include "arg_struct.hpp"

namespace neam::cmdline
{
  /// \brief parse the command line args and fill structs
  class parse
  {
    public:
      parse(int _argc, char** _argv) : argc((unsigned)_argc), argv(_argv)
      {
        skip(1); // skip the program name
      }

      bool has_remaining_args() const { return index < argc; }

      bool skip(unsigned count)
      {
        index += count;
        return index < argc;
      }

      /// \brief Do the processing
      /// \param parameters_to_parse how many parameters to parse.
      ///                           Usefull when there's a set of global options,
      ///                           a command-name and then a set of command options
      template<metadata::concepts::StructWithMetadata Struct>
      Struct process(bool& success, unsigned parameters_to_parse = ~0u)
      {
        success = true;
        arg_struct<Struct> helper;
        // set to true when encountering `--`, force everything else to be treated as parameters
        bool force_parameters = false;
        unsigned parameter_count = 0;
        for (; index < argc; ++index)
        {
          const std::string_view arg = argv[index];
          if (!force_parameters)
          {
            if (arg == "--")
            {
              force_parameters = true;
              continue;
            }
            if (arg.starts_with("--"))
            {
              const size_t eq_pos = arg.find('=');
              const std::string_view orig_arg_name_sv = arg.substr(0, eq_pos).substr(2);
              std::string arg_name = std::string(orig_arg_name_sv.data(), orig_arg_name_sv.size());
              // replace all - to _
              for (char& c : arg_name) { if (c == '-') c = '_'; }

              if (eq_pos != std::string_view::npos)
              {
                // we have a value
                const std::string_view arg_value = arg.substr(eq_pos + 1);
                if (!helper.process_option(arg_name, arg_value))
                  success = false;
              }
              else
              {
                // it's a boolean parameter
                if (!helper.process_option(arg_name))
                  success = false;
              }
              continue;
            }
            else if (arg.starts_with("-") && arg != "-")
            {
              const std::string_view arg_list = arg.substr(1);
              if (!helper.process_shorthands(arg_list))
                success = false;
              continue;
            }
          }

          if (parameter_count >= parameters_to_parse)
            break;

          ++parameter_count;

          if (!helper.process_parameter(arg))
            success = false;

          if (parameter_count >= parameters_to_parse)
          {
            ++index;
            break;
          }
        }
        return helper.data;
      }

    private:
      unsigned argc = 0;
      char** argv = nullptr;

      unsigned index = 0;
  };
}

