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

#include "task_manager.hpp"

#include "../scoped_flag.hpp"
#include "../tracy.hpp"

namespace neam::threading
{
  task_manager::task_manager()
  {
    frame_state.groups.resize(1);
    frame_state.threads.resize(1);

    transient_tasks.pool_debug_name = "task_manager::transient_tasks pool";
    non_transient_tasks.pool_debug_name = "task_manager::non_transient_tasks pool";
    completion_marker_pool.pool_debug_name = "task_manager::completion_marker pool";

    TRACY_PLOT_CONFIG_EX_CSTR("task_manager::waiting_tasks", tracy::PlotFormatType::Number, true, 0x7f1111ff);
    TRACY_PLOT_CONFIG_EX_CSTR("task_manager::delayed_tasks", tracy::PlotFormatType::Number, true, 0x7f117fff);
  }

  void task_manager::request_stop(function_t&& on_stopped, bool flush_all_delayed_tasks)
  {
    std::lock_guard _ul(spinlock_exclusive_adapter::adapt(frame_state.stopping_lock));
    frame_state.should_stop = true;
    check::debug::n_check(!frame_state.on_stopped, "task_manager::request_stop: stop already requested, with a fallback already registered. This is undefined behavior.");
    frame_state.on_stopped = std::move(on_stopped);
    if (flush_all_delayed_tasks)
      poll_delayed_tasks(true);
  }

  bool task_manager::try_request_stop(function_t&& on_stopped, bool flush_all_delayed_tasks)
  {
    std::lock_guard _ul(spinlock_exclusive_adapter::adapt(frame_state.stopping_lock));
    if (frame_state.should_stop)
      return false;
    frame_state.should_stop = true;
    frame_state.on_stopped = std::move(on_stopped);
    if (flush_all_delayed_tasks)
      poll_delayed_tasks(true);
    return true;
  }

  bool task_manager::is_stop_requested() const
  {
    std::lock_guard _ul(spinlock_shared_adapter::adapt(frame_state.stopping_lock));
    return frame_state.should_stop;
  }

  void task_manager::_flush_all_delayed_tasks()
  {
    poll_delayed_tasks(true);
  }


  void task_manager::set_start_task_group_callback(group_t group, function_t&& fnc)
  {
    check::debug::n_assert(group < frame_state.groups.size(), "group {} does not exists", group);
    frame_state.groups[group].start_group = std::move(fnc);
  }

  void task_manager::set_end_task_group_callback(group_t group, function_t&& fnc)
  {
    check::debug::n_assert(group < frame_state.groups.size(), "group {} does not exists", group);
    frame_state.groups[group].end_group = std::move(fnc);
  }

  task_wrapper task_manager::get_task(group_t task_group, function_t&& func)
  {
    check::debug::n_assert(task_group < frame_state.groups.size(), "Trying to create a task from a task group that does not exists. Did you forgot to create a task group?");
    check::debug::n_check(!frame_state.ensure_on_task_insertion, "task for task-group {} created while the ensure flag is on", task_group);

    if (task_group == k_non_transient_task_group)
      return get_long_duration_task(std::move(func));

    frame_state.groups[task_group].remaining_tasks.fetch_add(1, std::memory_order_release);

    check::debug::n_assert(frame_state.groups[task_group].is_completed == false, "Trying to create a task from a completed group");

    task* ptr = (task*)transient_tasks.allocate(sizeof(task));
    new (ptr) task(*this, task_group, frame_state.groups[task_group].required_named_thread, frame_state.frame_key, std::move(func));
    return *ptr;
  }

  task_wrapper task_manager::get_long_duration_task(named_thread_t thread, function_t&& func)
  {
    check::debug::n_check(!frame_state.ensure_on_task_insertion, "long-duration task created while the ensure flag is on");
    frame_state.groups[k_non_transient_task_group].remaining_tasks.fetch_add(1, std::memory_order_release);

    task* ptr = non_transient_tasks.allocate();
    check::debug::n_assert(ptr != nullptr, "Failed to allocate a task");

    new (ptr) task(*this, k_non_transient_task_group, thread, frame_state.frame_key, std::move(func));
    return *ptr;
  }

  task_wrapper task_manager::get_delayed_task(std::chrono::milliseconds delay, function_t&& func)
  {
    const auto execution_time_point = std::chrono::high_resolution_clock::now() + delay;
    task_wrapper tw = get_long_duration_task(std::move(func));
    if (!frame_state.frame_lock._get_state() || frame_state.should_stop)
      tw.get_task().execution_time_point = execution_time_point;
    return tw;
  }

  void task_manager::destroy_task(task& t)
  {
    TRACY_SCOPED_ZONE;
    const group_t group = t.key;
    check::debug::n_assert(group < frame_state.groups.size(), "Trying to destroy a task from a task-group that does not exist");
    check::debug::n_assert(t.is_completed(), "Trying to destroy a task that wasn't completed");

    if (group != k_non_transient_task_group)
    {
      check::debug::n_assert(t.get_frame_key() == frame_state.frame_key.load(std::memory_order_acquire), "Trying to destroy a task that outlived its lifespan");
    }

    t.~task();
    if (group == k_non_transient_task_group)
    {
      non_transient_tasks.deallocate(&t);
    }

    const uint32_t orig_count = frame_state.groups[group].remaining_tasks.fetch_sub(1, std::memory_order_acquire);
    check::debug::n_assert(orig_count > 0, "Invalid task count: Trying to decrement the task count when it was 0 (task group: {})", group);

    if (orig_count <= 1 && group != k_non_transient_task_group)
    {
      // increment the global state key as we are the last task pending for that group
      frame_state.global_state_key.fetch_add(1, std::memory_order_relaxed);
    }
  }

  task_completion_marker_ptr_t task_manager::_allocate_completion_marker()
  {
    task_completion_marker_t* ptr = completion_marker_pool.allocate();
    return { ptr, this };
  }

  void task_manager::_deallocate_completion_marker_ptr(task_completion_marker_ptr_t&& ptr)
  {
    check::debug::n_check(ptr.is_completed(), "task_completion_marker_ptr_t: cannot destroy a non-completed marker.");
    completion_marker_pool.deallocate(ptr.ptr.release());
  }

  void task_manager::add_task_to_run(task& t)
  {
    const bool has_delay = t.execution_time_point != decltype(t.execution_time_point){};
    check::debug::n_assert(t.unlock_is_completed() == false, "Trying to push an already completed task");
    check::debug::n_assert(t.unlock_is_waiting_to_run() == false, "Trying to push an already waiting task");
    check::debug::n_assert(t.key < (uint32_t)frame_state.groups.size(), "Invalid task group type (group: {})", t.key);
    check::debug::n_assert(t.thread_key < (uint32_t)frame_state.threads.size(), "Invalid named thread (thread: {})", t.thread_key);
    if (t.key != k_non_transient_task_group)
    {
      check::debug::n_assert(t.get_frame_key() == frame_state.frame_key, "Trying to push a task to run when that task has outlived its lifespan");
      check::debug::n_assert(frame_state.groups[t.key].is_completed == false, "Trying to push a task to a completed group");
      check::debug::n_assert(!has_delay, "Trying to push a non-long duration task with an execution delay");
      check::debug::n_assert(t.thread_key == frame_state.groups[t.key].required_named_thread, "Incorrect named thread for task. Should be {}, but instead is {}",
                             frame_state.groups[t.key].required_named_thread, t.thread_key);
    }
    // skip tasks that cannot run, as they will be added automatically
    // this check is necessary as this function can be called by a user
    if (!t.unlock_can_run()) return;


    if (t.key == k_non_transient_task_group && has_delay)
    {
      const auto now = std::chrono::high_resolution_clock::now();
      if (now < t.execution_time_point)
      {
        TRACY_SCOPED_ZONE;

        // Add the task to the sorted list. Will generate a lot of contention if called a lot.
        std::lock_guard _lg(frame_state.delayed_tasks.lock);
        frame_state.delayed_tasks.delayed_tasks.emplace(&t);
        TRACY_PLOT_CSTR("task_manager::delayed_tasks", (int64_t)frame_state.delayed_tasks.delayed_tasks.size());
        return;
      }
      // we have reached the delay already, so we can proceed as normal
    }

    t.set_task_as_waiting_to_run();

    // don't wake waiting thread when the task is not really usable (group hasn't started yet)
    if (t.key == k_non_transient_task_group
        || frame_state.groups[t.key].will_start.load(std::memory_order_seq_cst)
        || frame_state.groups[t.key].is_started.load(std::memory_order_seq_cst))
    {
      [[maybe_unused]] const uint32_t count = frame_state.threads[t.thread_key].tasks_that_can_run.fetch_add(1, std::memory_order_release);
      TRACY_PLOT_CSTR("task_manager::waiting_tasks", (int64_t)(count + 1));
    }
    else
    {
      // The lack of atomicity here is handled in the execute group (in advance())
      frame_state.groups[t.key].tasks_that_can_run.fetch_add(1, std::memory_order_seq_cst);
    }

    if (t.key == k_non_transient_task_group && t.thread_key != k_no_named_thread)
    {
      frame_state.threads[t.thread_key].long_duration_tasks_to_run.push_back(&t);
    }
    else
    {
      frame_state.groups[t.key].tasks_to_run.push_back(&t);
    }
  }

  void task_manager::add_compiled_frame_operations(resolved_graph&& _frame_ops, resolved_threads_configuration&& rtc)
  {
    frame_ops = std::move(_frame_ops);
    named_threads_conf = std::move(rtc);

    // chains:
    frame_state.chains.resize(frame_ops.chain_count);
    for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
    {
      check::debug::n_assert(frame_ops.opcodes[i].opcode == ir_opcode::declare_chain_index, "Invalid frame operation: expected declare_chain_index opcode, got {:X}", std::to_underlying(frame_ops.opcodes[i].opcode));
      frame_state.chains[i].index = frame_ops.opcodes[i].arg;
    }

    // groups: (TODO: add a thread/group count)
    uint32_t max_group = 0;
    for (const auto& it : frame_ops.groups)
    {
      max_group = std::max<uint32_t>(max_group, it.second);
    }
    uint32_t max_thread = 0;
    for (const auto& it : named_threads_conf.named_threads)
    {
      max_thread = std::max<uint32_t>(max_thread, it.second);
    }

    frame_state.groups.resize(max_group + 1);
    frame_state.threads.resize(max_thread + 1);

    for (const auto& it : frame_ops.groups)
    {
      const auto& conf = frame_ops.configuration.at(it.second);
      if (conf.restrict_to_named_thread != id_t::none)
      {
        if (auto thit = named_threads_conf.named_threads.find(conf.restrict_to_named_thread); thit != named_threads_conf.named_threads.end())
        {
          frame_state.groups[it.second].required_named_thread = thit->second;
          frame_state.threads[thit->second].groups.push_back(it.second);
        }
        else
        {
          check::debug::n_assert(false, "Invalid named thread requirement: unknown named thread {} in group {}", conf.restrict_to_named_thread, it.second);
        }
      }
      else
      {
        frame_state.threads[0].groups.push_back(it.second);
      }
    }

    for (const auto& it : named_threads_conf.named_threads)
    {
      frame_state.threads[it.second].configuration = named_threads_conf.configuration[it.second];
    }

    frame_state.groups[k_non_transient_task_group].is_started = true;
  }

  bool task_manager::advance()
  {
    // check that we can advance
    if (frame_state.frame_lock._get_state())
    {
      // the lock is locked, we don't advance.
      return false;
    }

    // Avoid spamming advance() and create lock contention
    thread_local uint32_t last_global_state_key = ~0u;
    const uint32_t global_state_key = frame_state.global_state_key.load(std::memory_order_acquire);
    if (last_global_state_key == global_state_key)
      return false;
    last_global_state_key = global_state_key;

    TRACY_SCOPED_ZONE_COLOR(0x0000FF);

    constexpr uint32_t k_function_complexity = 20;

    // past this threshold, run_a_task will return after the call to advance()
    // advance can have callbacks that are non-trivial and those can count as a task
    constexpr uint32_t k_complexity_threshold = 19;

    // arbitrary indicator of the work done here:
    uint32_t complexity = 0;

    // whether we should have the duty to end the frame and reset the state
    bool should_reset_state = false;

    if (frame_state.ended_chains.load(std::memory_order_acquire) != frame_ops.chain_count)
    {
      for (int32_t chain_index = 0; chain_index < (int32_t)frame_state.chains.size(); ++chain_index)
      {
        auto& chain = frame_state.chains[chain_index];
        bool start_again = false;
        {
          // if (!chain.lock.try_lock())
            // continue;
          std::lock_guard _lg(chain.lock/*, std::adopt_lock*/);
          if (chain.ended)
            continue;

          bool loop = true;
          while (loop)
          {
            ++complexity;

            const ir_opcode& op = frame_ops.opcodes [chain.index];
            switch (op.opcode)
            {
              case ir_opcode::end_chain:
              {
                chain.ended = true;
                const uint32_t ended_chains = frame_state.ended_chains.fetch_add(1, std::memory_order_release);
                if (ended_chains + 1 == frame_ops.chain_count)
                  should_reset_state = true;
                loop = false; // we are at the end of the chain, nothing to do
                break;
              }
              case ir_opcode::wait_task_group:
              {
                const group_t task_group_to_wait = op.arg;
                check::debug::n_assert(task_group_to_wait != k_non_transient_task_group,
                                       "Invalid frame operation: wait_task_group: cannot waitthe non-transient group");
                check::debug::n_assert(task_group_to_wait < frame_state.groups.size(),
                                       "Invalid frame operation: wait_task_group: group out of range");
                auto& group_info = frame_state.groups[task_group_to_wait];

                // task group has not started yet, nothing to do
                if (!group_info.is_started.load(std::memory_order_acquire))
                {
                  loop = false;
                  break;
                }

                if (group_info.is_completed.load(std::memory_order_acquire))
                {
                  // group is already completed, nothing to do
                  ++chain.index;
                  break;
                }

                if (group_info.remaining_tasks.load(std::memory_order_acquire) == 0)
                {
                  const bool has_run_cb = group_info.is_completed.exchange(true, std::memory_order_release);

                  // sanity check
                  // If it hit, please check that you only add tasks to either your own task group
                  // or to task groups that have dependency to your task group
                  //
                  // The start_group callback is considered part of the task-group
                  // whereas the end_group callback is not considered part of the task-group
                  check::debug::n_assert(group_info.remaining_tasks.load(std::memory_order_acquire) == 0,
                                         "Race condition detected while trying to complete a group: unexpected task has been added");

                  ++chain.index;

                  // extra complexity score as we have to lookup stuff
                  ++complexity;

                  if (!has_run_cb && group_info.end_group)
                  {
                    TRACY_SCOPED_ZONE;
                    complexity += k_function_complexity;

                    const group_t previous_gid = thread_state().current_gid;
                    thread_state().current_gid = k_invalid_task_group;

                    group_info.end_group();

                    thread_state().current_gid = previous_gid;
                  }

                  // we need to increment the state as some other chains might be waiting on us
                  // (so we must unlock advance)
                  frame_state.global_state_key.fetch_add(1, std::memory_order_relaxed);

                  start_again = true;

                  TRACY_FRAME_MARK_END(frame_ops.debug_names[task_group_to_wait].data());
                  break;
                }

                // there are remaining tasks
                loop = false;
                break;
              }
              case ir_opcode::execute_task_group:
              {
                const group_t task_group_to_execute = op.arg;

                TRACY_FRAME_MARK_START(frame_ops.debug_names[task_group_to_execute].data());

                check::debug::n_assert(task_group_to_execute != k_non_transient_task_group,
                                       "Invalid frame operation: execute_task_group: cannot execute the non-transient group");
                check::debug::n_assert(task_group_to_execute < frame_state.groups.size(),
                                       "Invalid frame operation: execute_task_group: group out of range");

                auto& group_info = frame_state.groups[task_group_to_execute];
                check::debug::n_assert(!group_info.is_completed.load(std::memory_order_relaxed),
                                       "Invalid frame operation: execute_task_group: trying to execute an already completed group");

                // set the will-start flag before calling the start call-back:
                group_info.will_start.store(true, std::memory_order_seq_cst);

                // start by executing the callback, so that any wait operation that
                // can be already present don't immediatly complete. This leave the chance of start_group to add tasks
                if (group_info.start_group)
                {
                  complexity += k_function_complexity;
                  const group_t previous_gid = thread_state().current_gid;
                  thread_state().current_gid = task_group_to_execute;

                  group_info.start_group();

                  thread_state().current_gid = previous_gid;
                }

                // we then wake all the threads
                uint32_t tasks_to_run = group_info.tasks_that_can_run.load(std::memory_order_seq_cst);
                while (!group_info.tasks_that_can_run.compare_exchange_strong(tasks_to_run, 0, std::memory_order_seq_cst));

                frame_state.threads[group_info.required_named_thread].tasks_that_can_run.fetch_add(tasks_to_run, std::memory_order_release);

                [[maybe_unused]] const bool was_started = group_info.is_started.exchange(true, std::memory_order_seq_cst);
                check::debug::n_assert(!was_started, "Invalid frame operation: execute_task_group: task group was already started");

                group_info.will_start.store(false, std::memory_order_seq_cst);

                ++chain.index;

                break;
              }
              default: check::debug::n_assert(false, "Invalid frame operation: unexpected opcode: {:X}", std::to_underlying(op.opcode));
                break;
            }
          }
        }
        if (start_again)
          chain_index = -1;
      }
    }

    if (should_reset_state)
    {
      reset_state();
      last_global_state_key = ~0u;
      return true;
    }

    // Return wether or not this should count as having executed a task:
    return complexity > k_complexity_threshold;
  }

  task* task_manager::get_task_to_run(named_thread_t thread, bool exclude_long_duration, task_selection_mode mode)
  {
    check::debug::n_assert(thread < frame_state.threads.size(), "get_task_to_run: invalid named thread: {}", (uint32_t)thread);

    const auto& conf = frame_state.threads[thread].configuration;
    const bool is_general_thread = (thread == k_no_named_thread);
    const bool can_run_general_tasks = !is_general_thread && (conf.can_run_general_tasks);

    task* ptr = get_task_to_run_internal(thread, exclude_long_duration);
    if (ptr)
      return ptr;
    if (((!can_run_general_tasks || mode == task_selection_mode::only_own_tasks) && mode != task_selection_mode::anything) || is_general_thread)
      return nullptr;
    return get_task_to_run_internal(k_no_named_thread, (!conf.can_run_general_long_duration_tasks && mode != task_selection_mode::anything) || exclude_long_duration);
  }

  task* task_manager::get_task_to_run_internal(named_thread_t thread, bool exclude_long_duration)
  {
    if (frame_state.groups.empty() || frame_state.threads[thread].tasks_that_can_run.load(std::memory_order_acquire) == 0)
      return nullptr;

    thread_local group_t start_group = 1;

    TRACY_SCOPED_ZONE;

    start_group += 7691;
    const group_t start_index = start_group;

    for (group_t i = 0; i < frame_state.groups.size(); ++i)
    {
      // weak shuffle, but hey, it works
      // + prioritize any other task group over the non-transient one
      // const group_t group_it = i == frame_state.groups.size() - 1
      //                          ? k_non_transient_task_group
      //                          : (1 + ((start_index + i) % (frame_state.groups.size() - 1)));
      const group_t group_it = (start_index + i) % frame_state.groups.size();

      group_info_t& group_info = frame_state.groups[group_it];

      if (exclude_long_duration && group_it == k_non_transient_task_group)
        continue;
      if (group_info.required_named_thread != thread && group_it != k_non_transient_task_group)
        continue;

      // skip groups that are completed / have not started
      if (group_it != k_non_transient_task_group)
      {
        if (group_info.is_completed.load(std::memory_order_acquire)
            || !group_info.is_started.load(std::memory_order_acquire))
          continue;
      }
      if (group_info.remaining_tasks.load(std::memory_order_acquire) == 0)
        continue;

      {
        task* ptr = nullptr;
        if (group_it == k_non_transient_task_group && thread != k_no_named_thread)
        {
          const bool has_task = frame_state.threads[thread].long_duration_tasks_to_run.try_pop_front(ptr);
          if (!has_task)
            continue;
        }
        else
        {
          const bool has_task = group_info.tasks_to_run.try_pop_front(ptr);
          if (!has_task)
            continue;
        }
        [[maybe_unused]] const uint32_t count = frame_state.threads[thread].tasks_that_can_run.fetch_sub(1, std::memory_order_release);
        // not a fatal error per say, but may lead to very incorect behavior
        check::debug::n_assert(count != 0, "Invalid state: tasks_that_can_run (named thread: {}, group: {}): underflow detected", thread, group_it);
        TRACY_PLOT_CSTR("task_manager::waiting_tasks", (int64_t)(count - 1));
        check::debug::n_assert(ptr != nullptr, "Corrupted task data");
        check::debug::n_assert(ptr->get_task_group() == group_it, "Task was not in the correct queue (expected {}, got {})", group_it, ptr->get_task_group());

        check::debug::n_assert(!ptr->is_completed(), "Invalid state: trying to execute a task that is already completed");
        check::debug::n_assert(ptr->is_waiting_to_run(), "Invalid state: trying to execute a task that is not expecting to run");

        if (group_it != k_non_transient_task_group)
        {
          check::debug::n_assert(ptr->get_frame_key() == frame_state.frame_key,
                                 "Trying to run a task that has outlived its lifespan (expected: {}, got: {}, task group: {})",
                                 frame_state.frame_key.load(), ptr->get_frame_key(), group_it);
        }


        return ptr;
      }
    }

    return nullptr;
  }

  void task_manager::do_run_task(task& task)
  {
    TRACY_SCOPED_ZONE_COLOR(0x00FF00);

    // Sanity checks:
    {
      const named_thread_t thread = get_current_thread();
      const group_t group = task.get_task_group();
      [[maybe_unused]] const auto& thread_conf = frame_state.threads[thread].configuration;
      [[maybe_unused]] group_info_t& group_info = frame_state.groups[group];

      if (group != k_non_transient_task_group)
      {
        check::debug::n_assert(group_info.required_named_thread == k_no_named_thread || group_info.required_named_thread == thread,
                              "Trying to run a task on the wrong thread (the current thread does not match the task requirements)");
        check::debug::n_assert(group_info.required_named_thread == thread || thread_conf.can_run_general_tasks,
                              "Trying to run a task on the wrong thread (the current task does not match the thread requirements)");
      }
    }


    const group_t previous_gid = thread_state().current_gid;
    frame_state.running_tasks.fetch_add(1, std::memory_order_release);
    thread_state().current_gid = task.get_task_group();
    if (thread_state().current_gid != k_non_transient_task_group)
      frame_state.running_transient_tasks.fetch_add(1, std::memory_order_release);

    task.run();
    // task has been destructed past this point

    if (thread_state().current_gid != k_non_transient_task_group)
      frame_state.running_transient_tasks.fetch_sub(1, std::memory_order_release);
    thread_state().current_gid = previous_gid;
    frame_state.running_tasks.fetch_sub(1, std::memory_order_release);
  }


  void task_manager::run_a_task(bool exclude_long_duration, task_selection_mode mode)
  {
    TRACY_SCOPED_ZONE_COLOR(0x444444);
    const named_thread_t thread = get_current_thread();

    // grab a random task and run it
    task* ptr = get_task_to_run(thread, exclude_long_duration, mode);
    if (ptr)
    {
      const group_t group = ptr->get_task_group();
      do_run_task(*ptr);

      if (group != k_non_transient_task_group)
      {
        if (frame_state.groups[group].remaining_tasks.load(std::memory_order_acquire) == 0)
          advance();
      }
    }
  }

  void task_manager::wait_for_a_task()
  {
    static std::atomic<uint64_t> waiting_threads_count { 0 };

    const uint8_t thread_index = thread_state().thread_index;
    const named_thread_t thread = get_current_thread();

    const auto& conf = frame_state.threads[thread].configuration;
    const bool can_run_general_tasks = thread != k_no_named_thread && (conf.can_run_general_tasks);
    cr::scoped_ordered_list waiting_threads { waiting_threads_count, can_run_general_tasks || thread == k_no_named_thread ? thread_index : (uint8_t)0xFF };

    const auto check_for_tasks = [=, this, &waiting_threads](std::memory_order mo)
    {
      const uint32_t place = waiting_threads.count_entries_before();
      if (frame_state.threads[thread].tasks_that_can_run.load(mo) > (thread == k_no_named_thread ? place : 0))
        return true;
      if (can_run_general_tasks && frame_state.threads[k_no_named_thread].tasks_that_can_run.load(mo) > place)
        return true;
      return false;
    };

    if (check_for_tasks(std::memory_order_acquire))
      return;

    TRACY_SCOPED_ZONE_COLOR(0xFF0000);

    constexpr uint32_t k_max_spin_count = 15000;
    constexpr uint32_t k_max_loop_count_before_sleep = 60; // 500us sleep
    constexpr uint32_t k_max_loop_count_before_long_sleep = 120; // 5ms sleep
    constexpr uint32_t k_short_sleep_us = 100;
    constexpr uint32_t k_long_sleep_us = 500;
    constexpr uint32_t k_max_long_sleep_us = 5000;
    uint32_t loop_count = 0;

    while (true)
    {
      // While the frame-lock is present, we sleep. (sleep for 5ms to minimize cpu overhead).
      // frame-lock is only ever used in those situations: startup, shutdown, non-interractive apps
      // so it should be okay to give-up 5ms, and it could be even more.
      // (the frame lock is effectively a freeze of the task-manager and using it means perfs are not critical)
      if (frame_state.frame_lock._relaxed_test())
      {
        TRACY_SCOPED_ZONE_COLOR(0x0F0000);
        do
        {
          if (frame_state.should_threads_leave)
            return;
          // we have long durtion tasks, we should run them (long duration tasks don't affect the task-graph / frames)
          if (!frame_state.threads[thread].long_duration_tasks_to_run.empty())
            return;
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        while (frame_state.frame_lock._relaxed_test());
      }

      if (loop_count < k_max_loop_count_before_sleep)
      {
        // only spin when we are in the "reactive" part of the wait. If we are in the sleep part, don't do much.
        uint32_t spin_count = 0;
        for (; !check_for_tasks(std::memory_order_relaxed) && spin_count < k_max_spin_count; ++spin_count);
      }
      if (check_for_tasks(std::memory_order_acquire))
        return;

      // avoid spining and consuming all the cpu:
      {
        if (loop_count > k_max_loop_count_before_long_sleep && thread == k_no_named_thread)
        {
          TRACY_SCOPED_ZONE_COLOR(0x3F0000);
          // every k_max_loop_count_before_long_sleep, add k_long_sleep_us to the sleep duration, up to k_max_long_sleep_us
          std::this_thread::sleep_for(std::chrono::microseconds(std::max(k_max_long_sleep_us, k_long_sleep_us * (loop_count / k_max_loop_count_before_long_sleep))));
        }
        else if (loop_count > k_max_loop_count_before_sleep)
        {
          TRACY_SCOPED_ZONE_COLOR(0x3F1F00);
          std::this_thread::sleep_for(std::chrono::microseconds(k_short_sleep_us));
        }
        else
        {
          std::this_thread::yield();
        }
      }
      ++loop_count;
    }
  }

  void task_manager::actively_wait_for(group_t group, task_completion_marker_ptr_t&& t)
  {
    if (t.is_completed())
      return;

    TRACY_SCOPED_ZONE_COLOR(0xFF0000);
    // This check prevent the most common deadlock pattern where the task calling
    // the function is the one preventing the task's group to be running.
    // Having this check here may trigger in some cases where it shouldn't,
    // but if the group of the task actively waiting and the task provided in param is the same,
    // the assert will never trigger.
    check::debug::n_assert(frame_state.groups[group].is_started.load(std::memory_order_acquire), "actively_wait_for must be called on a task whose group is already running");

    // we don't use the locked version here to avoid being locked-out during the task execution
    while (!t.is_completed())
      run_a_task(group != k_non_transient_task_group);
  }

  void task_manager::actively_wait_for(task_wrapper&& tw)
  {
    group_t group = tw->get_task_group();
    task_completion_marker_ptr_t t = tw.create_completion_marker();
    tw = {}; // allow the task to run
    return actively_wait_for(group, std::move(t));
  }

  std::chrono::microseconds task_manager::run_tasks(std::chrono::microseconds duration)
  {
    // number of times we can miss a task before returning
    constexpr uint32_t k_max_unlucky_strikes = 16;

    uint32_t unlucky_strikes = 0;
    uint32_t task_count = 0;

    const std::chrono::time_point start = std::chrono::high_resolution_clock::now();
    while (unlucky_strikes < k_max_unlucky_strikes)
    {
      // grab a random task and run it
      task* ptr = get_task_to_run(get_current_thread());
      if (ptr)
      {
        do_run_task(*ptr);
        ++task_count;
      }
      else if (!advance())
      {
        ++unlucky_strikes;
      }
      else
      {
        // treat advance as a task if it returns true
        ++task_count;
      }

      const std::chrono::time_point now = std::chrono::high_resolution_clock::now();
      const std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start);

      // we overshooted, return
      if (elapsed >= duration)
        return elapsed;

      // check if another task will make us overshoot:
      if (task_count > 0)
      {
        const std::chrono::microseconds projected_duration = (elapsed / task_count) * (task_count + 1);
        if (projected_duration >= duration)
          return elapsed;
      }
    }

    // we could not find a task, so we return early:
    const std::chrono::time_point now = std::chrono::high_resolution_clock::now();
    const std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
    return elapsed;
  }

  bool task_manager::has_pending_tasks() const
  {
    for (const auto& it : frame_state.threads)
    {
      if (it.tasks_that_can_run.load(std::memory_order_acquire) > 0)
        return true;
    }
    return false;
  }

  bool task_manager::has_running_tasks() const
  {
    if (frame_state.running_tasks.load(std::memory_order_acquire) > 0)
      return true;
    return false;
  }

  uint32_t task_manager::get_pending_tasks_count() const
  {
    uint32_t count = 0;
    for (const auto& it : frame_state.threads)
    {
      count += (it.tasks_that_can_run.load(std::memory_order_acquire));
    }
    return count;
  }

  void task_manager::poll_delayed_tasks(bool force_push)
  {
    TRACY_SCOPED_ZONE_COLOR(0xFF0000);

    std::lock_guard _lg(frame_state.delayed_tasks.lock);

    if (frame_state.delayed_tasks.delayed_tasks.empty())
      return;

    const auto now = std::chrono::high_resolution_clock::now();

    auto it = frame_state.delayed_tasks.delayed_tasks.begin();
    while (it != frame_state.delayed_tasks.delayed_tasks.end() && (force_push || it->ptr->execution_time_point <= now))
    {
      it->ptr->execution_time_point = {};
      add_task_to_run(*it->ptr);
      it = frame_state.delayed_tasks.delayed_tasks.erase(it);
    }

    TRACY_PLOT_CSTR("task_manager::delayed_tasks", (int64_t)frame_state.delayed_tasks.delayed_tasks.size());
    return;
  }

  void task_manager::reset_state()
  {
    TRACY_FRAME_MARK;
    TRACY_FRAME_MARK_START_CSTR("task_manager/reset_state");
    {
      TRACY_SCOPED_ZONE;

      bool is_stopped = false;

      // wait for no more running tasks:
      while (frame_state.running_transient_tasks.load(std::memory_order_acquire) != 0)
      {
      }

      // lock the frame lock if we have to stop (first so advance() can never happen)
      if (frame_state.should_stop)
      {
        is_stopped = true;
        frame_state.should_stop = false;
        frame_state.frame_lock.lock();
      }

      // lock the chains:
      for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
      {
        frame_state.chains[i].lock.lock();
      }

      // reset the chains:
      for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
      {
        frame_state.chains[i].index = frame_ops.opcodes[i].arg;
        frame_state.chains[i].ended = false;
      }

      transient_tasks.fast_clear();

      // reset groups:
      bool is_persistent_tasks = true;
      for (auto& it : frame_state.groups)
      {
        if (is_persistent_tasks)
        {
          is_persistent_tasks = false;
          continue;
        }
        check::debug::n_assert(it.remaining_tasks == 0, "Trying to reset state while a task group has still tasks running");
        check::debug::n_assert(it.is_completed, "Trying to reset state while a task group is not completed");

        it.remaining_tasks.store(0, std::memory_order_release);
        it.is_started.store(false, std::memory_order_release);
        it.is_completed.store(false, std::memory_order_release);
      }

      // set the value _before_ the unlock, to avoid threads having the chance to write to it
      frame_state.ended_chains.store(0, std::memory_order_release);

      // before any unlock, to avoid anyone creating a task with the wrong frame_key
      frame_state.frame_key.store((frame_state.frame_key.load(std::memory_order_relaxed) + 1) & 0xFFFFFF, std::memory_order_release);

      // unlock the chains:
      for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
      {
        frame_state.chains[i].lock.unlock();
      }

      if (is_stopped && frame_state.on_stopped)
      {
        frame_state.on_stopped();
        frame_state.on_stopped = {};
      }

      // set a new state key
      // Must be the last operation in order to unlock advance()
      frame_state.global_state_key.fetch_add(1, std::memory_order_release);


      advance();
    }
    TRACY_FRAME_MARK_END_CSTR("task_manager/reset_state");

    // done once per frame, after the reset ended
    poll_delayed_tasks();
  }
}

