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

#if __has_include(<fmt/format.h>)
  #define N_LOGGER_USE_FMT 1
  #define N_LOGGER_CRIPPLED 0
  #include <fmt/format.h>
#elif __has_include(<format>)
  #define N_LOGGER_USE_FMT 0
  #define N_LOGGER_CRIPPLED 0
  #include <format>
#else
  #define N_LOGGER_USE_FMT 0
  #define N_LOGGER_CRIPPLED 1
  #warning "Please update the compiler or add fmt, neam::cr::logger is crippled"
#endif


#include <vector>
#include <source_location>
#include <string>
#include "../spinlock.hpp"


#ifndef N_LOGGER_STRIP_DEBUG
  #define N_LOGGER_STRIP_DEBUG 0
#endif


namespace neam::cr
{
  class logger
  {
    public:
      enum class severity
      {
        debug,
        message,
        warning,
        error,
        critical,
      };

      severity min_severity = severity::debug;

      bool can_log(severity s) const
      {
        return (int)s >= (int)min_severity;
      }

      static const char* severity_to_str(severity s)
      {
        switch (s)
        {
          case severity::debug: return "debug";
          case severity::message: return "message";
          case severity::warning: return "warning";
          case severity::error: return "error";
          case severity::critical: return "critical";
        }
        return "unknown";
      }
      static const char* severity_abbr(severity s)
      {
        switch (s)
        {
          case severity::debug: return "DEBG";
          case severity::message: return "MESG";
          case severity::warning: return "WARN";
          case severity::error: return "ERR";
          case severity::critical: return "CRIT";
        }
        return "????";
      }

      void log_str(severity s, const std::string& str, std::source_location loc = std::source_location::current());

      std::lock_guard<spinlock> acquire_lock() { return std::lock_guard<spinlock>{lock}; }
      void wait_for_lock() { lock._wait_for_lock(); }

#if N_LOGGER_USE_FMT
      template<typename... Args> using format_string = fmt::format_string<Args...>;
#else
      template<typename... Args> using format_string = const char*;
#endif

#if N_LOGGER_USE_FMT
      template<typename... Args>
      void log_fmt(severity s, std::source_location loc, format_string<Args...> str, Args&&... args)
      {
        log_str(s, fmt::format(std::move(str), std::forward<Args>(args)...), loc);
      }
#elif !N_LOGGER_CRIPPLED
      template<typename... Args>
      void log_fmt(severity s, std::source_location loc, format_string<Args...> str, Args&&... args)
      {
        log_str(s, std::format(str, std::forward<Args>(args)...), loc);
      }
#else
      template<typename... Args>
      void log_fmt(severity s, std::source_location loc, format_string<Args...> str, Args&&... args)
      {
        log_str(s, str, loc);
      }
#endif

      struct log_location_helper
      {
        logger& output;
        const std::source_location loc;
        bool skip_lock = false;

        template<typename... Args>
        void log_fmt(severity s, format_string<Args...> str, Args&& ... args)
        {
          if (!skip_lock)
            output.wait_for_lock();
          output.log_fmt(s, loc, str, std::forward<Args>(args)...);
        }

        // helpers:
        template<typename... Args>
        void debug(format_string<Args...> str, Args&& ... args)
        {
#if !N_LOGGER_STRIP_DEBUG
          output.log_fmt<Args...>(severity::debug, loc, std::move(str), std::forward<Args>(args)...);
#endif
        }
        template<typename... Args>
        void log(format_string<Args...> str, Args&& ... args)
        {
          output.log_fmt<Args...>(severity::message, loc, std::move(str), std::forward<Args>(args)...);
        }
        template<typename... Args>
        void warn(format_string<Args...> str, Args&& ... args)
        {
          output.log_fmt<Args...>(severity::warning, loc, std::move(str), std::forward<Args>(args)...);
        }
        template<typename... Args>
        void error(format_string<Args...> str, Args&& ... args)
        {
          output.log_fmt<Args...>(severity::error, loc, std::move(str), std::forward<Args>(args)...);
        }

        template<typename... Args>
        void critical(format_string<Args...> str, Args&& ... args)
        {
          output.log_fmt<Args...>(severity::critical, loc, std::move(str), std::forward<Args>(args)...);
        }
      };

      log_location_helper operator()(bool skip_lock = false, std::source_location loc = std::source_location::current())
      {
        return { *this, loc, skip_lock };
      }

      // callback handling:
      // callbacks are called for each logged message that pass the min_severity filter
      using callback_t = void(*)(void*, severity, const std::string&, std::source_location);

      void register_callback(callback_t cb, void* data);
      void unregister_callback(callback_t cb, void* data);

    private:
      struct callback_context
      {
        callback_t fnc;
        void* data;
      };
      std::vector<callback_context> callbacks;

      spinlock lock;
  };

  extern logger out;


  /// \brief Neat callback for cr::logger that prints the output to the console
  void print_log_to_console(void*, neam::cr::logger::severity s, const std::string& msg, std::source_location loc);

  /// \brief Uses the same format as print_log_to_console but returns a string instead
  std::string format_log_to_string(neam::cr::logger::severity s, const std::string& msg, std::source_location loc);
}
