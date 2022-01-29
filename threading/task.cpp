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
  void task::on_completed()
  {
    manager.destroy_task(*this);
  }

  void task::push_to_run(bool from_wrapper)
  {
    // Avoid super weird and hard to debug race conditions
    if (!from_wrapper && held_by_wrapper) return;

    check::debug::n_assert(from_wrapper == held_by_wrapper, "incoherent state");

    manager.add_task_to_run(*this);

    held_by_wrapper = false;
  }

  /// \brief Chain a task
  task& task::then(function_t&& fnc)
  {
    auto wr = manager.get_task(get_task_group(), std::move(fnc));
    wr->add_dependency_to(*this);
    return *wr;
  }
}


