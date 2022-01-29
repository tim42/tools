//
// created by : Timothée Feuillet
// date: 2022-1-2
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

#include "types.hpp"
#include "task.hpp"
#include "task_group_graph.hpp"

#include "../memory_pool.hpp"
#include "../frame_allocation.hpp"
#include "../ring_buffer.hpp"
#include "../spinlock.hpp"

#include <chrono>
#include <atomic>
#include <deque>

namespace neam::threading
{
  /// \brief Handle tasks allocation
  class task_manager
  {
    public: // General stuff
      task_manager() = default;

    public: // task group stuff. WARNING MUST BE CALLED BEFORE ANY CALL TO get_task()
      /// \brief Add the compiled frame operations
      /// \warning MUST BE CALLED BEFORE ANY OTHER OPERATION CAN BE DONE ON THE task_manager
      /// \warning NOT THREAD SAFE
      void add_compiled_frame_operations(resolved_graph&& _frame_ops);

      bool has_group(id_t id) const
      {
        if (const auto it = frame_ops.groups.find(id); it != frame_ops.groups.end())
          return true;
        return false;
      }

      group_t get_group_id(id_t id) const
      {
        if (const auto it = frame_ops.groups.find(id); it != frame_ops.groups.end())
          return it->second;
        return k_invalid_task_group;
      }


      void set_start_task_group_callback(group_t group, const std::function<void()>& fnc);
      void set_start_task_group_callback(id_t id, const std::function<void()>& fnc)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != 0xFF, "group name does not exists?");
        set_start_task_group_callback(group, fnc);
      }

      void set_end_task_group_callback(group_t group, const function_t& fnc);
      void set_end_task_group_callback(id_t id, const function_t& fnc)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != 0xFF, "group name does not exists?");
        set_end_task_group_callback(group, fnc);
      }

    public: // task stuff

      /// \brief allocate and construct a task
      /// deallocation is handled automatically
      task_wrapper get_task(group_t task_group, function_t&& func);

      task_wrapper get_task(id_t id, function_t&& func)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != 0xFF, "group name does not exists?");
        return get_task(group, std::move(func));
      }

      /// \brief allocate and construct a task that can span multiple frames / be executed anywhere in a frame
      /// deallocation is handled automatically
      task_wrapper get_long_duration_task(function_t&& func);

      /// \brief run a task
      /// \note this allows a thread to participate on a task-by-task basis, without commiting the whole thread.
      /// To commit a thread to run tasks, simply loop over that function.
      void run_a_task();

      /// \brief Wait for a task that can be run
      /// Does (should) not spin
      void wait_for_a_task();

      /// \brief run tasks for the specified duration.
      /// \note The system may either undershoot or overshoot the duration.
      /// \return the actual elapsed duration (as mesured internally)
      std::chrono::microseconds run_tasks(std::chrono::microseconds duration);

    private:
      /// \brief Mark the setup of the task as complete and allow it to run (avoid race-conditions in the setup)
      /// \note Creating a task an not adding it will cause the task to never run (if it's a non-transient_tasks)
      ///       or a deadlock (threads will wait for the end of a task group, but the task is never pushed to run)
      /// using a task raii wrapper to automatically push the task at the end of scope is the preferred way to deal with that.
      void add_task_to_run(task& t);

      void destroy_task(task& t);

      // Try to advance the frame-state
      // Return true if a task was executed in the process of advancing the graph
      bool advance();

      /// \brief Get a task to run
      /// \note The task is selected in a somewhat random fashion: the first task found is the selected one
      task* get_task_to_run();

      void reset_state();

    private:
      spinlock alloc_lock; // TODO: remove

      // Tasks that belong to a task group. Deletion is done at the end of the frame (no need to manually deallocate tasks this way)
      // Tasks allocated this way must have a task group and must run inside the frame it is allocated.
      cr::frame_allocator<1024 * sizeof(task), true> transient_tasks;

      // Tasks that can run on multiple frames / are lower priority (they are used to fill gaps)
      // Those tasks don't belong to a task group (or more explicitly belong to task group 0)
      // and needs to be manually deallocated upon completion
      // Completion within a frame is not guaranteed, tasks are lower priority than normal tasks
      cr::memory_pool<task> non_transient_tasks;

      resolved_graph frame_ops;

      struct group_info_t
      {
        static constexpr uint32_t k_task_to_run_size = 8;
        cr::ring_buffer<task*, 2048> tasks_to_run[k_task_to_run_size];
        std::atomic<uint32_t> insert_buffer_index = 0;
        std::atomic<uint32_t> remaining_tasks = 0;
        std::atomic<bool> is_completed = false;
        std::atomic<bool> is_started = false;

        std::function<void()> start_group;
        std::function<void()> end_group;
      };

      struct chain_info_t
      {
        bool ended = false;
        uint16_t index = 0;
      };

      struct frame_state_t
      {
        std::deque<group_info_t> groups;
        std::deque<chain_info_t> chains;

        alignas(64) std::atomic<uint32_t> tasks_that_can_run = 0;

        uint32_t frame_key = 0;
        spinlock lock;
      };
      frame_state_t frame_state;

      friend class task;
  };
}

