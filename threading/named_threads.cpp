//
// created by : Timothée Feuillet
// date: 2023-7-14
//
//
// Copyright (c) 2023 Timothée Feuillet
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


#include "../logger/logger.hpp"
#include "named_threads.hpp"

neam::threading::named_thread_t neam::threading::threads_configuration::add_named_thread(string_id id, named_thread_configuration conf)
{
  const group_t key = named_thread_id;
  if (key == 0 || key == 0xFF)
  {
    cr::out().critical("threading::threads_configuration::add_named_thread: overflow in named-thread id");
    return 0xFF;
  }

  if (auto it = rtc.named_threads.find(id); it != rtc.named_threads.end())
  {
    cr::out().warn("threading::threads_configuration::add_named_thread: Skipping add_named_thread call as a thread with the name {} is already added (thread skipped: {})", id, key);
    return it->second;
  }

  ++named_thread_id;
  rtc.named_threads.emplace(id, key);
  if (id.get_string() != nullptr)
    rtc.debug_names.emplace(key, id.get_string_view());
  rtc.configuration.emplace(key, std::move(conf));
  return key;
}

void neam::threading::resolved_threads_configuration::print_debug()
{
  cr::out().debug("----named  thread  debug----");
  if (debug_names.empty())
  {
    cr::out().debug(" there is no named threads");
  }
  else
  {
    cr::out().debug(" threads:");
    for (const auto& it : debug_names)
    {
      const auto& conf = configuration.at(it.first);
      cr::out().debug("  thread {}: {} [can run: long duration tasks: {}, task-group tasks: {}]", it.first, it.second, conf.can_run_general_long_duration_tasks, conf.can_run_general_tasks);
    }
  }
  cr::out().debug("----named  thread  debug----");
}

