//
// file : backtrace.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/backtrace.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 13/02/2015 17:02:08
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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

#ifndef __N_1145625880472195080_2067372013__BACKTRACE_HPP__
# define __N_1145625880472195080_2067372013__BACKTRACE_HPP__

#ifdef __linux__
#include <execinfo.h>
#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "logger/logger.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief print the current callstack
    /// (and demangle functions name, and print a lovely addr2line command that you can use to get the line number)
    /// \param[in] backtrace_size the depth of the backtrace: the number of entries to print
    /// \param[in] skip the number of entries to skip from printing. (the N first entries to skip).
    /// \note currently only on LINUX
    static inline void print_callstack(size_t backtrace_size = 25, size_t skip = 1)
    {
#ifdef __linux__
      static std::atomic<uint32_t> counter(0);

      char **strings;
      void *bt[backtrace_size];

      ++counter;
      int num = backtrace(bt, backtrace_size);
      strings = backtrace_symbols(bt, num);

      // acquire the logger lock
      auto &stream = neam::cr::out.warning();
      stream << "#############[  B A C K T R A C E  ]#############" << neam::cr::newline
             << neam::cr::newline
             << "[ most recent call first: ]" << neam::cr::newline
             << neam::cr::newline;

      for (int j = skip; j < num; ++j)
      {
        // extract the symbol from the string
        int i = 0;
        int k = 0;
        for (; strings[j][i] && strings[j][i] != '('; ++i);
        if (strings[j][i]) ++i;
        for (; strings[j][i + k] && strings[j][i + k] != '+' && strings[j][i + k] != ')'; ++k);
        char *mangled = strndup(strings[j] + i, k);

        // demangle the symbol
        int status;
        char *realname = abi::__cxa_demangle(mangled, 0, 0, &status);
        if (status)
          realname = mangled;
        else
          free(mangled);

        // print
        stream << "[" << num - j << "] ";

        if (realname[0] != '\0')
          stream << realname << neam::cr::newline << "    ";

        stream << strings[j] << neam::cr::newline;

        // and free
        free(realname);
      }
      stream << neam::cr::newline
             << "## addr2line -e " << program_invocation_name << " -fipsC ";

      // print the indexes
      for (int j = skip; j < num; ++j)
      {
        // extract the address from the string
        int i = 0;
        int k = 0;
        for (; strings[j][i] && strings[j][i] != '['; ++i);
        if (strings[j][i]) ++i;
        for (; strings[j][i + k] && strings[j][i + k] != ']'; ++k);
        char *fnc_addr = strndup(strings[j] + i, k);

        stream << fnc_addr << " ";
        free(fnc_addr);
      }

      stream << neam::cr::newline << neam::cr::newline
             << "########[  B A C K T R A C E     E N D  ]########" << std::endl;
      // release the lock.

      free(strings);
#endif
    }
  }
} // namespace neam

#endif /*__N_1145625880472195080_2067372013__BACKTRACE_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on;

