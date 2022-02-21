//
// created by : Timothée Feuillet
// date: 2022-2-19
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

namespace neam::threading
{
  /// \brief Dispatch tasks that call func for each element of the container (container must be indexable via operator [] and have .size())
  ///
  /// \param func Callable that must match void ((const) Element& entry, size_t index)
  /// \param entry_per_task is the number of entries each task will operate on. Th default is a nice value for a ~cheap for-each
  /// \param max_task_to_dispatch is max number of task that should be waiting to be executed. If == 1, is effectively single threaded.
  ///        the default works nicely and avoid starving cores while avoid dispatching too many tasks.
  ///        If smaller than the number of threads, it will run on at most the specified number of thread.
  ///
  /// \note The function participates to the for-each operation and returns when everything is completed
  /// \note Tasks are dispatched asynchronously to avoid spending too much time creating tasks for large collections
  template<typename IndexableContainer, typename Func>
  void for_each(neam::threading::task_manager& tm, neam::threading::group_t group, IndexableContainer&& array, Func&& func,
                size_t entry_per_task = 1024, size_t max_task_to_dispatch = (std::thread::hardware_concurrency() + 2) * 2)
  {
    const size_t size = array.size();

    // Used to synchronise all the tasks to a single point:
    neam::threading::task_wrapper final_task = tm.get_task(group, []() {});

    std::atomic<size_t> index = 0;

    // inner for-each function
    std::function<void()> inner_for_each;
    inner_for_each = [&tm, &array, &index, &func, &inner_for_each, &final_task = *final_task, size, entry_per_task, group]()
    {
      const size_t base_index = index.fetch_add(entry_per_task, std::memory_order_acq_rel);
      for (size_t i = 0; i < entry_per_task && base_index + i < size; ++i)
      {
        func(array[base_index + i], base_index + i);
      }

      // Add a new task if necessary:
      if (index.load(std::memory_order_acquire) < size)
      {
        auto task = tm.get_task(group, std::function<void()>(inner_for_each));
        final_task.add_dependency_to(task);
      }
    };

    // avoid spending too much time by creating too many tasks
    // the additional tasks will be created on a case-by-case basis on different threads
    size_t dispatch_count = size / entry_per_task;
    if (dispatch_count > max_task_to_dispatch)
      dispatch_count = max_task_to_dispatch;

    // dispatch the initial tasks:
    for (size_t i = 0; i < dispatch_count; ++i)
    {
      auto task = tm.get_task(group, std::function<void()>(inner_for_each));
      final_task->add_dependency_to(task);
    }

    // make the wrapper release the ref so it can run:
    threading::task& final_task_ref = *final_task;
    final_task = {};

    // Prevent leaving the function before final_task is done,
    // thus keeping alive all the variables that are in the lambda capture
    tm.actively_wait_for(final_task_ref);
  }
}

