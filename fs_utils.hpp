//
// created by : Timothée Feuillet
// date: 2023-4-22
//
//
// Copyright (c) 2023 Timothée Feuillet
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

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <filesystem>
#include <deque>

namespace neam::cr
{
  static std::filesystem::file_time_type timespec_to_fstime(struct timespec ts)
  {
    return std::filesystem::file_time_type
    {
      std::chrono::duration_cast<std::filesystem::file_time_type::duration>
      (
        std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec}
      )
    };
  }

  /// \brief return the more recent of either the modified time or the created time
  /// (some copy utilities seems to keep the modified date but only update the created date)
  static std::filesystem::file_time_type get_modified_or_created_time(const std::filesystem::path& p, bool& success)
  {
    // non portable way of doing that:
    struct stat st;
    if (stat(p.c_str(), &st) != 0)
    {
      success = false;
      return {};
    }
    success = true;
    std::filesystem::file_time_type m = timespec_to_fstime(st.st_mtim);
    std::filesystem::file_time_type c = timespec_to_fstime(st.st_ctim);
    return c > m ? c : m;
  }

  static bool is_file_newer_than(const std::filesystem::path& p, std::filesystem::file_time_type t)
  {
    bool c;
    std::filesystem::file_time_type ft = get_modified_or_created_time(p, c);
    if (!c)
      return false; // file might not exist
    return ft >= t;
  }
  static bool is_file_newer_than(const std::filesystem::path& p, const std::filesystem::path& ref)
  {
    bool c;
    std::filesystem::file_time_type ft = get_modified_or_created_time(p, c);
    if (!c)
      return false; // file might not exist
    std::filesystem::file_time_type reft = get_modified_or_created_time(ref, c);
    if (!c)
      return true; // file might not exist
    return ft >= reft;
  }

  static std::filesystem::file_time_type get_oldest_timestamp(const std::filesystem::path& a, const std::filesystem::path& b)
  {
    bool ac;
    bool bc;
    std::filesystem::file_time_type at = get_modified_or_created_time(a, ac);
    std::filesystem::file_time_type bt = get_modified_or_created_time(b, bc);

    if (!ac && !bc)
      return std::filesystem::file_time_type{}; // files might not exist
    if (!ac)
      return bt; // file might not exist
    if (!bc)
      return at; // file might not exist
    return at < bt ? at : bt;
  }

  static std::deque<std::filesystem::path> get_all_files_recursive(const std::filesystem::path& root)
  {
    std::deque<std::filesystem::path> ret;
    std::error_code c;
    for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied, c))
    {
      if (c) continue;
      if (dir_entry.is_regular_file() || dir_entry.is_symlink())
        ret.emplace_back(dir_entry.path().lexically_relative(root));
    }
    return ret;
  }
  static std::deque<std::filesystem::path> get_all_files(const std::filesystem::path& root)
  {
    std::deque<std::filesystem::path> ret;
    std::error_code c;
    for (auto const& dir_entry : std::filesystem::directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied, c))
    {
      if (c) continue;
      if (dir_entry.is_regular_file() || dir_entry.is_symlink())
        ret.emplace_back(dir_entry.path().lexically_relative(root));
    }
    return ret;
  }

  static std::deque<std::filesystem::path> filter_files_newer_than(const std::deque<std::filesystem::path>& files, const std::filesystem::path& root, std::filesystem::file_time_type t)
  {
    std::deque<std::filesystem::path> ret;
    for (auto const& file : files)
    {
      if (is_file_newer_than(root/file, t))
        ret.emplace_back(file);
    }
    return ret;
  }

  static std::deque<std::filesystem::path> filter_files_newer_than(const std::deque<std::filesystem::path>& files, const std::filesystem::path& root, const std::filesystem::path& ref)
  {
    bool c;
    std::filesystem::file_time_type reft = get_modified_or_created_time(ref, c);
    if (!c) return {};

    return filter_files_newer_than(files, root, reft);
  }
}

