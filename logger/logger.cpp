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


neam::cr::stream_logger::stream_logger(const std::string& _file, std::string_view _name)
  : streams({{*new std::ofstream(_file, std::ios_base::trunc), true}, {std::cout, false}}), name(_name), launch_time(std::chrono::system_clock::now())
{
  debug() << "stream_logger created from file '" << _file << "'" << '\n';
}

neam::cr::stream_logger::stream_logger(std::initializer_list<std::pair<std::ostream &, bool>> _oss, std::string_view _name)
  : streams(_oss), name(_name), launch_time(std::chrono::system_clock::now())
{
  debug() << "stream_logger created from stream list" << '\n';
}


neam::cr::stream_logger::~stream_logger()
{
  debug() << "stream_logger destructed" << '\n';
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

neam::cr::multiplexed_stream &neam::cr::stream_logger::get_log_header(stream_logger &logger, const char *level, const std::experimental::source_location& sloc)
{
  const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  const float nowf = std::chrono::duration_cast<std::chrono::microseconds>(now - logger.launch_time).count() / 1000000.0f;

  const std::string_view file = sloc.file_name();
  int i = file.size() - 1;
  for (unsigned count = 0; i > 0; --i)
  {
    // TODO: A better handling of `count` here...
    if (file[i] == '/' && ++count >= 3)
    {
      ++i;
      break;
    }
  }
  const std::string_view subpath = file.substr(i);

  return logger.streams << internal::locker
    << '[' << std::setw(14) << std::setfill(' ') << std::fixed << std::setprecision(6) << std::right << nowf << "] "
    << std::left << std::setw(8) << std::setfill(' ') << logger.name
    << ": " << std::left << std::setw(8) <<  std::setfill(' ') << level << " -- "
    << std::setw(55) << std::left << std::setfill('.') << subpath << ": " << std::setw(4) << std::setfill(' ') << sloc.line() << std::right << ": " << neam::cr::internal::end_header;
}


neam::cr::multiplexed_stream &neam::cr::stream_logger::debug(const std::experimental::source_location& sloc)
{
  if (log_level <= verbosity_level::debug)
  {
    if (no_header)
      return streams << internal::locker;
    return get_log_header(*this, "DEBUG", sloc);
  }
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::info(const std::experimental::source_location& sloc)
{
  if (log_level <= verbosity_level::info)
  {
    if (no_header)
      return streams << internal::locker;
    return get_log_header(*this, "INFO", sloc);
  }
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::log(const std::experimental::source_location& sloc)
{
  if (log_level <= verbosity_level::log)
  {
    if (no_header)
      return streams << internal::locker;
    return get_log_header(*this, "LOG", sloc);
  }
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::warning(const std::experimental::source_location& sloc)
{
  if (log_level <= verbosity_level::warning)
  {
    if (no_header)
      return streams << internal::locker;
    return get_log_header(*this, "WARNING", sloc);
  }
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::error(const std::experimental::source_location& sloc)
{
  if (log_level <= verbosity_level::error)
  {
    if (no_header)
      return streams << internal::locker;
    return get_log_header(*this, "ERROR", sloc);
  }
  return (empty_stream << internal::locker);
}

neam::cr::multiplexed_stream &neam::cr::stream_logger::critical(const std::experimental::source_location& sloc)
{
  if (no_header)
    return streams << internal::locker;
  return get_log_header(*this, "CRITICAL", sloc);
}
