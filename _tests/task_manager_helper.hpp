//
// created by : Timothée Feuillet
// date: 2023-9-22
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

#include "../threading/threading.hpp"

namespace neam
{
  class tm_helper_t
  {
    public:
      threading::task_manager tm;

    public:
      void setup(uint32_t thread_count, threading::task_group_dependency_tree&& tgd)
      {
        tm.add_compiled_frame_operations(tgd.compile_tree(), {});
        threads.reserve(thread_count);
        should_stop = false;

        for (unsigned i = 0; i < thread_count; ++i)
        {
          threads.emplace_back([this]
          {
            while (!should_stop)
            {
              tm.wait_for_a_task();
              tm.run_a_task();
            }
          });
        }
      }

      // Enroll the main thread + launch the task manager
      void enroll_main_thread()
      {
        tm._advance_state();
        while (!should_stop)
        {
          tm.wait_for_a_task();
          tm.run_a_task();
        }
      }

      // Can be called from anywhere, request a stop from the task-manager
      void request_stop()
      {
        tm.request_stop([this]
        {
          should_stop = true;
          tm.should_threads_exit_wait(true);
          // Run remaining long duration tasks
          while (tm.has_pending_tasks() || tm.has_running_tasks())
          {
            tm.run_a_task();
          }
          tm.should_ensure_on_task_insertion(true);
        }, true);
      }

      // Must be called from the main thread
      void join_all_threads()
      {
        for (auto& it : threads)
        {
          if (it.joinable())
            it.join();
        }

        tm.get_frame_lock()._unlock();
      }

    private:
      bool should_stop = false;
      std::vector<std::thread> threads;
  };
}

