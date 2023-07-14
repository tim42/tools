//
// created by : Timothée Feuillet
// date: 2023-7-14
//
//
// Copyright (c) 2023 Timothée Feuillet
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

#include <map>

#include "../id/string_id.hpp"
#include "types.hpp"

namespace neam::threading
{
  /// \brief Named thread configuration struct.
  struct named_thread_configuration
  {
    // NOTE: Those flags applies only to tasks not tagged with a specific named-thread
    // Tasks that are tagged with this thread will take priority over those not
    // NOTE: Running a general tasks will be more costly for a named thread
    bool can_run_general_tasks = true;
    bool can_run_general_long_duration_tasks = false;
  };

  struct resolved_threads_configuration
  {
    std::map<id_t, named_thread_t> named_threads;
    std::map<group_t, std::string> debug_names;
    std::map<named_thread_t, named_thread_configuration> configuration;

    void print_debug();
  };

  class threads_configuration
  {
    public:
      named_thread_t add_named_thread(string_id id, named_thread_configuration conf = {});

      resolved_threads_configuration get_configuration() { return rtc; }
    private:
      resolved_threads_configuration rtc;
      named_thread_t named_thread_id = 1;
  };
}

