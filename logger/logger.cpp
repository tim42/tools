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

#include <chrono>
#include <iostream>
#include <iomanip>
#include "logger.hpp"

namespace neam
{
  namespace cr
  {
    stream_logger out( {{std::cerr, false}}, "out");
  } // namespace r
} // namespace neam


neam::cr::stream_logger::stream_logger(const std::string &_file, const std::string &_name)
  : streams({{*new std::ofstream(_file, std::ios_base::trunc), true}, {std::cout, false}}), name(_name), launch_time(std::chrono::system_clock::now())
{
  debug() << LOGGER_INFO << "stream_logger created from file '" << _file << "'" << std::endl;
}

neam::cr::stream_logger::stream_logger(std::initializer_list<std::pair<std::ostream &, bool>> _oss, const std::string &_name)
  : streams(_oss), name(_name), launch_time(std::chrono::system_clock::now())
{
  debug() << LOGGER_INFO << "stream_logger created from stream list" << std::endl;
}


neam::cr::stream_logger::~stream_logger()
{
  debug() << LOGGER_INFO << "stream_logger destructed" << std::endl;
}

void neam::cr::stream_logger::add_stream(std::ostream &stream, bool do_delete)
{
  streams.add_stream(stream, do_delete);
}

double neam::cr::stream_logger::get_time() const
{
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now - launch_time).count() / 1000000.0f;
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::get_log_header(stream_logger &logger, const std::string &level)
{
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  return logger.streams << internal::locker << "[" << std::setw(14) << std::setfill(' ') << std::fixed << std::setprecision(6) << std::chrono::duration_cast<std::chrono::microseconds>(now - logger.launch_time).count() / 1000000.0f << "] "
         << std::left << std::setw(14) << std::setfill(' ') << logger.name
         << ": " << std::left << std::setw(8) <<  std::setfill(' ') << level << " -- ";
}


neam::cr::multiplexed_stream &neam::cr::stream_logger::debug()
{
  if (log_level <= verbosity_level::debug)
    return get_log_header(*this, "DEBUG");
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::info()
{
  if (log_level <= verbosity_level::info)
    return get_log_header(*this, "INFO");
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::log()
{
  if (log_level <= verbosity_level::log)
    return get_log_header(*this, "LOG");
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::warning()
{
  if (log_level <= verbosity_level::warning)
    return get_log_header(*this, "WARNING");
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::error()
{
//   if (!do_not_taint)
//     new neam::taint("LOG_ERR", "Error in logs");
  if (log_level <= verbosity_level::error)
    return get_log_header(*this, "ERROR");
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::critical()
{
  return get_log_header(*this, "CRITICAL");
}
