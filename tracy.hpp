//
// created by : Timothée Feuillet
// date: 2022-2-3
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

/// \brief Conditional support for tracy

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>
#include <tracy/common/TracySystem.hpp>

#include "ct_string.hpp"

namespace neam::ct::internal
{
  template<neam::ct::string_holder Str>
  struct unique_str_t
  {
    static constexpr neam::ct::string_holder string = Str;
  };

  template<neam::ct::string_holder Str>
  static constexpr auto& unique_str = unique_str_t<Str>::string.string;

}


  #define TRACY_FRAME_MARK FrameMark
  #define TRACY_SCOPED_ZONE ZoneScoped
  #define TRACY_SCOPED_ZONE_COLOR(c) ZoneScopedC(c)
  #define TRACY_NAME_THREAD(name) tracy::SetThreadName(name)

  #define TRACY_FRAME_MARK_START_CSTR(x) FrameMarkStart(neam::ct::internal::unique_str<x>)
  #define TRACY_FRAME_MARK_END_CSTR(x) FrameMarkEnd(neam::ct::internal::unique_str<x>)

  #define TRACY_FRAME_MARK_START(x) FrameMarkStart(x)
  #define TRACY_FRAME_MARK_END(x) FrameMarkEnd(x)

  #define TRACY_LOCKABLE(type, name) TracyLockable(type, name)
  #define TRACY_LOCKABLE_BASE(type) LockableBase(type)

  #define TRACY_ALLOC_CSTR(ptr, size, name) TracyAllocN(ptr, size, neam::ct::internal::unique_str<name>)
  #define TRACY_FREE_CSTR(ptr, size, name) TracyFreeN(ptr, size, neam::ct::internal::unique_str<name>)

  #define TRACY_ALLOC(ptr, size, name) TracyAllocN(ptr, size, name)
  #define TRACY_FREE(ptr, size, name) TracyFreeN(ptr, name)

  #define TRACY_PLOT_CSTR(name, value)  TracyPlot(neam::ct::internal::unique_str<name>, value)
  #define TRACY_PLOT(name, value)  TracyPlot(name, value)
  #define TRACY_PLOT_CONFIG_CSTR(name, format) TracyPlotConfig(neam::ct::internal::unique_str<name>, format)
  #define TRACY_PLOT_CONFIG(name, format) TracyPlotConfig(name, format)

  #define TRACY_LOG(msg, size) TracyMessage(msg, size, color)
  #define TRACY_LOG_COLOR(msg, size, color) TracyMessageC(msg, size, color)
#else

  #define TRACY_FRAME_MARK
  #define TRACY_SCOPED_ZONE
  #define TRACY_SCOPED_ZONE_COLOR(c)
  #define TRACY_NAME_THREAD(x)

  #define TRACY_FRAME_MARK_START_CSTR(x)
  #define TRACY_FRAME_MARK_END_CSTR(x)

  #define TRACY_FRAME_MARK_START(x)
  #define TRACY_FRAME_MARK_END(x)

  #define TRACY_LOCKABLE(type, name) type name
  #define TRACY_LOCKABLE_BASE(type) type

  #define TRACY_ALLOC_CSTR(ptr, size, name)
  #define TRACY_FREE_CSTR(ptr, size, name)

  #define TRACY_ALLOC(ptr, size, name)
  #define TRACY_FREE(ptr, size, name)

  #define TRACY_PLOT_CSTR(name, value)
  #define TRACY_PLOT(name, value)
  #define TRACY_PLOT_CONFIG_CSTR(name, format)
  #define TRACY_PLOT_CONFIG(name, format)

  #define TRACY_LOG(msg, size)
  #define TRACY_LOG_COLOR(msg, size, color)

#endif

