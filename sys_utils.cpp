
//
// created by : Timothée Feuillet
// date: 2022-7-15
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

#include "sys_utils.hpp"

#ifdef __unix__
  #include <unistd.h>
  #include <sys/wait.h>
  #include <sched.h>
#elif defined(_WIN32)
  #include <windows.h>
  #include <stringapiset.h>
#endif

namespace neam::sys
{
#if defined(_WIN32)
  static std::wstring convert_to_ps(const std::string& as)
  {
    // deal with trivial case of empty string
    if (as.empty()) return std::wstring();

    // determine required length of new string
    size_t reqLength = ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), 0, 0);

    // construct new string of required length
    std::wstring ret(reqLength, L'\0');

    // convert old string to new string
    ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), &ret[0], (int)ret.length());

    // return new string ( compiler should optimize this away )
    return ret;
  }
#endif

  void open_url(const std::string& url)
  {
#ifdef __unix__
    const pid_t child = fork();
    if (child == 0)
    {
      const char* args[] =
      {
#ifdef __APPLE__
        "open",
#else
        "xdg-open",
#endif
        url.c_str(),
        nullptr,
      };
      execvp(args[0], (char**)args);
    }
    int r;
    waitpid(child, &r, 0);
#elif define(_WIN32)
    ShellExecute(0, 0, convert_to_ps(url).c_str(), 0, 0, SW_SHOW);
#endif
  }


  void set_cpu_affinity(uint32_t thread_index)
  {
#ifdef __unix__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(thread_index, &set);
    sched_setaffinity(0, sizeof(cpu_set_t), &set);
#endif
  }
}

