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
#include "named_threads.hpp"

#include "../memory_pool.hpp"
#include "../frame_allocation.hpp"
#include "../ring_buffer.hpp"
#include "../spinlock.hpp"
#include "../queue_ts.hpp"

#include <chrono>
#include <atomic>
#include <deque>

namespace neam::threading
{
  enum class task_selection_mode
  {
    normal, // as specified in the conf
    only_own_tasks, // if in a named thread, will prevent running any other tasks
    anything, // will allow running any tasks
  };

  /// \brief Handle tasks allocation
  class task_manager
  {
    public: // General stuff
      task_manager();

      /// \brief The frame lock will prevent the task graph from advancing keeping it locked in its current state.
      /// The only task group that can run is the non_transient_tasks
      /// Usefull during the app boot process to prevent task group to run while core resources are not yet loaded
      [[nodiscard]] spinlock& get_frame_lock()
      {
        return frame_state.frame_lock;
      }

      /// \brief Will stop the task manager next frame
      /// \note the frame_lock (get_frame_lock()) will be locked. Unlocking it will resume the operations.
      /// \note long-duration tasks are not affected by this and can continue to run
      void request_stop(function_t&& on_stopped, bool flush_all_delayed_tasks = false);

      bool try_request_stop(function_t&& on_stopped, bool flush_all_delayed_tasks = false);

      /// \brief Return whether the task manager is already tasked to stop this frame
      bool is_stop_requested() const;

      /// \brief When shutting-down the task manager, some threads may be in the wait_for_a_task function.
      ///        This cause them to exit that function without doing anything.
      /// \note Only work when the frame-lock is held
      void should_threads_exit_wait(bool should) { frame_state.should_threads_leave = should; }

      void _flush_all_delayed_tasks();

    public: // task group stuff. WARNING MUST BE CALLED BEFORE ANY CALL TO get_task()
      /// \brief Add the compiled frame operations
      /// \warning MUST BE CALLED BEFORE ANY OTHER OPERATION CAN BE DONE ON THE task_manager
      /// \warning NOT THREAD SAFE
      void add_compiled_frame_operations(resolved_graph&& _frame_ops, resolved_threads_configuration&& rtc);


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

      named_thread_t get_named_thread(id_t id) const
      {
        if (const auto it = named_threads_conf.named_threads.find(id); it != named_threads_conf.named_threads.end())
          return it->second;
        return k_invalid_named_thread;
      }

      void set_start_task_group_callback(group_t group, function_t&& fnc);
      void set_start_task_group_callback(id_t id, function_t&& fnc)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != 0xFF, "group name does not exists?");
        set_start_task_group_callback(group, std::move(fnc));
      }

      void set_end_task_group_callback(group_t group, function_t&& fnc);
      void set_end_task_group_callback(id_t id, function_t&& fnc)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != 0xFF, "group name does not exists?");
        set_end_task_group_callback(group, std::move(fnc));
      }

    public: // task stuff

      /// \brief allocate and construct a task
      /// deallocation is handled automatically
      task_wrapper get_task(group_t task_group, function_t&& func);

      task_wrapper get_task(id_t id, function_t&& func)
      {
        const group_t group = get_group_id(id);
        check::debug::n_assert(group != k_invalid_task_group, "group name does not exists?");
        return get_task(group, std::move(func));
      }

      /// \brief allocate and construct a task that can span multiple frames / be executed anywhere in a frame
      /// deallocation is handled automatically
      /// "long-duration" tasks does not belong to a given task-group and thus can be executed during any downtimes
      /// (including during a state-reset at the end of a "frame").
      /// This also means that only long-duration tasks can depend on other long-duration tasks.
      ///
      /// \warning Spawning any non-long duration tasks from within a long-duration task is undefined behavior and may lead to crashs
      task_wrapper get_long_duration_task(named_thread_t thread, function_t&& func);
      task_wrapper get_long_duration_task(id_t thread, function_t&& func)
      {
        const named_thread_t th = get_named_thread(thread);
        check::debug::n_assert(th != k_invalid_named_thread, "named thread does not exists?");
        return get_long_duration_task(th, std::move(func));
      }

      task_wrapper get_long_duration_task(function_t&& func) { return get_long_duration_task(k_no_named_thread, std::move(func)); }

      /// \brief allocate and construct a long-duration task that is guaranteed to not execute before delay has expired
      /// \note the actual execution time can be arbitrary long after the delay has been reached
      /// \see get_long_duration_task
      /// \warning Spawning any non-long duration tasks from within a long-duration task is undefined behavior and may lead to crashs
      /// \warning Should not be spammed as it does have a super high contention problem at task insertion and is very very slow overall
      /// \note delayed task will have the lowest priority if they didn't reach their delay at insertion
      task_wrapper get_delayed_task(std::chrono::milliseconds delay, function_t&& func);

      /// \brief tentatively run a task
      ///
      /// Can be safely called inside a task.
      /// (please note that the function does not have access to its task object and using it will deadlock)
      ///
      /// \note this allows a thread to participate on a task-by-task basis, without commiting the whole thread.
      ///       To commit a thread to run tasks, simply loop over that function.
      ///       (possibly calling wait_for_a_task to avoid spining)
      ///
      /// \note most compilers will directly jump to the task (and not perform a call), so this function may not appear in callstacks
      void run_a_task(bool exclude_long_duration = false, task_selection_mode mode = task_selection_mode::normal);

      /// \brief Wait for a task that can be run.
      /// May decide to sleep the thread for a while if necessary to avoid hogging the CPU
      ///
      /// Can be safely called inside a task.
      ///
      /// \note Is subject to the ABA issue in multiple ways, but this should not have any negative impacts.
      void wait_for_a_task();

      /// \brief Actively wait for a task to be completed (run tasks untill the task is completed)
      ///        Does not call to wait_for_a_task
      ///
      /// Can be safely called inside a task.
      ///
      /// \note If called from inside a task, it is incorrect to wait for a task from a different task-group
      ///       as it may lead to a deadlock.
      void actively_wait_for(group_t group, task_completion_marker_ptr_t&& t);

      void actively_wait_for(task_completion_marker_ptr_t&& t)
      {
        return actively_wait_for(get_current_group(), std::move(t));
      }

      /// \brief Actively wait for a task to be completed (run tasks untill the task is completed)
      ///        Does not call to wait_for_a_task
      ///
      /// Can be safely called inside a task.
      ///
      /// \note If called from inside a task, it is incorrect to wait for a task from a different task-group
      ///       as it may lead to a deadlock.
      void actively_wait_for(task_wrapper&& tw);

      /// \brief run tasks for the specified duration. (wait_for_a_task is not called)
      ///
      /// Can safely be called from within a task
      ///
      /// \note The system may either undershoot or overshoot the duration.
      /// \note The function may return early if there hasn't been any task to execute for a while
      /// \return the actual elapsed duration (as mesured internally)
      std::chrono::microseconds run_tasks(std::chrono::microseconds duration);

      /// \brief Returns whether tasks are pending or not
      /// Only tasks that can be actively executed in the current context are reported
      /// This does not include tasks that are pending but whose task-group is not yet started
      bool has_pending_tasks() const;

      bool has_running_tasks() const;

      /// \brief Return the number of pending tasks
      /// \see has_pending_tasks
      uint32_t get_pending_tasks_count() const;

      /// \brief Usefull for debugging what is continually inserting tasks when tasks should not be inserted
      void should_ensure_on_task_insertion(bool should_ensure) { frame_state.ensure_on_task_insertion = should_ensure; }

    public: // state stuff, must be called from within a task (or have a task in scope)
            // WARNING: ALL THOSE FUNCTIONS MIGHT BREAK IF THERE ARE MULTIPLE TASK_MANAGERS

      /// \brief Return the group of the task that is running on the current thread.
      /// If no task is running, returns k_invalid_task_group
      group_t get_current_group() const { return thread_state().current_gid; }

      /// \brief Return the group of the task that is running on the current thread.
      /// If no task is running, returns k_invalid_task_group
      named_thread_t get_current_thread() const { return thread_state().current_thread; }

      /// \brief Helper for get_task. Will run in the same task group as the current one.
      /// Must be called from within a task
      task_wrapper get_task(function_t&& func) { return get_task(get_current_group(), std::move(func)); }

      void _set_current_thread(named_thread_t thread) { thread_state().current_thread = thread; }
      void _set_current_thread(id_t thread) { thread_state().current_thread = get_named_thread(thread); }
      void _set_current_thread_index(uint8_t index) { thread_state().thread_index = index; }

      /// \brief Allocates a completion marker, usually used for getting completion status of tasks
      [[nodiscard]] task_completion_marker_ptr_t _allocate_completion_marker();
      void _deallocate_completion_marker_ptr(task_completion_marker_ptr_t&& ptr);

    public: // advanced internals
      void _advance_state() { advance(); }

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
      [[nodiscard]] task* get_task_to_run_internal(named_thread_t thread, bool exclude_long_duration = false);
      [[nodiscard]] task* get_task_to_run(named_thread_t thread, bool exclude_long_duration = false, task_selection_mode mode = task_selection_mode::normal);

      void reset_state();

      void poll_delayed_tasks(bool force_push = false);

      void do_run_task(task& task);

    private:
      // Tasks that belong to a task group. Deletion is done at the end of the frame (no need to manually deallocate tasks this way)
      // Tasks allocated this way must have a task group and must run inside the frame it is allocated.
      cr::frame_allocator<8, true> transient_tasks;

      // Tasks that can run on multiple frames / are lower priority (they are used to fill gaps)
      // Those tasks don't belong to a task group (or more explicitly belong to task group 0)
      // and needs to be manually deallocated upon completion
      // Completion within a frame is not guaranteed, tasks are lower priority than normal tasks
      cr::memory_pool<task> non_transient_tasks;

      cr::memory_pool<task_completion_marker_t, cr::raw_memory_pool_ts, 1> completion_marker_pool;

      resolved_graph frame_ops;
      resolved_threads_configuration named_threads_conf;

      struct group_info_t
      {
        cr::queue_ts<cr::queue_ts_atomic_wrapper<task*>> tasks_to_run;
        std::atomic<uint32_t> remaining_tasks = 0;
        std::atomic<bool> is_completed = false;
        std::atomic<bool> is_started = false;
        std::atomic<bool> will_start = false;

        named_thread_t required_named_thread = k_no_named_thread;

        // held temporarily to avoid wasting cpu time spinning waiting for a group to start
        std::atomic<uint32_t> tasks_that_can_run = 0;

        function_t start_group;
        function_t end_group;
      };

      struct chain_info_t
      {
        bool ended = false;
        uint16_t index = 0;
        spinlock lock;
      };

      struct named_thread_frame_state_t
      {
        named_thread_configuration configuration;
        std::vector<group_t> groups;

        cr::queue_ts<cr::queue_ts_atomic_wrapper<task*>> long_duration_tasks_to_run;
        std::atomic<uint32_t> tasks_that_can_run = 0;
      };

      struct delayed_task_t
      {
        task* ptr;
        auto operator <=> (const delayed_task_t& o) const
        {
          const auto x = ptr->execution_time_point <=> o.ptr->execution_time_point;
          [[likely]] if (x != 0)
            return x;
          return ptr <=> o.ptr;
        }
      };
      struct sorted_task_list_t
      {
        // FIXME: something better (probably bucket-based with very coarse/exponential sorting).
        //        currently, it's so very slow.
        std::set<delayed_task_t> delayed_tasks;
        spinlock lock; // the contention problem
      };

      struct frame_state_t
      {
        std::deque<group_info_t> groups;
        std::deque<chain_info_t> chains;
        std::deque<named_thread_frame_state_t> threads;

        sorted_task_list_t delayed_tasks;

        std::atomic<uint32_t> running_tasks = 0;
        std::atomic<uint32_t> running_transient_tasks = 0;
        std::atomic<uint32_t> ended_chains = 0;

        // so we can avoid spinning the advance function and instead wait
        std::atomic<uint32_t> global_state_key = 0;

        std::atomic<uint32_t> frame_key = 0;

        // used to stop the frame from progressing
        // (usefull during the boot process)
        spinlock frame_lock;

        mutable shared_spinlock stopping_lock; // protects on-stopped and should-stop
        function_t on_stopped;
        bool should_stop = false;
        bool ensure_on_task_insertion = false;
        bool should_threads_leave = false; // require the frame-lock to be held, will cause threads to exit
      };
      frame_state_t frame_state;

      struct thread_state_t
      {
        named_thread_t current_thread = k_no_named_thread;
        group_t current_gid = k_invalid_task_group;
        uint8_t thread_index = 0xFF;
      };

      static thread_state_t& thread_state()
      {
        thread_local thread_state_t state;
        return state;
      }

      friend class task;
  };
}

