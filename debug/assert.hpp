//
// created by : Timothée Feuillet
// date: 2021-11-24
//
//
// Copyright (c) 2021 Timothée Feuillet
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

#include <source_location>

#include "../logger/logger.hpp"
#include "../backtrace.hpp"
#include "../scoped_flag.hpp"

#ifndef N_ALLOW_DEBUG
  #define N_ALLOW_DEBUG false
#endif

#ifndef N_DISABLE_CHECKS
  #define N_DISABLE_CHECKS 0
#endif


namespace neam::debug
{
  struct dummy_error
  {
    using error_type = int;

    static bool is_error(int) { return false; }
    static bool exists(int) { return false; }
    static const char* get_code_name(int) { return "[dummy error: no code]"; }
    static const char* get_description(int) { return "[dummy error: no description]"; }
  };
  template<typename ErrorClass = dummy_error>
  class on_error
  {
    public:
      on_error() = delete;

      using error_type = typename ErrorClass::error_type;

#if !N_DISABLE_CHECKS
      template<typename... Args>
      static void _assert(std::source_location sloc, const bool test, const char* test_str, fmt::format_string<Args...> message, Args&&... args)
      {
        [[unlikely]] if (N_ALLOW_DEBUG && test && cr::out.can_log(cr::logger::severity::debug))
        {
          cr::out().log_fmt(cr::logger::severity::debug, sloc, "[ASSERT PASSED: {0}]", test_str);
          return;
        }
        [[likely]] if (test)
          return;
        {
          cr::scoped_counter _sc(thread_waiting);

          auto _sl = cr::out.acquire_lock();
          cr::out().log_fmt(cr::logger::severity::critical, sloc, "[ASSERT FAILED: {0}]: {1}", test_str, fmt::format(std::move(message), std::forward<Args>(args)...));
          cr::print_callstack(25, 1, true);
        }

        while (thread_waiting.load(std::memory_order_acquire) > 0) {}

        asm("int $3"); // break
        abort();       // abort, just in case
      }

      template<typename... Args>
      static bool _check(std::source_location sloc, const bool test, const char* test_str, fmt::format_string<Args...> message, Args&&... args)
      {
        [[unlikely]] if (N_ALLOW_DEBUG && test && cr::out.can_log(cr::logger::severity::debug))
        {
          cr::out().log_fmt(cr::logger::severity::debug, sloc, "[CHECK  PASSED: {0}]", test_str);
          return test;
        }
        [[likely]] if (test)
          return test;
        {
          cr::scoped_counter _sc(thread_waiting);

          auto _sl = cr::out.acquire_lock();
          cr::out().log_fmt(cr::logger::severity::error, sloc, "[CHECK  FAILED: {0}]: {1}", test_str, fmt::format(std::move(message), std::forward<Args>(args)...));
          cr::print_callstack(25, 1, true);
        }

//         asm("int $3"); // break

        return test;
      }

      template<typename... Args>
      static error_type _assert_code(std::source_location sloc, const error_type code, const char* test_str, fmt::format_string<Args...> message, Args&&... args)
      {
        const bool test = !ErrorClass::is_error(code);
        [[unlikely]] if (N_ALLOW_DEBUG && test && cr::out.can_log(cr::logger::severity::debug))
        {
          cr::out().log_fmt(cr::logger::severity::debug, sloc, "[ASSERT PASSED: {0} returned {1}: {2}]",
                          test_str, ErrorClass::get_code_name(code), ErrorClass::get_description(code));
          return code;
        }
        [[likely]] if (test)
          return code;
        {
          cr::scoped_counter _sc(thread_waiting);

          auto _sl = cr::out.acquire_lock();
          cr::out().log_fmt(cr::logger::severity::critical, sloc,
                          "[ASSERT FAILED: {0} returned {1}: {2}]: {3}",
                          test_str, ErrorClass::get_code_name(code), ErrorClass::get_description(code),
                          fmt::format(std::move(message), std::forward<Args>(args)...));
          cr::print_callstack(25, 1, true);
        }

        while (thread_waiting.load(std::memory_order_acquire) > 0) {}

        asm("int $3"); // break
        abort();
        return code;
      }

      template<typename... Args>
      static error_type _check_code(std::source_location sloc, const error_type code, const char* test_str, fmt::format_string<Args...> message, Args&&... args)
      {
        const bool test = !ErrorClass::is_error(code);
        [[unlikely]] if (N_ALLOW_DEBUG && test && cr::out.can_log(cr::logger::severity::debug))
        {
          cr::out().log_fmt(cr::logger::severity::debug, sloc, "[CHECK  PASSED: {0} returned {1}: {2}]",
                          test_str, ErrorClass::get_code_name(code), ErrorClass::get_description(code));
          return code;
        }
        [[likely]] if (test)
          return code;
        {
          cr::scoped_counter _sc(thread_waiting);

          auto _sl = cr::out.acquire_lock();
          cr::out().log_fmt(cr::logger::severity::error, sloc,
                          "[CHECK  FAILED: {0} returned {1}: {2}]: {3}",
                          test_str, ErrorClass::get_code_name(code), ErrorClass::get_description(code),
                          fmt::format(std::move(message), std::forward<Args>(args)...));
          cr::print_callstack(25, 1, true);
        }
//         asm("int $3"); // break
        return code;
      }
#endif

      static void _dummy() {}
      static bool _dummy(bool r) { return r; }
      static error_type _dummy_code(error_type r) { return r; }
    private:
      static std::atomic<uint32_t> thread_waiting;
  };
  template<typename ErrorClass> std::atomic<uint32_t> on_error<ErrorClass>::thread_waiting = 0;
}

#if !N_DISABLE_CHECKS

#define n_assert(test, ...)       _assert(std::source_location::current(), test, #test, __VA_ARGS__)
#define n_check(test, ...)        _check(std::source_location::current(), test, #test, __VA_ARGS__)

#define n_assert_code(expr, ...)  _assert_code(std::source_location::current(), expr, #expr, __VA_ARGS__)
#define n_assert_success(expr)    _assert_code(std::source_location::current(), expr, #expr, "expression failed")
#define n_check_code(expr, ...)   _check_code(std::source_location::current(), expr, #expr, __VA_ARGS__)
#define n_check_success(expr)     _check_code(std::source_location::current(), expr, #expr, "expression failed")

#else // N_DISABLE_CHECKS

#define n_assert(test, ...)       _dummy()
#define n_check(test, ...)        _dummy(test)

// We keep expr but strip everything else
#define n_assert_code(expr, ...)  _dummy_code(expr)
#define n_assert_success(expr)    _dummy_code(expr)
#define n_check_code(expr, ...)   _dummy_code(expr)
#define n_check_success(expr)     _dummy_code(expr)

#endif

namespace neam::check
{
  using debug = neam::debug::on_error<>;
}
