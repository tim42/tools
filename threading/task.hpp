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

#include <atomic>
#include <vector>


#include "types.hpp"
#include "../debug/assert.hpp"
#include "../memory_pool.hpp" // for friendship

namespace neam::threading
{
  class task_manager;

  /// \brief Wrapper around a function to run and its reverse-dependencies
  class task
  {
    public:
      // cannot move the task as we:
      //  1: allocate it in a pool
      //  2: rely on pointers
      task(task&& o) = delete;
      task(const task& o) = delete;
      task& operator = (task&& o) = delete;
      task& operator = (const task& o) = delete;

      /// \brief Return if the task can run now (all the tasks it depends on are done)
      bool is_completed() const { std::lock_guard<spinlock> _lg(lock); return unlock_is_completed(); }
      bool is_running() const { std::lock_guard<spinlock> _lg(lock); return unlock_is_running(); }
      bool is_waiting_to_run() const { std::lock_guard<spinlock> _lg(lock); return unlock_is_waiting_to_run(); }
      bool can_run() const { std::lock_guard<spinlock> _lg(lock); return unlock_can_run(); }

      void add_dependency_to(task& other)
      {
        check::debug::n_assert(&other != this, "Trying to create a circular dependency");
        // If you need to do that, add a task group dependency.
        // Task group level stuff needs to be done at the task group level.
        check::debug::n_assert(key == other.key, "Cannot depend on a task not in a different task group");

        if (key != k_non_transient_task_group)
        {
          check::debug::n_assert(frame_key == other.frame_key, "Using a transient task outside its intended life span");
        }
        check::debug::n_assert(frame_key != ~0u, "Using a transient task outside its intended life span");
        check::debug::n_assert(other.frame_key != ~0u, "Using a transient task outside its intended life span");

        std::lock_guard<spinlock> _lg(lock);
        std::lock_guard<spinlock> _olg(other.lock);

        // the task is completed, nothing to be done
        if (other.unlock_is_completed())
        {
          cr::out().warn("trying to make a task depend on an already completed task");
          return;
        }

        check::debug::n_assert(!unlock_is_running(), "Cannot add dependency when the task is already running");
        check::debug::n_assert(!unlock_is_completed(), "Cannot add dependency when the task is already completed");
        check::debug::n_assert(!unlock_is_waiting_to_run(), "Cannot add dependency on a task that is waiting to run");

        // ask to be notified of its completion
        check::debug::n_assert(dependencies < k_max_dependencies, "Max number of task to wait for reached (you have more than 4 billion tasks waiting to be launched ?!)");

        dependencies += 1;

        {
          check::debug::n_assert(other.number_of_task_to_notify < k_max_task_to_notify, "Max number of tasks to notify reached (use a new task-group)");
          other.tasks_to_notify[other.number_of_task_to_notify] = this;
          other.number_of_task_to_notify += 1;
        }
      }

      group_t get_task_group() const { return key; }
      uint32_t get_frame_key() const { return frame_key; }

      /// \brief Chain a task, automatically adding the dependency and task-group
      task& then(function_t&& fnc);

  private:
      task(task_manager& _manager, group_t _key, uint32_t _frame_key, function_t _function)
      : manager(_manager), function(std::move(_function)), key(_key), frame_key(_frame_key)
      {
        memset(tasks_to_notify, 0, sizeof(tasks_to_notify));
      }
      ~task()
      {
        frame_key = ~0u;
        check::debug::n_assert(is_completed(), "Task is being destroyed without being completed");
      }

      // unlocked version of the getters
      bool unlock_is_running() const { return dependencies == k_running_marker; }
      bool unlock_is_completed() const { return dependencies == k_completed_marker; }
      bool unlock_is_waiting_to_run() const { return dependencies == k_is_slated_to_run_marker; }
      bool unlock_can_run() const { return dependencies == 0; }


      /// \brief Run the task
      void run()
      {
        {
          std::lock_guard<spinlock> _lg(lock);
          check::debug::n_assert(!unlock_is_completed(), "task::run called when the task has already been completed");
          check::debug::n_assert(unlock_is_waiting_to_run(), "task::run called on a task that isn't waiting to run (corruption ?)");

          // mark the task as completed:
          dependencies = k_running_marker;

          // run:
          function();

          dependencies = k_completed_marker;

          // notify dependent tasks:
          notify_dependent_tasks();
        }

        // may destroy the task
        on_completed();
      }

      // implemented in task_manager.hpp, so it can be inlined
      void push_to_run(bool from_wrapper);

      void on_completed();

      void set_task_as_waiting_to_run()
      {
        dependencies = k_is_slated_to_run_marker;
      }

      void notify_dependency_complete()
      {
        std::lock_guard<spinlock> _lg(lock);
        check::debug::n_assert(!unlock_is_completed(), "Trying to notify a task that is already completed");
        check::debug::n_assert(!unlock_is_waiting_to_run(), "Trying to notify a task that is already waiting to run");

        check::debug::n_assert(dependencies > 0, "Trying to notify a task that already has all its dependencies completed");

        --dependencies;
        if (dependencies == 0)
          push_to_run(false);
      }

      void notify_dependent_tasks()
      {
        // NOTE: Must be called with the lock held
        const uint32_t entry_count = number_of_task_to_notify;
        for (uint32_t i = 0; i < entry_count; ++i)
        {
          check::debug::n_assert(tasks_to_notify[i] != nullptr, "Memory corruption ?");
          tasks_to_notify[i]->notify_dependency_complete();
        }
        number_of_task_to_notify = 0;
      }

    private:
      static constexpr uint32_t k_completed_marker = ~0u;
      static constexpr uint32_t k_running_marker = k_completed_marker - 1;
      static constexpr uint32_t k_is_slated_to_run_marker = k_running_marker - 1;

      // More than 4 billion queued tasks. (It's not the total dependency count, it's the still un-executed tasks)
      // If that limit is hit something is going very very wrong.
      static constexpr uint32_t k_max_dependencies = k_is_slated_to_run_marker - 2;

      static constexpr uint32_t k_max_task_to_notify = 16;

      mutable spinlock lock;

      task_manager& manager;
      function_t function;
      group_t key;

      // Avoids being pushed-to-run when the task is still held by the wrapper.
      // This may happens when creating and dispatching dependencies of a task and contention haappens
      bool held_by_wrapper = true;

      uint32_t number_of_task_to_notify = 0;
      uint32_t dependencies = 0;

      // Debug purpose only. Can catch some weird dangling references.
      uint32_t frame_key = 0;

      task* tasks_to_notify[k_max_task_to_notify];

      friend class task_manager;
      friend class task_wrapper;
      friend class cr::memory_pool<task>;
  };

  /// \brief auto-register a task at the end of its scope
  /// \note Avoid registering the task too soon / prevent push_to_run race conditions
  class task_wrapper
  {
    public:
      task_wrapper() = default;
      task_wrapper(task& _t) : t(&_t) { t->held_by_wrapper = true; }
      task_wrapper(task_wrapper&& o) : t(o.t) {o.t = nullptr;}
      task_wrapper& operator = (task_wrapper&& o)
      {
        if (&o == this) return *this;
        push_to_run();
        t = o.t;
        o.t = nullptr;
        return *this;
      }
      ~task_wrapper() { push_to_run(); }

      operator task&() { return *t; }
      operator const task&() const { return *t; }

      task& get_task() { return *t; }
      const task& get_task() const { return *t; }
      task& operator*() { return *t; }
      const task& operator*() const { return *t; }

      task* operator -> () { return t; }
      const task* operator -> () const { return t; }

    private:
      void push_to_run()
      {
        if (t)
        {
          std::lock_guard<spinlock> _lg(t->lock);
          check::debug::n_assert(t->held_by_wrapper, "incoherent state");
          t->push_to_run(true);
          t = nullptr;
        }
      }

    private:
      task* t = nullptr;
  };
}

