//
// created by : Timothée Feuillet
// date: 2022-1-21
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

#include <functional>
#include <cstdint>

// waiting for std::move_only_function to be availlable, here is a ~drop-in replacement
#include "../async/internal_do_not_use_cpp23_replacement/function2.hpp"
#include "../raw_ptr.hpp"


namespace neam::threading
{
  class task_manager;
  class task;

//           std::move_only_function
  using function_t = fu2::unique_function<void()>;

  using group_t = uint8_t;
  static constexpr group_t k_non_transient_task_group = 0;
  static constexpr group_t k_invalid_task_group = ~group_t(0);

  using named_thread_t = uint8_t;
  static constexpr named_thread_t k_no_named_thread = 0;
  static constexpr named_thread_t k_invalid_named_thread = ~named_thread_t(0);

  using task_completion_marker_t = bool;

  class task_completion_marker_ptr_t
  {
    public:
      task_completion_marker_ptr_t() = delete;
      task_completion_marker_ptr_t(task_completion_marker_ptr_t&&) = default;
      task_completion_marker_ptr_t& operator = (task_completion_marker_ptr_t&&) = default;
      ~task_completion_marker_ptr_t();


      bool is_completed() const
      {
        check::debug::n_check(_is_valid(), "task_completion_marker_ptr_t: trying to get task completion using an invalid marker.");
        return *ptr;
      }
      operator bool () const { return is_completed(); }

      bool _is_valid() const { return ptr != nullptr; }
      task_completion_marker_t* _get_raw_pointer() { return ptr; }

      group_t get_task_group() const { return task_group; }

    private:
      task_completion_marker_ptr_t(task_completion_marker_t* _ptr, task_manager* _tm) : ptr(_ptr), tm(_tm) {}

    private:
      cr::raw_ptr<task_completion_marker_t> ptr;
      task_manager* tm;
      group_t task_group = k_invalid_task_group;

      friend task_manager;
      friend task;
  };
}

