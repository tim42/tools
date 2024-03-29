//
// created by : Timothée Feuillet
// date: 2022-3-18
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

#include <cstdint>

/// \brief Utility for handling raw memory/pages. Mostly there to abstract OS shenanigans.
namespace neam::memory
{
  /// \brief Return the page size the os supports
  uint64_t get_page_size();

  /// \brief Allocates a single page (directly from the os)
  /// \return nullptr if the allocation failed
  ///
  /// \note pages allocated this way cannot hold executable code
  void* allocate_page(uint32_t page_count = 1, bool use_pool = true);

  /// \brief Returns a page to the OS.
  /// \note pointer must be page aligned.
  void free_page(void* page_ptr, uint32_t page_count, bool use_pool = true);


  namespace statistics
  {
    uint32_t get_current_allocated_page_count();
    uint32_t get_total_allocated_page_count();
  }
}
