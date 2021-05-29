//
// file : multiplexed_stream.hpp
// in : file:///home/tim/projects/reflective/reflective/multiplexed_stream.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 17/01/2015 23:03:08
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

#ifndef __N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__
# define __N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__

#include <algorithm>
#include <vector>
#include <deque>
#include <mutex>
#include <ostream>
#include <sstream>
#include "../spinlock.hpp"

#ifdef _MSC_VER
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

namespace neam
{
  namespace cr
  {
    namespace internal
    {
      /// \brief lock the internal mutex, begin the header
      struct _locker {};
      [[maybe_unused]] static constexpr _locker locker = _locker{};

      /// \brief end the header (the header wil be repeated each time a neam::r::newline is send)
      struct _end_header {};
      [[maybe_unused]] static constexpr _end_header end_header = _end_header{};
    } // namespace internal

    /// \brief append a newline to the streams and repeat the line header.
    struct _newline {};
    [[maybe_unused]] static constexpr _newline newline = _newline{};
    struct _end{};
    [[maybe_unused]] static constexpr _end end = _end{};

    using stream_callback_t = void(*)(void*);

    /// \brief multiplexing output stream (only for the << operator).
    /// This class also allow thread safe line output.
    /// \note a proper use of the class is
    ///       \code multiplexed_stream_instance << internal::locker <<  ... << internal::end_header << ... << ... << ... << std::endl; \endcode
    /// \note unlocking is done by std::endl;
    class multiplexed_stream
    {
      public:
        multiplexed_stream() {}

        multiplexed_stream(std::initializer_list<std::pair<std::ostream &, bool>> _oss)
        {
          for (auto &it : _oss)
            add_stream(it.first, it.second);
        }

        ~multiplexed_stream()
        {
          for (auto &it : oss)
          {
            if (it.second)
              delete &it.first;
          }
        }

        void add_stream(std::ostream &os, bool do_delete = false)
        {
          const std::lock_guard<neam::spinlock> _u(lock);
          oss.emplace_back(&os, do_delete);
          initial_states.emplace_back(nullptr).copyfmt(os);
        }

        void remove_stream(std::ostream &os)
        {
          const std::lock_guard<neam::spinlock> _u(lock);
          size_t index = std::find_if(oss.begin(), oss.end(), [&os](const auto& a) { return a.first == &os; } ) - oss.begin();
          if (index < oss.size())
          {
            std::swap(oss[index], oss.back());
            oss.pop_back();
            // std::ios is not movable, not swappable, so this is the workaround:
            initial_states[index].copyfmt(initial_states.back());
            initial_states.pop_back();
          }
        }

        void add_callback(stream_callback_t callback, void* data)
        {
          const std::lock_guard<neam::spinlock> _u(lock);
          callbacks.push_back({callback, data});
        }

        void remove_callback(stream_callback_t callback, void* data)
        {
          const std::lock_guard<neam::spinlock> _u(lock);
          callbacks.erase(std::find(callbacks.begin(), callbacks.end(), std::pair<stream_callback_t, void*>{callback, data}));
        }

        typedef std::ostream &(*func_3)(std::ostream &);

        multiplexed_stream &operator << (func_3 type)
        {
          for (auto &it : oss)
            (*it.first) << (type);
          if (type == static_cast<func_3>(std::endl))
          {
            for (auto& it : callbacks)
              it.first(it.second);
            lock.unlock();
          }
          return *this;
        }

        template<typename T>
        multiplexed_stream &operator << (T&& type)
        {
          using type_t = std::remove_cv_t<std::decay_t<T>>;
          if constexpr (std::is_same_v<type_t, _end>)
          {
            for (auto &it : oss)
              (*it.first) << std::endl;
            for (auto& it : callbacks)
              it.first(it.second);
            lock.unlock();
            return *this;
          }
          else if constexpr(std::is_same_v<type_t, internal::_locker>)
          {
            lock.lock();
            os_header = std::ostringstream{};
            header_ended = false;
            for (unsigned i = 0; i < initial_states.size(); ++i)
              oss[i].first->copyfmt(initial_states[i]);
          }
          else if constexpr(std::is_same_v<type_t, internal::_end_header>)
          {
            header_ended = true;
          }
          else if constexpr(std::is_same_v<type_t, _newline>)
          {
            header_ended = true;
            *this << "\n";
            for (auto& it : callbacks)
              it.first(it.second);
            *this << os_header.str();
          }
          else
          {
            for (auto &it : oss)
              (*it.first) << std::forward<T>(type);
            if (!header_ended)
              os_header << std::forward<T>(type);
          }
          return *this;
        }

      protected:
        std::vector<std::pair<std::ostream *, bool>> oss;
        std::deque<std::ios> initial_states;
        std::vector<std::pair<stream_callback_t, void*>> callbacks;
        neam::spinlock lock;
        std::ostringstream os_header;
        bool header_ended = false;
    };
  } // namespace cr
} // namespace neam

#endif /*__N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

