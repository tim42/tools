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

    if (task_group == k_non_transient_task_group)
      return get_long_duration_task(std::move(func));

    frame_state.groups[task_group].remaining_tasks.fetch_add(1, std::memory_order_release);

    check::debug::n_assert(frame_state.groups[task_group].is_completed == false, "Trying to create a task from a completed group");

    task* ptr = (task*)transient_tasks.allocate(sizeof(task));
    new (ptr) task(*this, task_group, frame_state.frame_key, std::move(func));
    return *ptr;
  }

  task_wrapper task_manager::get_long_duration_task(function_t&& func)
  {
    frame_state.groups[k_non_transient_task_group].remaining_tasks.fetch_add(1, std::memory_order_release);

    task* ptr = non_transient_tasks.allocate();
    check::debug::n_assert(ptr != nullptr, "Failed to allocate a task");

    new (ptr) task(*this, k_non_transient_task_group, frame_state.frame_key, std::move(func));
    return *ptr;
  }

  void task_manager::destroy_task(task& t)
  {
    const group_t group = t.key;
    check::debug::n_assert(group < frame_state.groups.size(), "Trying to destroy a task from a task-group that does not exist");
    check::debug::n_assert(t.is_completed(), "Trying to destroy a task that wasn't completed");

    if (group != k_non_transient_task_group)
    {
      check::debug::n_assert(t.get_frame_key() == frame_state.frame_key, "Trying to destroy a task that outlived its lifespan");
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

  void task_manager::add_task_to_run(task& t)
  {
    check::debug::n_assert(t.unlock_is_completed() == false, "Trying to push an already completed task");
    check::debug::n_assert(t.unlock_is_waiting_to_run() == false, "Trying to push an already waiting task");
    if (t.key != k_non_transient_task_group)
    {
      check::debug::n_assert(t.get_frame_key() == frame_state.frame_key, "Trying to push a task to run when that task has outlived its lifespan");
      check::debug::n_assert(frame_state.groups[t.key].is_completed == false, "Trying to push a task to a completed group");
    }
    // skip tasks that cannot run, as they will be added automatically
    // this check is necessary as this function can be called by a user
    if (!t.unlock_can_run()) return;

    t.set_task_as_waiting_to_run();

    const uint32_t index = frame_state.groups[t.key].insert_buffer_index.fetch_add(1, std::memory_order_acquire);

    // don't wake waiting thread when the task is not really usable (group hasn't started yet)
    if (t.key == k_non_transient_task_group
        || frame_state.groups[t.key].will_start.load(std::memory_order_seq_cst)
        || frame_state.groups[t.key].is_started.load(std::memory_order_seq_cst))
    {
      [[maybe_unused]] const uint32_t count = frame_state.tasks_that_can_run.fetch_add(1, std::memory_order_release);
      //frame_state.tasks_that_can_run.notify_one();
      TRACY_PLOT_CSTR("task_manager::waiting_tasks", (int64_t)count);
    }
    else
    {
      // The lack of atomicity here is handled in the execute group (in advance())
      frame_state.groups[t.key].tasks_that_can_run.fetch_add(1, std::memory_order_seq_cst);
    }

    bool is_inserted = frame_state.groups[t.key].tasks_to_run[index % group_info_t::k_task_to_run_size].push_back(&t);
    check::debug::n_assert(is_inserted, "ring buffer overflow in the task_manager (group: {})", t.key);
  }

  void task_manager::add_compiled_frame_operations(resolved_graph&& _frame_ops)
  {
    frame_ops = std::move(_frame_ops);

    // chains:
    frame_state.chains.resize(frame_ops.chain_count);
    for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
    {
      check::debug::n_assert(frame_ops.opcodes[i].opcode == ir_opcode::declare_chain_index, "Invalid frame operation: expected declare_chain_index opcode, got {}", frame_ops.opcodes[i].opcode);
      frame_state.chains[i].index = frame_ops.opcodes[i].arg;
    }

    // groups: (TODO: add a group count)
    uint32_t max_group = 1;
    for (const auto& it : frame_ops.groups)
    {
      max_group = std::max<uint32_t>(max_group, it.second);
    }
    frame_state.groups.resize(max_group + 1);

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
      for (auto& chain : frame_state.chains)
      {
        {
          if (!chain.lock.try_lock())
            continue;
          std::lock_guard<spinlock> _lg(chain.lock, std::adopt_lock);
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

                frame_state.tasks_that_can_run.fetch_add(tasks_to_run, std::memory_order_release);

                const bool was_started = group_info.is_started.exchange(true, std::memory_order_seq_cst);
                check::debug::n_assert(!was_started, "Invalid frame operation: execute_task_group: task group was already started");

                //frame_state.tasks_that_can_run.notify_all();
                group_info.will_start.store(false, std::memory_order_seq_cst);

                ++chain.index;

                break;
              }
              default: check::debug::n_assert(false, "Invalid frame operation: unexpected opcode: {}", op.opcode);
                break;
            }
          }
        }
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

  task* task_manager::get_task_to_run()
  {

    thread_local group_t start_group = 1;
    if (frame_state.groups.empty() || frame_state.tasks_that_can_run.load(std::memory_order_acquire) == 0)
      return nullptr;

    TRACY_SCOPED_ZONE;

    start_group += 1;

    for (group_t i = 0; i < frame_state.groups.size(); ++i)
    {

      // weak shuffle, but hey, it works
      const group_t group_it = (start_group + i) % frame_state.groups.size();
      group_info_t& group_info = frame_state.groups[group_it];

      // skip groups that are completed / have not started
      if (group_it != k_non_transient_task_group)
      {
        if (group_info.is_completed.load(std::memory_order_acquire)
            || !group_info.is_started.load(std::memory_order_acquire))
          continue;
      }
      if (group_info.remaining_tasks.load(std::memory_order_acquire) == 0)
        continue;

      const uint32_t base_index = group_info.insert_buffer_index.load(std::memory_order_relaxed) + 2;
      for (uint32_t j = 0; j < group_info_t::k_task_to_run_size; ++j)
      {
        bool has_task = false;
        task* ptr = group_info.tasks_to_run[(j + base_index) % group_info_t::k_task_to_run_size].pop_front(has_task);
//         task* ptr = group_info.tasks_to_run[(j + base_index) % group_info_t::k_task_to_run_size].quick_pop_front(has_task);
        if (!has_task)
          continue;
        check::debug::n_assert(ptr != nullptr, "Corrupted task data");

        if (group_it != k_non_transient_task_group)
        {
          check::debug::n_assert(ptr->get_frame_key() == frame_state.frame_key, "Trying to run a task that has outlived its lifespan");
        }

        check::debug::n_assert(!ptr->is_completed(), "Invalid state: trying to execute a task that is already completed");
        check::debug::n_assert(ptr->is_waiting_to_run(), "Invalid state: trying to execute a task that is not expecting to run");

        const uint32_t count = frame_state.tasks_that_can_run.fetch_sub(1, std::memory_order_release);
        // not a fatal error per say, but may lead to very incorect behavior
        // also, when that test is not present, there's a huge perf penalty
        check::debug::n_assert(count != 0, "Invalid state: tasks_that_can_run: underflow detected");
        TRACY_PLOT_CSTR("task_manager::waiting_tasks", (int64_t)count);
        return ptr;
      }
    }

    return nullptr;
  }

  void task_manager::do_run_task(task& task)
  {
    TRACY_SCOPED_ZONE_COLOR(0x00FF00);
    const group_t previous_gid = thread_state().current_gid;
    thread_state().current_gid = task.get_task_group();
    task.run();
    thread_state().current_gid = previous_gid;
  }


  void task_manager::run_a_task()
  {
    // try to advance the state
    if (advance())
      return;

    // grab a random task and run it
    task* ptr = get_task_to_run();
    if (ptr)
    {
      return do_run_task(*ptr);
    }
  }

  void task_manager::wait_for_a_task()
  {
    TRACY_SCOPED_ZONE_COLOR(0xFF0000);
    if (frame_state.tasks_that_can_run.load(std::memory_order_acquire) > 0)
      return;

    const uint32_t original_frame_key = frame_state.frame_key;

    advance();

    constexpr uint32_t k_max_spin_count = 1000;
    constexpr uint32_t k_max_loop_count_before_sleep = 100;
    uint32_t loop_count = 0;

    // avoid looping forever, force to exit the function on frame-end
    while (original_frame_key == frame_state.frame_key)
    {
      uint32_t spin_count = 0;
      for (; frame_state.tasks_that_can_run.load(std::memory_order_relaxed) == 0 && spin_count < k_max_spin_count; ++spin_count);
      if (frame_state.tasks_that_can_run.load(std::memory_order_acquire) > 0)
        return;

      if (loop_count >= k_max_loop_count_before_sleep)
      {
        TRACY_SCOPED_ZONE;
        // avoid spining and consuming all the cpu:
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      else
      {
        std::this_thread::yield();
        ++loop_count;
      }

      // try to advance the state to avoid being locked in waiting:
      advance();
    }
  }

  void task_manager::actively_wait_for(task& t)
  {
    const group_t group = t.get_task_group();

    // This check prevent the most common deadlock pattern where the task calling
    // the function is the one preventing the task's group to be running.
    // Having this check here may trigger in some cases where it shouldn't,
    // but if the group of the task actively waiting and the task provided in param is the same,
    // the assert will never trigger.
    check::debug::n_assert(frame_state.groups[group].is_started.load(std::memory_order_acquire), "actively_wait_for must be called on a task whose group is already running");

    // we don't use the locked version here to avoid being locked-out during the task execution
    while (!t.unlock_is_completed())
      run_a_task();
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
      task* ptr = get_task_to_run();
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
    if (frame_state.tasks_that_can_run.load(std::memory_order_acquire) > 0)
      return true;
    return false;
  }


  void task_manager::reset_state()
  {
    TRACY_FRAME_MARK;
    TRACY_FRAME_MARK_START_CSTR("task_manager/reset_state");
    {
      TRACY_SCOPED_ZONE;

      // lock the chains:
      for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
      {
        frame_state.chains[i].lock.lock();
      }

      frame_state.frame_key = (frame_state.frame_key + 1) & 0xFFFFFF;

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

      // unlock the chains:
      for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
      {
        frame_state.chains[i].lock.unlock();
      }

      // set a new state key
      // Must be the last operation in order to unlock advance()
      frame_state.global_state_key.fetch_add(1, std::memory_order_release);
    }
    TRACY_FRAME_MARK_END_CSTR("task_manager/reset_state");
  }
}

