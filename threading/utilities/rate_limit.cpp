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

#include "rate_limit.hpp"

#include "../task_manager.hpp"

namespace neam::threading
{
  void rate_limiter::dispatch(group_t group, function_t&& function, bool high_priority)
  {
    waiting_task_t task { std::move(function), group };

    if (enabled)
    {
      if (max_in_flight_tasks != 0)
      {
        std::lock_guard _l{lock};
        if (dispatched_task_count >= max_in_flight_tasks)
        {
          if (high_priority)
            to_dispatch_high_priority.emplace_back(std::move(task));
          else
            to_dispatch_normal_priority.emplace_back(std::move(task));
          return;
        }
      }

      // do the dispatch outside the lock (we don't need a lock for this)
      ++dispatched_task_count;
    }

    do_dispatch(std::move(task));
  }

  void rate_limiter::do_dispatch(waiting_task_t&& task)
  {
    tm.get_task(task.task_group, [this, func = std::move(task.function), was_enabled = enabled] () mutable
    {
      func();

      if (enabled && was_enabled)
      {
        std::lock_guard _l{lock};
        if ((!to_dispatch_high_priority.empty() || !to_dispatch_normal_priority.empty()) && dispatched_task_count <= max_in_flight_tasks)
        {
          // dispatch the next task, keep dispatched_task_count the same.
          // NOTE: We should call do_dispatch outside the lock...
          if (!to_dispatch_high_priority.empty())
          {
            do_dispatch(std::move(to_dispatch_high_priority.front()));
            to_dispatch_high_priority.pop_front();
          }
          else
          {
            do_dispatch(std::move(to_dispatch_normal_priority.front()));
            to_dispatch_normal_priority.pop_front();
          }
          return;
        }
        --dispatched_task_count;
      }
    });
  }

  void rate_limiter::enable(bool _enabled)
  {
    std::lock_guard _l{lock};
    if (enabled != _enabled)
    {
      enabled = _enabled;
      dispatched_task_count = 0;
      if (!enabled)
      {
        // flush every tasks:
        for (auto& it : to_dispatch_high_priority)
          do_dispatch(std::move(it));
        to_dispatch_high_priority.clear();

        for (auto& it : to_dispatch_normal_priority)
          do_dispatch(std::move(it));
        to_dispatch_normal_priority.clear();
      }
    }
  }

  void rate_limiter::set_max_in_flight_tasks(uint32_t max)
  {
    std::lock_guard _l{lock};
    max_in_flight_tasks = max;
    if (dispatched_task_count >= max_in_flight_tasks || (to_dispatch_high_priority.empty() && to_dispatch_normal_priority.empty()))
      return;

    // immeditaly fill the new quota if we have more tasks to dispatch
    while ((dispatched_task_count < max_in_flight_tasks || !max_in_flight_tasks) && !to_dispatch_high_priority.empty())
    {
      ++dispatched_task_count;
      // NOTE: We should call do_dispatch outside the lock...
      do_dispatch(std::move(to_dispatch_high_priority.front()));
      to_dispatch_high_priority.pop_front();
    }
    while ((dispatched_task_count < max_in_flight_tasks || !max_in_flight_tasks) && !to_dispatch_normal_priority.empty())
    {
      ++dispatched_task_count;
      // NOTE: We should call do_dispatch outside the lock...
      do_dispatch(std::move(to_dispatch_normal_priority.front()));
      to_dispatch_normal_priority.pop_front();
    }
  }
}
