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

#include <list>
#include <vector>
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
      struct _locker {constexpr _locker(){}};
      static constexpr _locker locker __attribute__((unused)) = _locker();

      /// \brief end the header (the header wil be repeated each time a neam::r::newline is send)
      struct _end_header {constexpr _end_header(){}};
      static constexpr _end_header end_header __attribute__((unused)) = _end_header();

    } // namespace internal

    /// \brief append a newline to the streams and repeat the line header.
    struct _newline {constexpr _newline(){}};
  	static constexpr _newline newline __attribute__((unused)) = _newline();

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

