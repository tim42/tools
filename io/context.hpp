//
// created by : Timothée Feuillet
// date: 2021-11-26
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

// must be before liburing.h, otherwise there's some weird compilation errors
#include "../tracy.hpp"


#include <liburing.h>

#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "../scoped_flag.hpp"

#include "../debug/assert.hpp" // for check::

#include "../id/string_id.hpp"
#include "../id/id.hpp"

#include "../async/async.hpp"
#include "../raw_data.hpp"
#include "../spinlock.hpp"

namespace neam::io
{
  /// \brief manage file access. Mostly optimized for reads.
  /// \warning only one thread should ever call process, process_completed_queries AND _wait_for_submit_queries
  ///
  /// FIXME: The flag handling is a mess.
  class context
  {
    private:

    public:
      using read_chain = async::chain<raw_data&& /*data*/, bool /*success*/>;
      using write_chain = async::chain<bool /*success*/>;

      static constexpr size_t whole_file = ~uint64_t(0);
      static constexpr size_t append = ~uint64_t(0); // for writes only, indicate we want to append

      explicit context(const unsigned _queue_depth = 32);

      ~context();

      /// \brief Tweak some of the behavior to avoid bad cases in multi-threaded contexts.
      /// \note if true, process() will have to be manually called (see the warning on the class comment)
      void is_used_across_threads(bool multithreaded)
      {
        is_called_on_multiple_threads = multithreaded;
      }

#if N_ASYNC_USE_TASK_MANAGER
      /// \brief User-provided callbacks will be called using the task manager if set by the user
      ///        If unset, they will be called in the context of the function
      void set_deferred_execution_to_user_default()
      {
        task_manager = nullptr;
        group_id = threading::k_invalid_task_group;
      }

      /// \brief Force deferred execution on another thread. If the chain is already deferred, it will not change the provided info
      ///        But for tasks without deferred execution, those settings will be used.
      /// \note This only applies to chains whose function is set. For the others, the callback will be called when they are set.
      void force_deferred_execution(threading::task_manager* tm, threading::group_t id)
      {
        task_manager = tm;
        group_id = id;
      }
#endif

      const std::string& get_prefix_directory() const { return prefix_directory; }
      void set_prefix_directory(std::string prefix)
      {
        // prefix change means conflicts, we force close all opened files:
        force_close_all_files();

        prefix_directory = std::move(prefix);
      }

      /// \note does not actually open a file, does not actually checks for a file existence
      ///       it simply "maps a file", and makes it visible to the context. Files are opened and closed
      ///       as needed.
      id_t map_file(const std::string& path)
      {
        id_t fid = get_file_id(path);
        std::string filename = fmt::format("{}{}{}", prefix_directory, (prefix_directory.empty() ? "" : "/"), path);

        std::lock_guard<spinlock> _ml(mapped_lock);
        mapped_files.insert_or_assign(fid, std::move(filename));
        return fid;
      }

      id_t map_unprefixed_file(std::string path)
      {
        id_t fid = get_file_id(path);

        std::lock_guard<spinlock> _ml(mapped_lock);
        mapped_files.insert_or_assign(fid, std::move(path));
        return fid;
      }

      static id_t get_file_id(const std::string& path)
      {
        return string_id::_runtime_build_from_string(path.data(), path.size());
      }

      void unmap_file(id_t fid)
      {
        std::lock_guard<spinlock> _ml(mapped_lock);
        mapped_files.erase(fid);
      }

      /// \brief Force a close operation on all the files
      /// \note the close operation is not immediate and is low priority
      ///       but there will be no contamination on the previously opened files
      ///       and the newly opened ones (FDs wont be reused past this point)
      void force_close_all_files();

      /// \brief Clear all mapped files
      void clear_mapped_files();

      bool is_file_mapped(id_t fid) const
      {
        if (fid == id_t::invalid)
          return false;
        if (fid == id_t::none)
          return false;

        std::lock_guard<spinlock> _ml(mapped_lock);
        if (auto it = mapped_files.find(fid); it != mapped_files.end())
          return true;
        return false;
      }

      /// \brief returns on-disk size of the file
      size_t get_file_size(id_t fid);

      /// \brief queue a read operation
      /// \param do_not_call_process should only be set to true when you follow the current call by another queue_read on the same file (for stuff like chunked-readig)
      /// \note may trigger a process if the waiting queue becomes too long
      /// \warning order of reads is not guaranteed unless reads are on contiguous chunks of data
      ///          (contiguous: offset + size = next_read_offset)
      ///          (in this case the order will be from the lowest offset to the highest one, not the submission order)
      read_chain queue_read(id_t fid, size_t offset, size_t size, bool do_not_call_process = false)
      {
        check::debug::n_assert(size > 0, "Reads of size 0 are invalid");

        if (size == whole_file)
          size = get_file_size(fid);

        read_chain ret;
        size_t pending_size = 0;
        {
          std::lock_guard<spinlock> _prl(pending_reads_lock);
          pending_reads.push_back({fid, offset, size, ret.create_state()});
          pending_size = pending_reads.size();

          r_files_to_open.emplace(fid);
          r_files_to_close.erase(fid);
        }
        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }

      /// \brief queue a write operation
      /// \param do_not_call_process should only be set to true when you follow the current call by another queue_write on the same file (for stuff like chunked-readig)
      /// \note an offset of 0 will truncate the file if it isn't already opened)
      /// \note may trigger a process if the waiting queue becomes too long
      /// \warning order of writes are not guranteed unless writes are on contiguous chunks of data
      ///          (contiguous: offset + data.size = next_write_offset)
      ///          (in this case the order will be from the lowest offset to the highest one, not the submission order)
      write_chain queue_write(id_t fid, size_t offset, raw_data&& data, bool do_not_call_process = false)
      {
        check::debug::n_assert(data.size > 0, "Writes of size 0 are invalid");

        write_chain ret;
        size_t pending_size = 0;
        {
          std::lock_guard<spinlock> _pwl(pending_writes_lock);
          pending_writes.push_back({fid, offset, std::move(data), ret.create_state()});
          pending_size = pending_writes.size();

          w_files_to_open.emplace(fid);
          w_files_to_close.erase(fid);
        }

        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }

      bool has_remaining_operations() const
      {
        // read then write
        std::lock_guard<spinlock> _prl(pending_reads_lock);
        std::lock_guard<spinlock> _pwl(pending_writes_lock);
        return submit_read_queries != 0 || submit_write_queries != 0;
      }

      /// \brief process the queue
      void process();

      /// \brief Only process completed queries
      /// \note Do not do any waiting
      void process_completed_queries();

      /// \brief Very much like process_completed_queries, but is instead locking
      /// \note deafeats the purpose of using asynchronous IO.
      ///       Should only be used in contexts where synchronicity is required.
      ///       (like application destruction, where nothing else can be done, ...)
      /// \note Please use process_completed_queries when possible, and when not possible please make it possible
      ///       This is kind of a last-resort function
      void _wait_for_submit_queries();

      std::string_view get_filename(id_t fid) const
      {
        if (fid == id_t::invalid)
          return "id:invalid";
        if (fid == id_t::none)
          return "id:none";
        std::lock_guard<spinlock> _ml(mapped_lock);
        if (auto it = mapped_files.find(fid); it != mapped_files.end())
          return it->second;
        return "id:not-a-mapped-file";
      }

      const char* get_c_filename(id_t fid) const
      {
        if (fid == id_t::invalid)
          return nullptr;
        if (fid == id_t::none)
          return nullptr;
        std::lock_guard<spinlock> _ml(mapped_lock);
        if (auto it = mapped_files.find(fid); it != mapped_files.end())
          return it->second.c_str();
        return nullptr;
      }

    private: // data structure:
      struct read_request
      {
        id_t fid;
        size_t offset;
        size_t size;

        read_chain::state state;
      };
      struct write_request
      {
        id_t fid;
        size_t offset;
        raw_data data;

        write_chain::state state;
      };

      // user data sent to io-uring
      struct query
      {
        id_t fid;

        enum class type_t
        {
          close,
          read,
          write
        } type;


        unsigned iovec_count;
        read_chain::state* read_states;
        write_chain::state* write_states;
        iovec iovecs[];


        ~query();

        static query* allocate(id_t fid, type_t t, unsigned iovec_count);
      };

    private: // functions:
      io_uring_sqe* get_sqe(bool should_process);

      int open_file(id_t fid, int flags, std::unordered_map<id_t, int>& opened_files);

      void process_completed_query(io_uring_cqe* cqe);


      bool queue_close_operations(std::unordered_set<int>& fds);
      bool queue_close_operations(std::unordered_set<id_t>& files_to_close, std::unordered_map<id_t, int>& opened_files);

      void queue_readv_operations();

      void queue_writev_operations();

      void process_write_completion(query& q, bool success);

      void process_read_completion(query& q, bool success);

      static const char* get_query_type_str(query::type_t t);

    private: // members:
      unsigned queue_depth;
      io_uring ring;

      std::string prefix_directory;

      // avoid re-entering in stuff we should never re-enter:
      spinlock process_lock;
      spinlock completion_lock;

      mutable spinlock mapped_lock;
      std::unordered_map<id_t, std::string> mapped_files;

      mutable spinlock pending_reads_lock;
      std::deque<read_request> pending_reads;
      std::unordered_map<id_t, int> r_opened_files;
      std::unordered_set<id_t> r_files_to_open;
      std::unordered_set<id_t> r_files_to_close;
      unsigned submit_read_queries = 0;

      mutable spinlock pending_writes_lock;
      std::deque<write_request> pending_writes;
      std::unordered_map<id_t, int> w_opened_files;
      std::unordered_set<id_t> w_files_to_open;
      std::unordered_set<id_t> w_files_to_close;
      unsigned submit_write_queries = 0;

      mutable spinlock pending_close_lock;
      std::unordered_set<int> opened_files_to_be_closed; // list of fd to close

      unsigned last_process_pending_read_size = 0;
      static constexpr unsigned k_max_pending_queue_size = 20; // above this, it will trigger a process call()

      bool is_called_on_multiple_threads = true;

#if N_ASYNC_USE_TASK_MANAGER
      threading::task_manager* task_manager = nullptr;
      threading::group_t group_id = threading::k_invalid_task_group;
#endif
  };
}
