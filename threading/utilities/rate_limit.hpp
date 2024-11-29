//
// created by : Timothée Feuillet
// date: 2024-6-9
//
//
// Copyright (c) 2024 Timothée Feuillet
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

#include "../types.hpp"
#include "../../spinlock.hpp"
#include "../../mt_check/deque.hpp"

namespace neam::threading
{
  class rate_limiter
  {
    public:
      rate_limiter(task_manager& _tm) : tm(_tm) {}

      void dispatch(group_t group, function_t&& function, bool high_priority = false);
      void set_max_in_flight_tasks(uint32_t max);

      void enable(bool _enabled);

    private:
      struct waiting_task_t
      {
        function_t function;
        group_t task_group;
      };
    private:
      void do_dispatch(waiting_task_t&& task);


      // FIXME: investigate a way to do this with minimal locks
      spinlock lock;
      std::deque<waiting_task_t> to_dispatch_normal_priority;
      std::deque<waiting_task_t> to_dispatch_high_priority;
      uint32_t dispatched_task_count = 0;
      bool enabled = true;

      uint32_t max_in_flight_tasks = 16;
      task_manager& tm;
  };
}
