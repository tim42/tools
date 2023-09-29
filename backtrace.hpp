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

#pragma once
# define __N_1145625880472195080_2067372013__BACKTRACE_HPP__

#ifdef __linux__
#include <execinfo.h>
#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>
#include <link.h>
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
    static inline void print_callstack(size_t backtrace_size = 25, size_t skip = 1, bool has_logger_lock = false)
    {
#ifdef __linux__
      char **strings;
      void *bt[backtrace_size + skip];
      int num = backtrace(bt, backtrace_size + skip);
      strings = backtrace_symbols(bt, num);

      auto logger = neam::cr::out(has_logger_lock);
      logger.warn("#############[  B A C K T R A C E  ]#############"); 
      logger.warn("## most recent call first:"); 

      std::vector<std::string> addr;
      void* last_fname = nullptr;
      addr.reserve(num);
      for (int j = skip; j < num; ++j)
      {
        int i = 0;
        int k = 0;
        // extract the symbol from the string
        for (; strings[j][i] && strings[j][i] != '('; ++i);
        if (strings[j][i]) ++i;
        for (; strings[j][i + k] && strings[j][i + k] != '+' && strings[j][i + k] != ')'; ++k);
        char *mangled = strndup(strings[j] + i, k);

        // extract the address from the string
        for (; strings[j][i] && strings[j][i] != '['; ++i);
        if (strings[j][i]) ++i;
        for (; strings[j][i + k] && strings[j][i + k] != ']'; ++k);
        uint64_t fnc_addr = strtoul(strings[j] + i, nullptr, 0);

        Dl_info info;
        link_map* map;
        dladdr1((void*)fnc_addr, &info, (void**)&map, RTLD_DL_LINKMAP);
        if (last_fname != info.dli_fname || addr.empty())
          addr.push_back(fmt::format(" addr2line -e {} -fipsC ", info.dli_fname));
        addr.back() = fmt::format("{} {}", addr.back(), (void*)(fnc_addr - map->l_addr));
        last_fname = (void*)info.dli_fname;

        // demangle the symbol
        int status;
        char buffer[1024];
        size_t len = 1024;
        char *realname = abi::__cxa_demangle(mangled, buffer, &len, &status);
        if (status)
          realname = mangled;

        // print
        logger.warn("  [{:3}]: {}\t{}", num - j, realname, strings[j]);

        // and free
        free(mangled);
      }

      // logger.warn("## addr2line -e {} -fipsC {}", program_invocation_name, fmt::join(addr, " "));
      logger.warn("##  {}", fmt::join(addr, " ; "));

      logger.warn("########[  B A C K T R A C E     E N D  ]########");

      free(strings);
#endif
    }
  }
} // namespace neam


// kate: indent-mode cstyle; indent-width 2; replace-tabs on;

