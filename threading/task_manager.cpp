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

namespace neam::threading
{
  void task_manager::set_start_task_group_callback(group_t group, const function_t& fnc)
  {
    check::debug::n_assert(group < frame_state.groups.size(), "group {} does not exists", group);
    frame_state.groups[group].start_group = fnc;
  }

  void task_manager::set_end_task_group_callback(group_t group, const function_t& fnc)
  {
    check::debug::n_assert(group < frame_state.groups.size(), "group {} does not exists", group);
    frame_state.groups[group].end_group = fnc;
  }

  raii_task_wrapper task_manager::get_task(group_t task_group, function_t&& func)
  {
    check::debug::n_assert(task_group < frame_state.groups.size(), "Trying to create a task to a task group that does not exists. Did you forgot to create a task group?");

    frame_state.groups[task_group].remaining_tasks.fetch_add(1, std::memory_order_release);

    if (task_group == k_non_transient_task_group)
      return get_long_duration_task(std::move(func));

    task* ptr = (task*)transient_tasks.allocate(sizeof(task));
    new (ptr) task(*this, task_group, std::move(func));
    return *ptr;
  }

  raii_task_wrapper task_manager::get_long_duration_task(function_t&& func)
  {
    task* ptr;
    {
      std::lock_guard<spinlock> _lg(alloc_lock);
      ptr = non_transient_tasks.allocate();
    }
    check::debug::n_assert(ptr != nullptr, "Failed to allocate a task");

    new (ptr) task(*this, k_non_transient_task_group, std::move(func));
    return *ptr;
  }

  void task_manager::destroy_task(task& t)
  {
    t.~task();

    check::debug::n_assert(t.key < frame_state.groups.size(), "Trying to destroy a task from a task-group that does not exist");
    const uint32_t orig_count = frame_state.groups[t.key].remaining_tasks.fetch_sub(1, std::memory_order_consume);
    check::debug::n_assert(orig_count > 0, "Invalid task count: Trying to decrement the task count when it was 0");

    if (t.key == k_non_transient_task_group)
    {
      std::lock_guard<spinlock> _lg(alloc_lock);
      non_transient_tasks.deallocate(&t);
    }
  }

  void task_manager::add_task_to_run(task& t)
  {
    // skip tasks that cannot run, as they will be added automatically
    // this check is necessary as this function can be called by a user
    if (!t.can_run()) return;

    check::debug::n_assert(frame_state.groups[t.key].is_completed == false, "Trying to push a task to a completed group");
    const uint32_t index = frame_state.groups[t.key].insert_buffer_index.fetch_add(1, std::memory_order_relaxed);
    bool is_inserted = frame_state.groups[t.key].tasks_to_run[index % group_info_t::k_task_to_run_size].push_back(&t);
    check::debug::n_assert(is_inserted, "ring buffer overflow in the task_manager (group: {})", t.key);

    frame_state.tasks_that_can_run.fetch_add(1, std::memory_order_release);
    frame_state.tasks_that_can_run.notify_all();
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
  }

  bool task_manager::advance()
  {
    constexpr uint32_t k_function_complexity = 20;

    // past this threshold, run_a_task will return after the call to advance()
    // advance can have callbacks that are non-trivial and those can count as a task
    constexpr uint32_t k_complexity_threshold = 19;

    // arbitrary indicator of the work done here:
    uint32_t complexity = 0;
    uint32_t ended_chains = 0;

    {
      cr::scoped_counter _sc(frame_state.threads_in_frame_state);

      for (auto& chain : frame_state.chains)
      {
        if (frame_state.lock._get_state())
          break;


        // Multiple threads can be in this very function, so we simply allow a single thread on each chain
        // (chains are independent, so we can (oportunisticaly) parallelize their update)
        if (chain.wlock.try_lock())
        {
          std::lock_guard<spinlock> _lg(chain.wlock, std::adopt_lock);
        if (chain.ended)
        {
          ++ended_chains;
          continue;
        }

          bool loop = true;
          while (loop)
          {
            ++complexity;

            const ir_opcode& op = frame_ops.opcodes [chain.index];
            switch (op.opcode)
            {
              case ir_opcode::end_chain:
              {
                ++ended_chains;
                chain.ended = true;
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
                    complexity += k_function_complexity;
                    group_info.end_group();
                  }
                  break;
                }

                // there are remaining tasks
                loop = false;
                break;
              }
              case ir_opcode::execute_task_group:
              {
                const group_t task_group_to_execute = op.arg;
                check::debug::n_assert(task_group_to_execute != k_non_transient_task_group,
                                       "Invalid frame operation: execute_task_group: cannot execute the non-transient group");
                check::debug::n_assert(task_group_to_execute < frame_state.groups.size(),
                                       "Invalid frame operation: execute_task_group: group out of range");

                auto& group_info = frame_state.groups[task_group_to_execute];
                check::debug::n_assert(!group_info.is_completed.load(std::memory_order_relaxed),
                                       "Invalid frame operation: execute_task_group: trying to execute an already completed group");

                // start by executing the callback, so that any wait operation that
                // can be already present don't immediatly complete. This leave the chance of start_group to add tasks
                if (group_info.start_group)
                {
                  complexity += k_function_complexity;
                  group_info.start_group();
                }
                const bool was_started = group_info.is_started.exchange(true, std::memory_order_release);
                check::debug::n_assert(!was_started, "Invalid frame operation: execute_task_group: task group was already started");

                ++chain.index;

                break;
              }
              default: check::debug::n_assert(false, "Invalid frame operation: unexpected opcode: {}", op.opcode);
                break;
            }
          }
        }
      }
    } // scoped counter

    if (ended_chains == frame_ops.chain_count)
    {
      if (frame_state.lock.try_lock())
      {
        const std::lock_guard<spinlock> _lg(frame_state.lock, std::adopt_lock);

        // wait for threads in the state to do their stuff:
        while (frame_state.threads_in_frame_state.load(std::memory_order_acquire) != 0);

        reset_state();
      }
    }
    frame_state.lock._wait_for_lock();

    // Return wether or not this should count as having executed a task:
    return complexity > k_complexity_threshold;
  }

  task* task_manager::get_task_to_run()
  {
    thread_local group_t start_group = 1;
    if (frame_state.groups.empty() || frame_state.lock._get_state())
      return nullptr;

    start_group += 1;

    for (group_t i = 0; i < frame_state.groups.size(); ++i)
    {
      // weak shuffle, but hey, it works
      const group_t group_it = (start_group + i) % frame_state.groups.size();
      group_info_t& group_info = frame_state.groups[group_it];

      // skip groups that are completed / have not started
      if (group_it != k_non_transient_task_group)
      {
        if (group_info.is_completed.load(std::memory_order_relaxed)
            || !group_info.is_started.load(std::memory_order_relaxed))
          continue;
      }

      bool has_task = false;
      uint32_t base_index = group_info.insert_buffer_index.load(std::memory_order_relaxed) + 2;
      for (uint32_t j = 0; j < group_info_t::k_task_to_run_size; ++j)
      {
        task* ptr = group_info.tasks_to_run[(j + base_index) % group_info_t::k_task_to_run_size].quick_pop_front(has_task);
        if (!has_task)
          continue;

        frame_state.tasks_that_can_run.fetch_sub(1, std::memory_order_release);
        return ptr;
      }
    }

    return nullptr;
  }

  void task_manager::run_a_task()
  {
    // try to advance the state
    if (advance())
      return; // advance() executed a transition task, so we did run _a_ task

    // grab a random task and run it
    task* ptr = get_task_to_run();
    if (ptr)
    {
      ptr->run();
      return;
    }
  }

  void task_manager::wait_for_a_task() const
  {
    frame_state.tasks_that_can_run.wait(0, std::memory_order_acquire);
  }

  void task_manager::reset_state()
  {
    // reset groups:
    for (auto& it : frame_state.groups)
    {
//       it.tasks_to_run.clear();
      it.remaining_tasks.store(0, std::memory_order_relaxed);
      it.is_started.store(false, std::memory_order_relaxed);
      it.is_completed.store(false, std::memory_order_relaxed);
    }

    // reset the chains:
    for (uint16_t i = 0; i < frame_ops.chain_count; ++i)
    {
      frame_state.chains[i].index = frame_ops.opcodes[i].arg;
      frame_state.chains[i].ended = false;
    }
  }
}

