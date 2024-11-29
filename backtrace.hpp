//
// file : backtrace.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/backtrace.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 13/02/2015 17:02:08
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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

namespace neam::cr
{
  /// \brief print the current callstack
  /// (and demangle functions name, and print a lovely addr2line command that you can use to get the line number)
  /// \param[in] backtrace_size the depth of the backtrace: the number of entries to print
  /// \param[in] skip the number of entries to skip from printing. (the N first entries to skip).
  /// \note currently only on LINUX
  void print_callstack(size_t backtrace_size = 25, size_t skip = 1, bool has_logger_lock = false);
} // namespace neam


// kate: indent-mode cstyle; indent-width 2; replace-tabs on;

