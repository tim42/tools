//
// file : assert.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/debug/assert.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 03/08/2014 16:18:37
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

#ifndef __N_1744658631925022054_1134258981__ASSERT_HPP__
# define __N_1744658631925022054_1134258981__ASSERT_HPP__

#include "../exception.hpp"

#ifndef N_DEBUG_USE_CERR
# include "../logger/logger.hpp"
#endif

#include <string>
#include <string.h>

#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <stdexcept>

//
// (yet another) nice assert/function check tool
// NOTE: you could define N_DISABLE_CHECKS to completly disable every checks.
//       you could define N_DISABLE_ASSERTS to disable asserts.
//       you could define N_DISABLE_FNC_CHECKS to disable function checks.
//       you could define N_DEBUG_USE_CERR to output only (and directly) to std::cerr
//
//       there's the macro N_COLUMN_WIDTH that you can define (either to a hardcoded value or a variable)
//        to control the width of a column. Default is 200.
//       there's the macro N_ALLOW_DEBUG that you can define (either  to a hardcoded value or a variable)
//        to allow *debug* information (like logging success and co). Default is false.
//
// NOTE: If you use reflective, you can change neam::r::conf::integrate_with_n_debug to disable/enable integration with
//       reflective. (currently the default value is to integrate neam::debug::* with reflective).
//       If you don't use reflective (what a shame...), just forget everything about that message.
//

#ifndef N_COLUMN_WIDTH
#define N_COLUMN_WIDTH 100
#endif
#ifndef N_ALLOW_DEBUG
#define N_ALLOW_DEBUG false
#endif

// integration with reflective
#ifdef N_REFLECTIVE_PRESENT
#define _NEAM_DEBUG_GET_ACTIVE_FUNC   if (neam::r::conf::integrate_with_n_debug && neam::r::function_call::get_active_function_call()) neam::r::function_call::get_active_function_call()
#define NEAM_DEBUG_IND_FAIL(rsn)      _NEAM_DEBUG_GET_ACTIVE_FUNC->fail(rsn)
namespace neam
{
  namespace debug
  {
    namespace internal
    {
      static const neam::r::reason error_detected_reason = neam::r::reason {"neam::debug error detected"};
      static const neam::r::reason assert_failled_reason = neam::r::reason {"neam::debug assertion failed"};
    }
  }
}
#else
#define NEAM_DEBUG_IND_FAIL(rsn)             /* :| */
#define N_REASON_INFO
#endif

namespace neam
{
  namespace debug
  {
    template<template<typename ErrorType> class ErrorClass, template<typename> class ExceptionClass = typed_exception>
    class on_error
    {
      private:
        on_error() = delete;
        on_error(const on_error &) = delete;

      public:
        template<typename ReturnType>
        static ReturnType _throw_exception(const std::string &func_call, ReturnType ret, const std::experimental::source_location& sloc = std::experimental::source_location::current())
        {
          const bool is_error = ErrorClass<ReturnType>::is_error(ret);
          if (N_ALLOW_DEBUG || is_error)
          {
            std::string message = get_message(func_call, ret);
#ifdef N_DEBUG_USE_CERR
            std::cerr << ((is_error) ? " ** " : " -- ") << message << std::endl;
#else
            if (is_error)
            {
              NEAM_DEBUG_IND_FAIL(internal::error_detected_reason(file, line, message));
              neam::cr::out.error(sloc) << message << std::endl;
            }
            else
              neam::cr::out.debug(sloc) << message << std::endl;
#endif
            if (is_error)
              throw ExceptionClass<ErrorClass<ReturnType>>(message);
          }
          return std::move(ret);
        }

        // return true in case of error
        template<typename ReturnType>
        static bool _log_and_check(const std::string &func_call, ReturnType ret, const std::experimental::source_location& sloc = std::experimental::source_location::current())
        {
          const bool is_error = ErrorClass<ReturnType>::is_error(ret);
          if (N_ALLOW_DEBUG || is_error)
          {
            std::string message = get_message(func_call, ret);
#ifdef N_DEBUG_USE_CERR
            std::cerr << ((is_error) ? " ** " : " -- ") << message << std::endl;
#else
            if (is_error)
            {
              NEAM_DEBUG_IND_FAIL(internal::error_detected_reason(file, line, message));
              neam::cr::out.error(sloc) << message << std::endl;
            }
            else
              neam::cr::out.debug(sloc) << message << std::endl;
#endif

            if (is_error)
              return false;
            return true;
          }
          return false;
        }

        // simply log the error and return the value 'as-is'
        template<typename ReturnType>
        static ReturnType _log_and_return(const std::string &func_call, ReturnType ret, const std::experimental::source_location& sloc = std::experimental::source_location::current())
        {
          const bool is_error = ErrorClass<ReturnType>::is_error(ret);
          if (N_ALLOW_DEBUG || is_error)
          {
            std::string message = get_message(func_call, ret);
#ifdef N_DEBUG_USE_CERR
            std::cerr << ((is_error) ? " ** " : " -- ") << message << std::endl;
#else
            if (is_error)
            {
              NEAM_DEBUG_IND_FAIL(internal::error_detected_reason(file, line, message));
              neam::cr::out.error(sloc) << message << std::endl;
            }
            else
              neam::cr::out.debug(sloc) << message << std::endl;
#endif
          }
          return std::move(ret);
        }

        static void _assert(const std::string &test, bool status, const std::string message, const std::experimental::source_location& sloc = std::experimental::source_location::current())
        {
          if (N_ALLOW_DEBUG || !status)
          {
#ifdef N_DEBUG_USE_CERR
            std::cerr << ((!status) ? " ** " : " -- ") << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << (!status ? " [FAILED]: " + message : " [SUCCESS]") << std::endl;
#else
            if (!status)
            {
              NEAM_DEBUG_IND_FAIL(internal::assert_failled_reason(file, line, message));
              neam::cr::out.error(sloc) << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << "[FAILED]: " + message << std::endl;
            }
            else
              neam::cr::out.debug(sloc) << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << " [SUCCESS]" << std::endl;
#endif
            if (!status)
              throw ExceptionClass<on_error>(test + ": assert failed: " + message);
          }
        }

        static bool _assert_and_return(const std::string &test, bool status, const std::string message, const std::experimental::source_location& sloc = std::experimental::source_location::current())
        {
          if (N_ALLOW_DEBUG || !status)
          {
#ifdef N_DEBUG_USE_CERR
            std::cerr << ((!status) ? " ** " : " -- ") << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << (!status ? " [FAILED]: " + message : " [SUCCESS]") << std::endl;
#else
            if (!status)
            {
              NEAM_DEBUG_IND_FAIL(internal::assert_failled_reason(file, line, message));
              neam::cr::out.error(sloc) << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << "[FAILED]: " + message << std::endl;
            }
            else
              neam::cr::out.debug(sloc) << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (test + " ") << " [SUCCESS]" << std::endl;
#endif
          }
          return !status;
        }

        template<typename ReturnType>
        static inline ReturnType _dummy(ReturnType ret)
        {
          return (ret);
        }

#if not defined N_DISABLE_ASSERTS and not defined N_DISABLE_CHECKS
#define n_assert(test, message)                   _assert(#test, test, message)
#define n_assert_exp(test, message)               assert(test, message)
#define n_assert_and_return(test, message)        _assert_and_return(#test, test, message)
#define n_assert_and_return_exp(test, message)    assert_and_return(test, message)
#else
#define n_assert(test, message)                   _dummy((test))
#define n_assert_exp(test, message)               assert(test, message)
#define n_assert_and_return(test, message)        _dummy(((test), true))
#define n_assert_and_return_exp(test, message)    assert_and_return(test, message)
#endif

#if not defined N_DISABLE_FNC_CHECKS and not defined N_DISABLE_CHECKS
# define n_throw_exception(fnc)                   _throw_exception(#fnc, fnc)
# define n_throw_exception_exp(fnc)               throw_exception(fnc)
# define n_log_and_check(fnc)                     _log_and_check(#fnc, fnc)
# define n_log_and_check_exp(fnc)                 log_and_check(fnc)
# define n_log_and_return(fnc)                    _log_and_return(#fnc, fnc)
# define n_log_and_return_exp(fnc)                log_and_return(fnc)
#else
# define n_throw_exception(fnc)                   _dummy(fnc)
# define n_throw_exception_exp(fnc)               _dummy(fnc)
# define n_log_and_return(fnc)                    _dummy(fnc)
# define n_log_and_return_exp(fnc)                _dummy(fnc)
# define n_log_and_check(fnc)                     _dummy((fnc, true))
# define n_log_and_check_exp(fnc)                 _dummy(fnc)
#endif

      private:
        static const char *get_filename(const char *file)
        {
          return (strrchr((file), '/') ? strrchr((file), '/') + 1 : file);
        }

        template<typename ReturnType>
        static std::string get_message(const std::string &func_call, const ReturnType &code)
        {
          std::ostringstream is;

          if (ErrorClass<ReturnType>::exists(code))
          {
            const std::string code_name = ErrorClass<ReturnType>::get_code_name(code);
            const std::string description = ErrorClass<ReturnType>::get_description(code);

            is << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (func_call + " ") << " [" << code_name << "]: " << description;
          }
          else
            is << std::left << std::setw(N_COLUMN_WIDTH) << std::setfill('.') << (func_call + " ") << " [code: " << code << "]: [UNKNOW ERROR CODE]";
          return is.str();
        }
    };
  } // namespace debug
} // namespace neam

#endif /*__N_1744658631925022054_1134258981__ASSERT_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

