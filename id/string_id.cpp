//
// created by : Timothée Feuillet
// date: 2023-6-17
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


#include <string_view>
#include <unordered_map>

#include "string_id.hpp"
#include "../logger/logger.hpp"
#include "../spinlock.hpp"
#include "../frame_allocation.hpp"

namespace neam
{
#if !N_STRIP_DEBUG
  namespace internal::id::debug
  {
    namespace
    {
      struct data_t
      {
        cr::frame_allocator<> allocator;

        shared_spinlock string_map_lock;
        std::unordered_map<id_t, std::string_view> string_map;
      };
      data_t& get_debug_data() { static data_t x; return x; }
    }

    std::string_view register_string(id_t id, std::string_view view)
    {
      auto& data = get_debug_data();
      {
        std::lock_guard _el(spinlock_shared_adapter::adapt(data.string_map_lock));
        if (auto it = data.string_map.find(id); it != data.string_map.end())
        {
          if (view.size() == it->second.size() && memcmp(view.data(), it->second.data(), view.size()) == 0)
          {
            return it->second;
          }
          cr::out().critical("string_id storage: id {} has two matching strings: `{}` and `{}`", id, view, it->second);
          return {};
        }
      }
      {
        std::lock_guard _el(spinlock_exclusive_adapter::adapt(data.string_map_lock));

        // handle race condition:
        if (auto it = data.string_map.find(id); it != data.string_map.end())
        {
          if (view.size() == it->second.size() && memcmp(view.data(), it->second.data(), view.size()) == 0)
          {
            return it->second;
          }
          cr::out().critical("string_id storage: id {} has two matching strings: `{}` and `{}`", id, view, it->second);
          return {};
        }

        // add the string:
        char* new_str = (char*)data.allocator.allocate(view.size());
        memcpy(new_str, view.data(), view.size());
        data.string_map.emplace(id, std::string_view(new_str, view.size()));
        return std::string_view(new_str, view.size());
      }
    }

    std::string_view get_string_for_id(id_t id)
    {
      auto& data = get_debug_data();
      std::lock_guard _el(spinlock_shared_adapter::adapt(data.string_map_lock));
      if (auto it = data.string_map.find(id); it != data.string_map.end())
      {
        return it->second;
      }
      return {};
    }
  }
#endif
}

