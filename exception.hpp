//
// file : exception.hpp (2)
// in : file:///home/tim/projects/nsched/nsched/tools/exception.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 03/08/2014 16:20:59
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

#ifndef __N_1232091940699248471_1200439019__EXCEPTION_HPP__2___
# define __N_1232091940699248471_1200439019__EXCEPTION_HPP__2___

#include <exception>
#include <string>

#include "macro.hpp"
#include "demangle.hpp"

//
// NOTE: you can define the macro N_DEBUG_USE_CERR to use std::cerr instead of neam::cr::out
//

namespace neam
{
  // the base neam exception class
  class exception : public std::exception
  {
    private:
      std::string msg;

    public:
      explicit exception(const std::string &what_arg) noexcept : msg(what_arg) {}
      explicit exception(std::string &&what_arg) noexcept : msg(std::move(what_arg)) {}
      virtual ~exception() noexcept = default;

      virtual const char *what() const noexcept
      {
        return msg.data();
      }
  };

  // an CRTP exception class that add the type name at the start of the string
  template<typename ExceptionType>
  class typed_exception : public exception
  {
    public:
      typed_exception(const std::string &s) noexcept : exception(neam::demangle<ExceptionType>() + ": " + s) {}
      typed_exception(std::string && s) noexcept : exception(neam::demangle<ExceptionType>() + ": " + std::move(s)) {}

      virtual ~typed_exception() = default;
  };

// log the line and the file of an exception
// string should be a simple quoted string
// (usage exemple: N_THROW(neam::exception, "this is the exception string !");
#define N_THROW(exception_class, string)        throw exception_class(__FILE__ ": " N_EXP_STRINGIFY(__LINE__) ": " string)
// for std::strings
#define N_THROW_STRING(exception_class, str) throw exception_class(std::string(__FILE__ ": " N_EXP_STRINGIFY(__LINE__) ": ") + str)

//
// some default catch macros
//
#ifdef N_DEBUG_USE_CERR
# define N_PRINT_EXCEPTION(e)            std::cerr << " ** " __FILE__ ": " N_EXP_STRINGIFY(__LINE__) ": catched exception  '" << e.what() << "'" << std::endl
# define N_PRINT_UNKNOW_EXCEPTION        std::cerr << " ** " __FILE__ ": " N_EXP_STRINGIFY(__LINE__) ": catched unknow exception..." << std::endl
# define N_CATCH_ACTION(x)               catch(std::exception &e) { N_PRINT_EXCEPTION(e); x; } catch(...) { N_PRINT_UNKNOW_EXCEPTION; x; }
#else
# define N_PRINT_EXCEPTION(e)            neam::cr::out.error() << "catched exception  '" << e.what() << "'" << std::endl
# define N_PRINT_UNKNOW_EXCEPTION        neam::cr::out.error() << "catched unknow exception..." << std::endl
# define N_CATCH_ACTION(x)               catch(std::exception &e) { N_PRINT_EXCEPTION(e); x; } catch(...) { N_PRINT_UNKNOW_EXCEPTION; x; }
#endif
#define N_CATCH                         N_CATCH_ACTION( )

// a verbose abort
#ifdef N_DEBUG_USE_CERR
# define N_ABORT                         do {std::cerr << " ** " __FILE__ ": " N_EXP_STRINGIFY(__LINE__) ": [aborting program]\n" << std::endl; std::cerr.flush(); abort();} while(0)
#else
# define N_ABORT                         do {neam::cr::out.error() << "[aborting program]\n" << std::endl; abort();} while(0)
#endif
// fatal exception (call abort)
#define N_FATAL_CATCH                   N_CATCH_ACTION(N_ABORT)
#define N_FATAL_CATCH_ACTION(x)         N_CATCH_ACTION({x;N_ABORT;})

} // namespace neam

#endif /*__N_1232091940699248471_1200439019__EXCEPTION_HPP__2___*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

