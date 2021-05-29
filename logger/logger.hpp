//
// file : logger.hpp
// in : file:///home/tim/projects/reflective/reflective/logger.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 17/01/2015 22:51:32
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

#ifndef __N_1571082496879360556_301943581__LOGGER_HPP__
# define __N_1571082496879360556_301943581__LOGGER_HPP__

#include <experimental/source_location>

#include <string_view>
#include <list>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <vector>
#include "../spinlock.hpp"

#include "multiplexed_stream.hpp"

namespace neam
{
  namespace cr
  {
    /// \brief a non-singletoned stream_logger
    class stream_logger
    {
      public:
        /// \brief create the stream_logger
        /// \param[in] _file the output log file
        /// \param[in] _name the name of the stream (will appear on each printed line)
        stream_logger(const std::string& _file, std::string_view _name);
        stream_logger(std::initializer_list<std::pair<std::ostream &, bool>> _oss, const std::string_view _name);
        ~stream_logger();

        /// \brief Adds a new output stream to the logger
        void add_stream(std::ostream &stream, bool do_delete = false);

        /// \brief Adds a new output stream to the logger
        void remove_stream(std::ostream &stream);

        multiplexed_stream& get_multiplexed_stream() { return streams; }

        /// \brief print a debug level message
        multiplexed_stream &debug(const std::experimental::source_location& sloc = std::experimental::source_location::current());
        /// \brief print a log level message
        multiplexed_stream &info(const std::experimental::source_location& sloc = std::experimental::source_location::current());
        /// \brief print a log level message
        multiplexed_stream &log(const std::experimental::source_location& sloc = std::experimental::source_location::current());
        /// \brief print a warning level message
        multiplexed_stream &warning(const std::experimental::source_location& sloc = std::experimental::source_location::current());
        /// \brief print an error level message
        multiplexed_stream &error(const std::experimental::source_location& sloc = std::experimental::source_location::current());
        /// \brief print a critical level message
        multiplexed_stream &critical(const std::experimental::source_location& sloc = std::experimental::source_location::current());

        enum class verbosity_level : uint32_t
        {
          debug = 0,
          info = 1,
          log = 1,
          warning = 2,
          error = 3,
          critical = 4,
        };

        /// \brief holds the minimal level allowed for logging
        verbosity_level log_level = verbosity_level::debug;

        bool no_header = false;

      protected:
        double get_time() const;

        static multiplexed_stream &get_log_header(stream_logger &logger, const char *level, const std::experimental::source_location& sloc);

      protected:
        multiplexed_stream empty_stream; // holds nothing, "multiplex" no stream.
        multiplexed_stream streams;
        const std::string name;
        const std::chrono::time_point<std::chrono::system_clock> launch_time;
    };

    /// \brief the "main" logger (out)
    extern stream_logger out;
  } // namespace cr
} // namespace neam

#endif /*__N_1571082496879360556_301943581__LOGGER_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

