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

#include "task.hpp"
#include "task_manager.hpp"

namespace neam::threading
{
  void task::run()
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

  void task::on_completed()
  {
    manager.destroy_task(*this);
  }

  void task::notify_dependency_complete()
  {
    std::lock_guard<spinlock> _lg(lock);
    check::debug::n_assert(!unlock_is_completed(), "Trying to notify a task that is already completed");
    check::debug::n_assert(!unlock_is_waiting_to_run(), "Trying to notify a task that is already waiting to run");

    check::debug::n_assert(dependencies > 0, "Trying to notify a task that already has all its dependencies completed");

    --dependencies;
    if (dependencies == 0)
      push_to_run(false);
  }

  void task::notify_dependent_tasks()
  {
    // NOTE: Must be called with the lock held
    const uint32_t entry_count = std::min(k_max_dependencies, number_of_task_to_notify);
    for (uint32_t i = 0; i < entry_count; ++i)
    {
      tasks_to_notify[i]->notify_dependency_complete();
    }
    number_of_task_to_notify = 0;
    if (marker_to_signal != nullptr)
      *marker_to_signal = true;
  }

  void task::push_to_run(bool from_wrapper)
  {
    // Avoid super weird and hard to debug race conditions
    // (dependencies are setup then completed while the wrapper still hold the task,
    //  leading to a incoherent task state (not fully setup. sometimes))
    if (!from_wrapper && held_by_wrapper) return;

    check::debug::n_assert(from_wrapper == held_by_wrapper, "incoherent state");

    manager.add_task_to_run(*this);

    held_by_wrapper = false;
  }

  void task::add_dependency_to(task& other)
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
      check::debug::n_assert(other.number_of_task_to_notify < k_max_task_to_notify, "Max number of tasks to notify reached (use a new task-group, maybe?)");
      other.tasks_to_notify[other.number_of_task_to_notify] = this;
      other.number_of_task_to_notify += 1;
    }
  }

  void task::signal_marker(task_completion_marker_ptr_t& ptr)
  {
    std::lock_guard<spinlock> _lg(lock);
    check::debug::n_assert(marker_to_signal == nullptr, "A marker is already present for the task, this is invalid");
    check::debug::n_assert(!unlock_is_running(), "Cannot signal a marker when the task is already running");
    check::debug::n_assert(!unlock_is_completed(), "Cannot signal a marker when the task is already completed");
    check::debug::n_assert(!unlock_is_waiting_to_run(), "Cannot signal a marker on a task that is waiting to run");

    // ask to be notified of its completion
    check::debug::n_assert(ptr._is_valid(), "Cannot signal an invalid marker");
    check::debug::n_assert(!ptr.is_completed(), "Cannot reuse an already completed marker");

    {
      marker_to_signal = ptr._get_raw_pointer();
    }
  }

  /// \brief Chain a task
  task& task::then(function_t&& fnc)
  {
    auto wr = manager.get_task(get_task_group(), std::move(fnc));
    wr->add_dependency_to(*this);
    return *wr;
  }

  task_completion_marker_ptr_t task_wrapper::create_completion_marker()
  {
    check::debug::n_assert(t != nullptr, "create_completion_marker: cannot create a completion marker when the task is not in the wrapper");
    task_completion_marker_ptr_t ret = t->manager._allocate_completion_marker();
    t->signal_marker(ret);
    return ret;
  }
}


