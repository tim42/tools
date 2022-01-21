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

#include <vector>

#include "types.hpp"
#include "../debug/assert.hpp"

namespace neam::threading
{
  class task_manager;

  /// \brief Wrapper around a function to run and its reverse-dependencies
  class task
  {
    public:
      ~task() { check::debug::n_assert(is_completed(), "Task is being destroyed without being completed"); }

      // cannot move the task as we:
      //  1: allocate it in a pool
      //  2: rely on pointers
      task(task&& o) = delete;
      task& operator = (task&& o) = delete;

      /// \brief Return if the task can run now (all the tasks it depends on are done)
      bool is_completed() const { return dependencies == k_completed_marker; }
      bool can_run() const { return !is_completed() && dependencies == 0; }

      void add_dependency_to(task& other)
      {
        // If you need to do that, add a task group dependency.
        // Task group level stuff needs to be done at the task group level.
        check::debug::n_assert(other.key == key, "Cannot depend on a task not in a different task group");

        // the task is completed, nothing to be done
        if (other.is_completed())
          return;

        // ask to be notified of its completion
        ++dependencies;
        other.tasks_to_notify.push_back(this);
      }

  private:
      task(task_manager& _manager, group_t _key, function_t _function) : manager(_manager), function(std::move(_function)), key(_key) {}

      /// \brief Run the task
      void run()
      {
        check::debug::n_assert(!is_completed(), "task::run called when not task has already been completed");
        check::debug::n_assert(can_run(), "task::run called when not all dependencies have been satisfied");

        // run:
        function();

        // complete the task:
        dependencies = k_completed_marker;

        // notify dependent tasks:
        notify_dependent_tasks();

        // may destroy the task
        on_completed();
      }

      // implemented in task_manager.hpp, so it can be inlined
      void push_to_run();

      void on_completed();

    private:
      void notify_dependency_complete()
      {
        check::debug::n_assert(!is_completed(), "Trying to notify a task that is already completed");
        check::debug::n_assert(dependencies > 0, "Trying to notify a task that already has all its dependencies completed");
        --dependencies;

        push_to_run();
      }

      void notify_dependent_tasks()
      {
        for (task* it : tasks_to_notify)
          it->notify_dependency_complete();
        tasks_to_notify.clear();
      }

    private:
      task_manager& manager;
      function_t function;
      group_t key;
      static constexpr unsigned k_completed_marker = ~0u;
      unsigned dependencies = 0;
      std::vector<task*> tasks_to_notify;

      friend class task_manager;
      friend class raii_task_wrapper;
  };

  /// \brief auto-register a task at the end of its scope
  class raii_task_wrapper
  {
    public:
      raii_task_wrapper(task& _t) : t(_t) {}
      ~raii_task_wrapper() { t.push_to_run(); }

      operator task&() { return t; }
      operator const task&() const { return t; }

      task& get_task() { return t; }
      const task& get_task() const { return t; }

      task* operator -> () { return &t; }
      const task* operator -> () const { return &t; }

    private:
      task& t;
  };

}

