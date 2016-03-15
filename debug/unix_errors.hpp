//
// file : unix_errors.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/debug/unix_errors.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 04/08/2014 14:24:51
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

#ifndef __N_14838165221451961813_480336325__UNIX_ERRORS_HPP__
# define __N_14838165221451961813_480336325__UNIX_ERRORS_HPP__

#include <cerrno>
#include <cstring>

#include <string>
#include <sstream>

namespace neam
{
  namespace debug
  {
    namespace errors
    {
      // errno / return codes (direct linux syscall return: -errno)
      template<typename T>
      struct unix_errors
      {
        static bool is_error(long code)
        {
          return code < 0;
        }

        static bool exists(long code)
        {
          return code <= 0;
        }

        static std::string get_code_name(long code)
        {
          std::ostringstream os;
          if (code == -1)
            os << "errno: " << errno;
          else if (code < -1)
            os << "code: " << -code;
          else if (!code)
            os << "success";
          return os.str();
        }

        static std::string get_description(long code)
        {
          if (code == -1)
            return strerror(errno);
          else if (code < -1)
            return strerror(-code);
          else
            return "success";
        }
      };
    } // namespace errors
  } // namespace debug
} // namespace neam

#endif /*__N_14838165221451961813_480336325__UNIX_ERRORS_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

