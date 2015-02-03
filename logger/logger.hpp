//
// file : logger.hpp
// in : file:///home/tim/projects/reflective/reflective/logger.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 17/01/2015 22:51:32
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

#ifndef __N_1571082496879360556_301943581__LOGGER_HPP__
# define __N_1571082496879360556_301943581__LOGGER_HPP__

#include <string>
#include <list>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <vector>
#include "../spinlock.hpp"

#include "multiplexed_stream.hpp"

/// \brief used to print a lovely file / line info on the log file.
/// \see LOGGER_INFO
#define LOGGER_INFO_TPL(f, l) std::setw(67) << std::left << std::setfill('.') << (std::string(f) + " ") << ": " << std::setw(4) << std::setfill(' ') << l << std::right << ": " << neam::cr::internal::end_header

/// \brief used to print a lovely current file / line info on the log file
#define LOGGER_INFO LOGGER_INFO_TPL(__BASE_FILE__, __LINE__)

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
        stream_logger(const std::string &_file, const std::string &_name);
        stream_logger(std::initializer_list<std::pair<std::ostream &, bool>> _oss, const std::string &_name);
        ~stream_logger();

        void add_stream(std::ostream &stream, bool do_delete = false);

        /// \brief print a debug level message
        multiplexed_stream &debug();
        /// \brief print a log level message
        multiplexed_stream &info();
        /// \brief print a log level message
        multiplexed_stream &log();
        /// \brief print a warning level message
        multiplexed_stream &warning();
        /// \brief print an error level message
        multiplexed_stream &error();
        /// \brief print a critical level message
        multiplexed_stream &critical();

        /// \brief return the logger time.
        double get_time() const;

        enum class verbosity_level
        {
          debug = 0,
          info = 1,
          log = 1,
          warning = 2,
          error = 3,
          critical = 4,
        };

        /// \brief holds the minimal level allowed for logging
        verbosity_level log_level = verbosity_level::info;

      protected:
        static multiplexed_stream &get_log_header(stream_logger &logger, const std::string &level);

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

