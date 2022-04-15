//
// created by : Timothée Feuillet
// date: 2021-12-5
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <memory>
#include <string>

#include "../debug/unix_errors.hpp"

#include "context.hpp"


namespace neam::io
{
  context::context(const unsigned _queue_depth)
    : queue_depth(_queue_depth)
  {
    check::unx::n_assert_success(io_uring_queue_init(queue_depth, &ring, 0));
    check::unx::n_assert_success(io_uring_ring_dontfork(&ring));
  }

  context::~context()
  {
    // wait pending submissions:
    _wait_for_submit_queries();

    // close all opened files:
    for (auto& it : r_opened_files)
      check::unx::n_check_success(close(it.second));
    r_opened_files.clear();
    for (auto& it : w_opened_files)
      check::unx::n_check_success(close(it.second));
    w_opened_files.clear();

    // exit uring
    io_uring_queue_exit(&ring);
  }

  size_t context::get_file_size(id_t fid)
  {
    // try to get a fd for the file if it is already open:
    int fd = -1;
    struct stat st;
    {
      if (fd == -1)
      {
        std::lock_guard<spinlock> _prl(pending_reads_lock);
        if (const auto it = r_opened_files.find(fid); it != r_opened_files.end())
        {
          fd = it->second;
          if (check::unx::n_check_success(fstat(fd, &st)) < 0)
            return 0;
        }
      }

      if (fd == -1)
      {
        std::lock_guard<spinlock> _pwl(pending_writes_lock);
        if (const auto it = w_opened_files.find(fid); it != w_opened_files.end())
        {
          fd = it->second;
          if (check::unx::n_check_success(fstat(fd, &st)) < 0)
            return 0;
        }
      }
    }

    if (fd == -1) // use stat in place of fstat
    {
      // FIXME: NOT THREADSAFE

      if (!is_file_mapped(fid))
      {
        check::debug::n_check(is_file_mapped(fid), "Failed to open {}: file not mapped", fid);
        return 0;
      }

      if (check::unx::n_check_success(stat(get_c_filename(fid), &st)) < 0)
        return 0;
    }

    // get the actual size from the stat structure:
    if (S_ISBLK(st.st_mode))
    {
      size_t bytes;
      if (check::unx::n_check_success(ioctl(fd, BLKGETSIZE64, &bytes)) != 0)
        return 0;
      return bytes;
    }
    else if (S_ISREG(st.st_mode))
    {
      return st.st_size;
    }

    return 0;
  }

  void context::force_close_all_files()
  {
    // read then write
    std::lock_guard<spinlock> _prl(pending_reads_lock);
    std::lock_guard<spinlock> _pwl(pending_writes_lock);

    neam::cr::out().debug("io::context: forcefully moving all opened files ({}) to the to-close list", r_opened_files.size() + w_opened_files.size());
    // move the opened lists to the to-close list
    for (auto& it : r_opened_files)
      opened_files_to_be_closed.emplace(it.second);
    for (auto& it : w_opened_files)
      opened_files_to_be_closed.emplace(it.second);

    // clear everything (and the to-close lists, as now everything is to be closed)
    r_opened_files.clear();
    r_files_to_close.clear();
    w_opened_files.clear();
    w_files_to_close.clear();
  }

  void context::clear_mapped_files()
  {
    force_close_all_files();
    std::lock_guard<spinlock> _ml(mapped_lock);
    mapped_files.clear();
  }


  void context::process()
  {
    // Avoid both re-entering and concurrency on that function
    if (!process_lock.try_lock())
      return;
    std::lock_guard<spinlock> _ipl(process_lock, std::adopt_lock);

    process_completed_queries();

    // we queue the close() calls:
    {
      std::lock_guard<spinlock> _pcl(pending_close_lock);
      if (!queue_close_operations(opened_files_to_be_closed))
        return; // failed to perform the close operations, submit queue is full
    }

    // It may call process_completed_queries() when needed
    {
      std::lock_guard<spinlock> _prl(pending_reads_lock);
      if (!queue_close_operations(r_files_to_close, r_opened_files))
        return; // failed to perform the close operations, submit queue is full
    }
    {
      std::lock_guard<spinlock> _pwl(pending_writes_lock);
      if (!queue_close_operations(w_files_to_close, w_opened_files))
        return; // failed to perform the close operations, submit queue is full
    }

    // we move all the opened files to the to_close list, so that next time we attempt to close them too
    {
      std::lock_guard<spinlock> _prl(pending_reads_lock);
      for (const auto& it : r_opened_files)
        r_files_to_close.emplace(it.first);
    }
    {
      std::lock_guard<spinlock> _pwl(pending_writes_lock);
      for (const auto& it : w_opened_files)
        w_files_to_close.emplace(it.first);
    }

    // remove the close queries from the results:
    // (close queries are completed inline)
    process_completed_queries();

    queue_readv_operations();
    queue_writev_operations();
  }

  void context::process_completed_queries()
  {
    if (!completion_lock.try_lock())
      return;
    std::lock_guard<spinlock> _cl(completion_lock, std::adopt_lock);

    // batch process the completion queue:
    io_uring_cqe* cqes[queue_depth];
    const unsigned completed_count = io_uring_peek_batch_cqe(&ring, cqes, queue_depth);

    if (completed_count > 0)
      cr::out().debug("process_completed_queries: {} completed queries", completed_count);

    for (unsigned i = 0; i < completed_count; ++i)
      process_completed_query(cqes[i]);
  }

  void context::_wait_for_submit_queries()
  {
    if (!completion_lock.try_lock())
      return;
    std::lock_guard<spinlock> _cl(completion_lock, std::adopt_lock);

    // Send the remaining queries
    process();

    if (has_remaining_operations())
      cr::out().debug("_wait_for_submit_queries: waiting for {} read and {} write queries", submit_read_queries, submit_write_queries);

    unsigned count = 0;
    // wat for everything to be done
    while (has_remaining_operations())
    {
      io_uring_cqe* cqe;
      check::unx::n_check_success(io_uring_wait_cqe(&ring, &cqe));
      process_completed_query(cqe);

      ++count;

      // Just in case the callback added new stuff to process:
      process();
    }
    if (count > 0)
      cr::out().debug("_wait_for_submit_queries: processed a total of {} queries", count);
  }

  context::query::~query()
  {
    // cleanup the remaining allocated memory / call destructors
    for (unsigned i = 0; i < iovec_count; ++i)
    {
      operator delete (iovecs[i].iov_base);
      if (read_states)
        read_states[i].~state();
      if (write_states)
        write_states[i].~state();
    }
  }

  context::query* context::query::allocate(id_t fid, type_t t, unsigned iovec_count)
  {
    constexpr size_t callback_size = std::max(sizeof(read_chain::state), sizeof(write_chain::state));
    const size_t callback_offset = sizeof(query) + sizeof(iovec) * iovec_count;
    void* ptr = operator new(callback_offset + callback_size * iovec_count);
    query* q = (query*)ptr;
    q->fid = fid;
    q->type = t;
    q->iovec_count = iovec_count;
    if (t == type_t::read)
    {
      q->read_states = (read_chain::state*)(((uint8_t*)ptr) + callback_offset);
      q->write_states = nullptr;
      for (unsigned i = 0; i < iovec_count; ++i)
      {
        new (q->read_states + i) read_chain::state ();
      }
    }
    if (t == type_t::write)
    {
      q->read_states = nullptr;
      q->write_states = (write_chain::state*)(((uint8_t*)ptr) + callback_offset);
      for (unsigned i = 0; i < iovec_count; ++i)
      {
        new (q->write_states + i) write_chain::state ();
      }
    }
    return q;
  }


  io_uring_sqe* context::get_sqe(bool should_process)
  {
    io_uring_sqe* const sqe = io_uring_get_sqe(&ring);
    if (sqe == nullptr)
    {
      if (!should_process)
      {
        // (still) no space in queue, cannot do anything.
        neam::cr::out().debug("io::context::get_sqe: submit queue is full");
        return nullptr;
      }
      else
      {
        // we have done stuff, process that and try again
        process_completed_queries();
        return get_sqe(false);
      }
    }
    return sqe;
  }

  int context::open_file(id_t fid, int flags, std::unordered_map<id_t, int>& opened_files)
  {
    int fd = -1;
    if (const auto opened_files_it = opened_files.find(fid); opened_files_it != opened_files.end())
    {
      fd = opened_files_it->second;
    }
    else
    {
      // FIXME: can have race conditions:
      if (!is_file_mapped(fid))
      {
        check::debug::n_check(is_file_mapped(fid), "Failed to open {}: file not mapped", fid);
        return -1;
      }
      fd = check::unx::n_check_code(open(get_c_filename(fid), flags, 0644), "Failed to open {}", get_filename(fid));
//       neam::cr::out().debug("io::context::open_file: opening `{}` using flags: 0x{:X}", get_filename(fid), flags);

      if (fd < 0)
      {
        return -1;
      }

      // opened: place the file in the opened files list
      opened_files.emplace(fid, fd);
    }
    return fd;
  }

  void context::process_completed_query(io_uring_cqe* cqe)
  {
    check::debug::n_assert(cqe != nullptr, "io::context::process_completed_queries: invalid null cqe");

    query* const data = (query*)io_uring_cqe_get_data(cqe);

    if (cqe->res < 0)
    {
      if (data == nullptr)
      {
        // just a warning 'cause it's a fire-and-forget query
        check::unx::n_check_code(cqe->res, "io::context::process_completed_queries: fire-and-forget query failed");
      }
      else
      {
        check::unx::n_check_code(cqe->res, "io::context::process_completed_queries: {} query on '{}' failed",
                                 get_query_type_str(data->type),
                                 get_filename(data->fid)
                                );
      }
    }

    // Queries that are pushed without data are 'fire and forget'queries where we don't care about the result
    // We also don't care about waiting for them
    if (data != nullptr)
    {
      switch (data->type)
      {
        case query::type_t::close: break;
        case query::type_t::write: process_write_completion(*data, cqe->res >= 0);
          --submit_write_queries;
          break;
        case query::type_t::read: process_read_completion(*data, cqe->res >= 0);
          --submit_read_queries;
          break;
      }

      // cleanup:
      data->~query();
      operator delete ((void*)data); // FIXME: use a pool for that !
    }

    io_uring_cqe_seen(&ring, cqe);
  }

  bool context::queue_close_operations(std::unordered_set<int>& fds)
  {
    if (fds.size() > 0)
      cr::out().debug("queue_close_operations: {} pending close operations", fds.size());

    bool has_done_any_delete = false;
    while (fds.size() > 0)
    {
      const int fd = *fds.begin();

      // Get a SQE
      io_uring_sqe* const sqe = get_sqe(has_done_any_delete);
      if (!sqe)
        return false;

      has_done_any_delete = true;
      fds.erase(fds.begin());

      io_uring_prep_close(sqe, fd);
      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
    return true;
  }

  bool context::queue_close_operations(std::unordered_set<id_t>& files_to_close, std::unordered_map<id_t, int>& opened_files)
  {
//     if (files_to_close.size() > 0)
//       cr::out().debug("queue_close_operations: {} pending close operations", files_to_close.size());

    bool has_done_any_delete = false;
    while (files_to_close.size() > 0)
    {
      const id_t fid = *files_to_close.begin();
      const auto opened_files_it = opened_files.find(fid);
//       check::debug::n_assert(opened_files_it != opened_files.end(), "io::context::process: file in the to-close list isn't in the opened list");
      const int fd = opened_files_it->second;

      // Get a SQE
      io_uring_sqe* const sqe = get_sqe(has_done_any_delete);
      if (!sqe)
        return false;

      has_done_any_delete = true;
      opened_files.erase(opened_files_it);
      files_to_close.erase(files_to_close.begin());

      io_uring_prep_close(sqe, fd);
      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
    return true;
  }

  void context::queue_readv_operations()
  {
    // so we can call the failed states outside the read lock
    std::vector<read_chain::state> failed_states;

    {
      std::lock_guard<spinlock> _prl(pending_reads_lock);
      if (pending_reads.empty())
        return;
      //cr::out().debug("queue_readv_operations: {} pending read operations", pending_reads.size());
      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(pending_reads.begin(), pending_reads.end(), [](const read_request& a, const read_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      bool has_done_any_read = false;

      while (pending_reads.size() > 0)
      {
        const id_t fid = pending_reads.front().fid;

        // check if the file is opened, else open it:
        int fd = open_file(fid, O_RDONLY, r_opened_files);
        if (fd < 0)
        {
          // fail all the queries for the same fid:
          while (!pending_reads.empty() && fid == pending_reads.front().fid)
          {
            failed_states.emplace_back(std::move(pending_reads.front().state));
            pending_reads.pop_front();
          }

          continue;
        }

        r_files_to_close.erase(fid);

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe(has_done_any_read);
        if (!sqe)
          return;
        has_done_any_read = true;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          size_t offset = pending_reads.front().offset + pending_reads.front().size;
          for (; iovec_count < pending_reads.size(); ++iovec_count)
          {
            if (fid != pending_reads[iovec_count].fid)
              break;
            if (offset != pending_reads[iovec_count].offset)
              break;
            offset += pending_reads[iovec_count].size;
          }
        }

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::read, iovec_count);
        const size_t offset = pending_reads.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->iovecs[i].iov_len = pending_reads.front().size;
          q->iovecs[i].iov_base = operator new(pending_reads.front().size);
          q->read_states[i] = std::move(pending_reads.front().state);

          memset(q->iovecs[i].iov_base, 0, pending_reads.front().size); // so valgrind is happy, can be skipped

          pending_reads.pop_front();
        }

        io_uring_prep_readv(sqe, fd, q->iovecs, iovec_count, offset);
        io_uring_sqe_set_data(sqe, q);

        check::unx::n_check_success(io_uring_submit(&ring));
        ++submit_read_queries;
      }
    } // lock scope

    // complete the failed states outside the readlock so that they can queue read operations
    for (auto& state : failed_states)
    {
      state.complete({raw_data::unique_ptr(), 0}, false);
    }
  }

  void context::queue_writev_operations()
  {
    // so we can call the failed states outside the write lock
    std::vector<write_chain::state> failed_states;

    {
      std::lock_guard<spinlock> _pwl(pending_writes_lock);
      if (pending_writes.empty())
        return;
      //cr::out().debug("queue_writev_operations: {} pending write operations", pending_writes.size());

      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(pending_writes.begin(), pending_writes.end(), [](const write_request& a, const write_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      bool has_done_any_writes = false;

      while (pending_writes.size() > 0)
      {
        const id_t fid = pending_writes.front().fid;

        // check if the file is opened, else open it:
        int flags = O_WRONLY|O_CREAT;
        if (pending_writes.front().offset == 0)
          flags |= O_TRUNC;

        int fd = open_file(fid, flags, w_opened_files);
        if (fd < 0)
        {
          // fail all the queries for the same fid:
          while (!pending_writes.empty() && fid == pending_writes.front().fid)
          {
            failed_states.emplace_back(std::move(pending_writes.front().state));
            pending_writes.pop_front();
          }

          continue;
        }

        w_files_to_close.erase(fid);

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe(has_done_any_writes);
        if (!sqe)
          return;

        has_done_any_writes = true;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          const bool should_append = pending_writes.front().offset == append;
          size_t offset = pending_writes.front().offset + pending_writes.front().data.size;
          for (; iovec_count < pending_writes.size(); ++iovec_count)
          {
            if (fid != pending_writes[iovec_count].fid)
              break;
            if (!should_append)
            {
              if (offset != pending_writes[iovec_count].offset)
                break;
              if (pending_writes[iovec_count].offset == append)
                break;
              offset += pending_writes[iovec_count].data.size;
            }
            else
            {
              if (pending_writes[iovec_count].offset != append)
                break;
            }
          }
        }

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::write, iovec_count);
        const size_t offset = pending_writes.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->iovecs[i].iov_len = pending_writes.front().data.size;
          q->iovecs[i].iov_base = pending_writes.front().data.data.release();
          q->write_states[i] = std::move(pending_writes.front().state);
          pending_writes.pop_front();
        }

        io_uring_prep_writev(sqe, fd, q->iovecs, iovec_count, offset);
        // add the append flags if necessary
        if (offset == append)
          sqe->rw_flags |= RWF_APPEND;
        io_uring_sqe_set_data(sqe, q);

        check::unx::n_check_success(io_uring_submit(&ring));
        ++submit_write_queries;
      }
    } // lock scope

    // complete the failed states outside the readlock so that they can queue read operations
    for (auto& state : failed_states)
    {
      state.complete(false);
    }
  }

  void context::process_write_completion(query& q, bool success)
  {
    for (unsigned i = 0; i < q.iovec_count; ++i)
    {
        q.write_states[i].complete(success);
    }
  }

  void context::process_read_completion(query& q, bool success)
  {
    for (unsigned i = 0; i < q.iovec_count; ++i)
    {
      if (!success)
      {
        q.read_states[i].complete({raw_data::unique_ptr(), 0}, false);
      }
      else
      {
        // FIXME: Should be dispatched on other threads
        q.read_states[i].complete({raw_data::unique_ptr(q.iovecs[i].iov_base), q.iovecs[i].iov_len}, true);

        // We have transfered the ownership to the callback, remove the pointer
        q.iovecs[i].iov_base = nullptr;
      }
    }
  }

  const char* context::get_query_type_str(query::type_t t)
  {
    switch (t)
    {
      case query::type_t::close: return "close";
      case query::type_t::read: return "read";
      case query::type_t::write: return "write";
    }
    return "unknown";
  }
}

