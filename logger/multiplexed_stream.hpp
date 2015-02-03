//
// file : multiplexed_stream.hpp
// in : file:///home/tim/projects/reflective/reflective/multiplexed_stream.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 17/01/2015 23:03:08
//
//
// Copyright (C) 2014 Timothée Feuillet
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#ifndef __N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__
# define __N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__

#include <list>
#include <vector>
#include <ostream>
#include <sstream>
#include "../spinlock.hpp"

namespace neam
{
  namespace cr
  {
    namespace internal
    {
      /// \brief lock the internal mutex, begin the header
      static const struct _locker {} locker __attribute__((unused));

      /// \brief end the header (the header wil be repeated each time a neam::r::newline is send)
      static const struct _end_header {} end_header __attribute__((unused));

    } // namespace internal

    /// \brief append a newline to the streams and repeat the line header.
    static const struct _newline {} newline __attribute__((unused));

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
          os_delete.reserve(_oss.size());
          for (auto & it : _oss)
          {
            oss.emplace_back(&it.first);
            os_delete.push_back(it.second);
          }
        }

        ~multiplexed_stream()
        {
          size_t i = 0;
          for (auto & it : oss)
          {
            if (os_delete[i++])
              delete it;
          }
        }

        void add_stream(std::ostream &os, bool do_delete = false)
        {
          oss.emplace_back(&os);
          os_delete.push_back(do_delete);
        }

#define _N__op(T)        \
  multiplexed_stream &operator << (T type)          \
  {                                                 \
    for (auto &it : oss) *it << (type);             \
    if (!header_ended)                              \
      os_header << type;                            \
    return *this;                                   \
  }

        _N__op(short)
        _N__op(unsigned short)
        _N__op(int)
        _N__op(unsigned int)
        _N__op(long)
        _N__op(unsigned long)
        _N__op(long long)
        _N__op(unsigned long long)
        _N__op(float)
        _N__op(double)
        _N__op(long double)
        _N__op(bool)

        _N__op(const void *)
        _N__op(std::basic_streambuf<char> *)

        typedef std::ios_base &(*func_1)(std::ios_base &);
        typedef std::ios &(*func_2)(std::ios &);
        typedef std::ostream &(*func_3)(std::ostream &);

        _N__op(func_1)
        _N__op(func_2)
        multiplexed_stream &operator << (func_3 type)
        {
          for (auto & it : oss) *it << (type);
          if (type == static_cast<func_3>(std::endl))
            lock.unlock();
          return *this;
        }

        multiplexed_stream &operator << (internal::_locker)
        {
          lock.lock();
          os_header.clear();
          os_header.str("");
          header_ended = false;
          return *this;
        }

        multiplexed_stream &operator << (internal::_end_header)
        {
          header_ended = true;
          return *this;
        }

        multiplexed_stream &operator << (_newline)
        {
          header_ended = true;
          *this << "\n" << os_header.str();
          return *this;
        }

        template<typename T>
        _N__op(T &&)
        template<typename T>
        _N__op(const T &)

#undef _N__op


      protected:
        std::list<std::ostream *> oss;
        std::vector<bool> os_delete;
        neam::spinlock lock;
        std::ostringstream os_header;
        bool header_ended = false;
    };
  } // namespace cr
} // namespace neam

#endif /*__N_1961882576311675539_2078880458__MULTIPLEXED_STREAM_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

