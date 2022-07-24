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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

    // close all opened fd:
    for (auto& it : opened_fd)
      check::unx::n_check_success(::close(it.second.fd));

    // exit uring
    io_uring_queue_exit(&ring);
  }

  size_t context::get_file_size(id_t fid)
  {
    // try to get a fd for the file if it is already open:
    int fd = -1;
    struct stat st;
    {
      std::lock_guard _fdl(fd_lock);
      if (const auto it = opened_fd.find(fid); it != opened_fd.end())
      {
        if (!it->second.file)
          return k_invalid_file_size;
        fd = it->second.fd;
        if (check::unx::n_check_success(fstat(fd, &st)) < 0)
          return k_invalid_file_size;
      }
    }

    if (fd == -1) // use stat in place of fstat
    {
      // FIXME: NOT THREADSAFE

      if (!is_file_mapped(fid))
      {
        check::debug::n_check(is_file_mapped(fid), "Failed to open {}: file not mapped", fid);
        return k_invalid_file_size;
      }

//       if (check::unx::n_check_code(stat(get_c_filename(fid), &st), "stat failed on file: {}", get_c_filename(fid)) < 0)
      if (stat(get_c_filename(fid), &st) < 0)
        return k_invalid_file_size;
    }

    // get the actual size from the stat structure:
    if (S_ISBLK(st.st_mode))
    {
      size_t bytes;
      if (check::unx::n_check_success(ioctl(fd, BLKGETSIZE64, &bytes)) != 0)
        return k_invalid_file_size;
      return bytes;
    }
    else if (S_ISREG(st.st_mode))
    {
      return st.st_size;
    }

    return k_invalid_file_size;
  }

  void context::force_close_all_fd(bool include_sockets)
  {
    // read then write
    std::lock_guard _fdl(fd_lock);

    neam::cr::out().debug("io::context: forcefully moving all opened fd ({}) to the to-close list", opened_fd.size());

    decltype(opened_fd) temp_fd;
    temp_fd.swap(opened_fd);
    // move the opened lists to the to-close list
    for (auto& it : temp_fd)
    {
      if (include_sockets || !it.second.socket)
        _unlocked_close(it.first);
      else
        opened_fd.emplace_hint(opened_fd.end(), it.first, it.second);
    }
  }

  void context::clear_mapped_files()
  {
    force_close_all_fd();
    std::lock_guard<spinlock> _ml(mapped_lock);
    mapped_files.clear();
  }






  // network thingies:




  id_t context::register_fd(file_descriptor fd)
  {
    // create an ID for the socket
    constexpr uint64_t k_external_id_flag = 0XF000000000000000;
//     const id_t id = (id_t)(k_external_id_flag | fd.fd);
    const id_t id = (id_t)(k_external_id_flag | reinterpret_cast<uint64_t&>(fd));

    std::lock_guard<spinlock> _sl(fd_lock);
    opened_fd.erase(id); // just in case
    opened_fd.emplace(id, fd);

    return id;
  }

  id_t context::register_socket(int fd, bool accept)
  {
    return register_fd({
      .fd = fd,
      .socket = true,

      .read = !accept,
      .write = !accept,
      .accept = accept,
    });
  }

  int context::_get_fd(id_t fid) const
  {
    std::lock_guard<spinlock> _sl(fd_lock);
    if (const auto it = opened_fd.find(fid); it != opened_fd.end())
      return it->second.fd;
    return -1;
  }

  uint16_t context::get_socket_port(id_t sid) const
  {
    const int sock = _get_fd(sid);
    if (sock < 0)
      return 0;
    struct sockaddr_in6 sin;
    socklen_t len = sizeof(sin);
    if (check::unx::n_check_success(getsockname(sock, (struct sockaddr*)&sin, &len)) < 0)
      return 0;
    return ntohs(sin.sin6_port);
  }

  void context::close(id_t fid)
  {
    std::lock_guard<spinlock> _sl(fd_lock);
    _unlocked_close(fid);
  }

  void context::_unlocked_close(id_t fid)
  {
    const auto it = opened_fd.find(fid);
    if (it == opened_fd.end())
      return;
    const int fd = it->second.fd;
    opened_fd.erase(it);
    fd_to_be_closed.insert(fd);
  }

  id_t context::create_listening_socket(uint16_t port, uint32_t listen_addr, uint16_t backlog_connection_count)
  {
    const int sock = check::unx::n_check_success(socket(PF_INET, SOCK_STREAM, 0));
    if (sock == -1)
      return id_t::invalid;
    const id_t id = register_socket(sock, true);

    {
      int enable = 1;
      check::unx::n_check_success(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)));
    }
    if (port != 0)
    {
      int enable = 1;
      check::unx::n_check_success(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)));
    }

    sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(listen_addr);

    if (check::unx::n_check_success(bind(sock, (const struct sockaddr*)&srv_addr, sizeof(srv_addr))) < 0)
    {
      close(id);
      return id_t::invalid;
    }
    if (check::unx::n_check_success(listen(sock, backlog_connection_count)) < 0)
    {
      close(id);
      return id_t::invalid;
    }

    return id;
  }

  id_t context::create_socket()
  {
    const int sock = check::unx::n_check_success(socket(PF_INET, SOCK_STREAM, 0));
    if (sock == -1)
      return id_t::invalid;
    const id_t id = register_socket(sock, false);

    return id;
  }

  bool context::create_pipe(id_t& read, id_t& write)
  {
    int pipe_fds[2] = { -1, -1 };
    if (check::unx::n_check_success(pipe(pipe_fds)) < 0)
      return false;

    read = register_fd({
      .fd = pipe_fds[0],
      .pipe = true,
      .read = true,
    });

    write = register_fd({
      .fd = pipe_fds[1],
      .pipe = true,
      .write = true,
    });

    return true;
  }

  // query handling:




  void context::process()
  {
    // Avoid both re-entering and concurrency on that function
    if (!process_lock.try_lock())
      return;
    std::lock_guard<spinlock> _ipl(process_lock, std::adopt_lock);

    process_completed_queries();

    // we move all the opened files to the to_close list, so that next time we attempt to close them too
    {
      std::lock_guard<spinlock> _fdl(fd_lock);
      std::vector<id_t> to_remove;
      for (auto& it : opened_fd)
      {
        if (it.second.file && (it.second.counter++) >= k_max_cycle_to_close)
        {
          fd_to_be_closed.insert(it.second.fd);
          to_remove.push_back(it.first);
        }
      }
      for (const id_t id : to_remove)
        opened_fd.erase(id);
    }

    // It may call process_completed_queries() when needed
    {
      // first cancel stuff, then close
      queue_cancel_operations();
      process_completed_queries();
    }

    {
      if (!queue_close_operations_fd())
        return; // failed to perform the close operations, submit queue is full
    }

    // remove the close queries from the results:
    // (close queries are completed inline)
    process_completed_queries();

    queue_readv_operations();
    queue_writev_operations();
    queue_accept_operations();
    queue_connect_operations();

    process_completed_queries();

    process_deferred_operations();

    process_completed_queries();
  }

  void context::process_completed_queries()
  {
    if (!completion_lock.try_lock())
      return;
    std::lock_guard<spinlock> _cl(completion_lock, std::adopt_lock);

    // batch process the completion queue:
    io_uring_cqe* cqes[queue_depth * 2];
    const unsigned completed_count = io_uring_peek_batch_cqe(&ring, cqes, queue_depth * 2);

    if (completed_count > 0)
      cr::out().debug("process_completed_queries: {} completed queries", completed_count);
    else
      return;

    for (unsigned i = 0; i < completed_count; ++i)
      process_completed_query(cqes[i]);
  }

  void context::_wait_for_submit_queries(bool wait_for_everything)
  {
    if (!completion_lock.try_lock())
      return;
    std::lock_guard<spinlock> _cl(completion_lock, std::adopt_lock);

    // Send the remaining queries
    process();

    if (has_in_flight_operations())
      cr::out().debug("_wait_for_submit_queries: waiting in-flight operations...");

    unsigned count = 0;
    // wat for everything to be done
    while (has_in_flight_operations())
    {
      io_uring_cqe* cqe;
      check::unx::n_check_success(io_uring_wait_cqe(&ring, &cqe));
      process_completed_query(cqe);

      ++count;

      // Just in case the callback added new stuff to process:
      process();

      if (!wait_for_everything)
        break;
    }
    if (count > 0)
      cr::out().debug("_wait_for_submit_queries: processed a total of {} queries", count);
  }

  void context::_wait_for_queries()
  {
    if (!completion_lock.try_lock())
      return;
    std::lock_guard<spinlock> _cl(completion_lock, std::adopt_lock);

    process();

    // try to process everything
    process_completed_queries();

    // wait for the remaining in-flight stuff
    if (has_in_flight_operations() && !has_pending_operations())
    {
      io_uring_cqe* cqe;
      check::unx::n_check_success(io_uring_wait_cqe(&ring, &cqe));
      process_completed_query(cqe);
    }
  }


  context::query::~query()
  {
    // cleanup the remaining allocated memory / call destructors
    for (unsigned i = 0; i < iovec_count; ++i)
    {
      operator delete (iovecs[i].iov_base);
      if (type == type_t::read)
        read_states[i].~state();
      else if (type == type_t::write)
        write_states[i].~state();
    }
    if (type == type_t::accept)
    {
      accept_state->~state();
    }
    else if (type == type_t::connect)
    {
      connect_state->~state();
    }
  }

  context::query* context::query::allocate(id_t fid, type_t t, unsigned iovec_count)
  {
    size_t callback_size = 0;
    switch (t)
    {
      case type_t::read: callback_size = sizeof(read_chain::state); break;
      case type_t::write: callback_size = sizeof(write_chain::state); break;
      case type_t::accept: callback_size = sizeof(accept_chain::state); break;
      case type_t::connect: callback_size = sizeof(connect_chain::state); break;
      case type_t::close: callback_size = 0; break;
    }
    const size_t callback_offset = sizeof(query) + sizeof(iovec) * iovec_count;
    void* ptr = operator new(callback_offset + callback_size * std::max(1u, iovec_count));
    query* q = (query*)ptr;
    q->fid = fid;
    q->type = t;
    q->is_canceled = false;
    q->iovec_count = iovec_count;
    if (t == type_t::read)
    {
      q->read_states = (read_chain::state*)(((uint8_t*)ptr) + callback_offset);
      for (unsigned i = 0; i < iovec_count; ++i)
      {
        new (q->read_states + i) read_chain::state ();
      }
    }
    else if (t == type_t::write)
    {
      q->write_states = (write_chain::state*)(((uint8_t*)ptr) + callback_offset);
      for (unsigned i = 0; i < iovec_count; ++i)
      {
        new (q->write_states + i) write_chain::state ();
      }
    }
    else if (t == type_t::accept)
    {
      q->accept_state = (accept_chain::state*)(((uint8_t*)ptr) + callback_offset);
      new (q->accept_state) accept_chain::state();
    }
    else if (t == type_t::connect)
    {
      q->connect_state = (connect_chain::state*)(((uint8_t*)ptr) + callback_offset);
      new (q->connect_state) connect_chain::state();
    }
    return q;
  }


  io_uring_sqe* context::get_sqe(bool should_process)
  {
    io_uring_sqe* const sqe = io_uring_get_sqe(&ring);
    if (sqe == nullptr)
    {
      // FIXME:
//      if (!should_process)
      {
        // (still) no space in queue, cannot do anything.
        neam::cr::out().debug("io::context::get_sqe: submit queue is full");
        return nullptr;
      }
//       else
//       {
//         // we have done stuff, process that and try again
//         process_completed_queries();
//         return get_sqe(false);
//       }
    }
    return sqe;
  }

  int context::open_file(id_t fid, bool read, bool write, bool truncate, bool& try_again)
  {
    check::debug::n_assert(read || write, "io::context: cannot open a file with neither read nor write flags.");
    off_t offset = 0;
    try_again = false;

    std::lock_guard _fdl(fd_lock);
    if (const auto it = opened_fd.find(fid); it != opened_fd.end())
    {
      check::debug::n_assert(!it->second.accept, "io::context: cannot perform operations other than accept() on a fd flagged for accept.");

      // reset the counter:
      it->second.counter = 0;

      bool reopen = false;
      // the file is already opened, check if it has the correct flags:
      if (read && !it->second.read)
        reopen = true;
      if (write && !it->second.write)
        reopen = true;

      if (reopen)
      {
        if (!it->second.file)
        {
          check::debug::n_check(false, "io::context::open_file: cannot change read/write mode on {}: it's not a file", fid);
          return -1;
        }

        neam::cr::out().debug("io::context::open_file: reopening {} with a different mode", fid);
        read = read || it->second.read;
        write = write || it->second.write;
        truncate = false;

        offset = lseek(it->second.fd, 0, SEEK_CUR);

        // move the current fd to be closed
        fd_to_be_closed.emplace(it->second.fd);
        it->second.fd = -1;
      }
      else
      {
        return it->second.fd;
      }
    }

    // not already opened, open it
    std::lock_guard<spinlock> _ml(mapped_lock);
    if (auto it = mapped_files.find(fid); it != mapped_files.end())
    {
      // avoid busting the fd limit
      if (opened_fd.size() >= k_max_open_file_count)
      {
        try_again = true;
        return -1;
      }

      // compute the flags:
      int flags = O_CLOEXEC; // our file fd (as compared to socket/pipe/... fd) are auto-managed, and should not be relied upon
      if (read && write)
        flags |= O_RDWR;
      else if (read)
        flags |= O_RDONLY;
      else if (write)
        flags = O_WRONLY|O_CREAT;

      if (truncate)
        flags |= O_TRUNC;

//       const int fd = check::unx::n_check_code(open(it->second.c_str(), flags, 0644), "Failed to open {}", it->second.c_str());
      const int fd = open(it->second.c_str(), flags, 0644);
      if (offset != 0) // restore the offset
        lseek(fd, offset, SEEK_SET);
      neam::cr::out().debug("io::context::open_file: opening `{}` [read: {}, write: {}]", it->second.c_str(), read, write);

      if (fd < 0)
      {
        opened_fd.erase(fid);
        return -1;
      }

      // opened: place the file in the opened files list
      opened_fd.insert_or_assign(fid, file_descriptor
      {
        .fd = fd,
        .socket = false,
        .file = true,
        .read = read,
        .write = write,
        .accept = false,
      });
      return fd;
    }
    else
    {
      opened_fd.erase(fid);
      check::debug::n_check(false, "Failed to open {}: file not mapped", fid);
      return -1;
    }
  }

  void context::process_completed_query(io_uring_cqe* cqe)
  {
    check::debug::n_check(cqe != nullptr, "io::context::process_completed_queries: invalid null cqe");
    if (cqe == nullptr)
      return;

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
        check::unx::n_check_code(cqe->res && !data->is_canceled, "io::context::process_completed_queries: {} query on '{}' failed",
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

        case query::type_t::write: process_write_completion(*data, cqe->res >= 0, cqe->res);
          write_requests.decrement_in_flight();
          break;
        case query::type_t::read: process_read_completion(*data, cqe->res >= 0, cqe->res);
          read_requests.decrement_in_flight();
          break;
        case query::type_t::accept: process_accept_completion(*data, cqe->res >= 0, cqe->res);
          accept_requests.decrement_in_flight();
          break;
        case query::type_t::connect: process_connect_completion(*data, cqe->res >= 0);
          connect_requests.decrement_in_flight();
          break;
      }

      // cleanup:
      data->~query();
      operator delete ((void*)data); // FIXME: use a pool for that !
    }

    io_uring_cqe_seen(&ring, cqe);
  }

  bool context::queue_close_operations_fd()
  {
    std::lock_guard _fdl(fd_lock);
    if (fd_to_be_closed.size() > 0)
      cr::out().debug("queue_close_operations_fd: {} pending close operations", fd_to_be_closed.size());
    else
      return true;

    bool has_done_any_delete = false;
    while (fd_to_be_closed.size() > 0)
    {
      const int fd = *fd_to_be_closed.begin();
      // Get a SQE
      io_uring_sqe* const sqe = get_sqe(has_done_any_delete);
      if (!sqe)
      {
        cr::out().debug("queue_close_operations_fd: done, incomplete");
        return false;
      }

      has_done_any_delete = true;
      fd_to_be_closed.erase(fd_to_be_closed.begin());

      io_uring_prep_close(sqe, fd);
      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
    cr::out().debug("queue_close_operations_fd: done !");
    return true;
  }

  void context::cancel_operation(query& q)
  {
    std::lock_guard _fdl(fd_lock);
    to_be_canceled.push_back(&q);
  }

  void context::queue_cancel_operations()
  {
    std::lock_guard _fdl(fd_lock);
    if (to_be_canceled.size() > 0)
      cr::out().debug("queue_cancel_operations: {} pending cancel operations", to_be_canceled.size());

    while (to_be_canceled.size() > 0)
    {
      query* q = to_be_canceled.front();

      // Get a SQE
      io_uring_sqe* const sqe = get_sqe(true);
      if (!sqe)
        return;

      to_be_canceled.pop_front();

      q->is_canceled = true;

      io_uring_prep_cancel(sqe, q, 0);
      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
  }

  void context::queue_readv_operations()
  {
    // so we can call the failed states outside the read lock
    std::vector<read_chain::state> failed_states;

    {
      std::lock_guard _prl(read_requests.lock);
      if (read_requests.requests.empty())
        return;
      //cr::out().debug("queue_readv_operations: {} pending read operations", pending_reads.size());
      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(read_requests.requests.begin(), read_requests.requests.end(), [](const read_request& a, const read_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      bool has_done_any_read = false;

      while (read_requests.requests.size() > 0)
      {
        if (read_requests.requests.front().state.is_canceled())
        {
          read_requests.requests.pop_front();
          continue;
        }

        const id_t fid = read_requests.requests.front().fid;

        // check if the file is opened, else open it:
        bool try_again;
        const int fd = open_file(fid, true, false, false, try_again);
        if (fd < 0)
        {
          if (try_again)
            break;
          // fail all the queries for the same fid:
          while (!read_requests.requests.empty() && fid == read_requests.requests.front().fid)
          {
            failed_states.emplace_back(std::move(read_requests.requests.front().state));
            read_requests.requests.pop_front();
          }

          continue;
        }

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe(has_done_any_read);
        if (!sqe)
          return;
        has_done_any_read = true;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          size_t offset = read_requests.requests.front().offset + read_requests.requests.front().size;
          for (; iovec_count < read_requests.requests.size() && iovec_count < k_max_iovec_merge; ++iovec_count)
          {
            if (fid != read_requests.requests[iovec_count].fid)
              break;
            if (offset != read_requests.requests[iovec_count].offset)
              break;
            offset += read_requests.requests[iovec_count].size;
          }
        }

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::read, iovec_count);
        const size_t offset = read_requests.requests.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->iovecs[i].iov_len = read_requests.requests.front().size;
          q->iovecs[i].iov_base = operator new(read_requests.requests.front().size);
          q->read_states[i] = std::move(read_requests.requests.front().state);

          memset(q->iovecs[i].iov_base, 0, read_requests.requests.front().size); // so valgrind is happy, can be skipped

          read_requests.requests.pop_front();
        }

        io_uring_prep_readv(sqe, fd, q->iovecs, iovec_count, offset);
        io_uring_sqe_set_data(sqe, q);

        check::unx::n_check_success(io_uring_submit(&ring));
        ++read_requests.in_flight;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->read_states[i].on_cancel([q, this]
          {
            cancel_operation(*q);
          });
        }
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
      std::lock_guard _pwl(write_requests.lock);
      if (write_requests.requests.empty())
        return;
      //cr::out().debug("queue_writev_operations: {} pending write operations", pending_writes.size());

      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(write_requests.requests.begin(), write_requests.requests.end(), [](const write_request& a, const write_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      bool has_done_any_writes = false;

      while (write_requests.requests.size() > 0)
      {
        if (write_requests.requests.front().state.is_canceled())
        {
          write_requests.requests.pop_front();
          continue;
        }

        const id_t fid = write_requests.requests.front().fid;

        // check if the file is opened, else open it:
        bool try_again;
        const int fd = open_file(fid, false, true, write_requests.requests.front().offset == 0, try_again);
        if (fd < 0)
        {
          if (try_again)
            break;
          // fail all the queries for the same fid:
          while (!write_requests.requests.empty() && fid == write_requests.requests.front().fid)
          {
            failed_states.emplace_back(std::move(write_requests.requests.front().state));
            write_requests.requests.pop_front();
          }

          continue;
        }

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe(has_done_any_writes);
        if (!sqe)
          return;

        has_done_any_writes = true;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          const bool should_append = write_requests.requests.front().offset == append;
          size_t offset = write_requests.requests.front().offset + write_requests.requests.front().data.size;
          for (; iovec_count < write_requests.requests.size() && iovec_count < k_max_iovec_merge; ++iovec_count)
          {
            if (fid != write_requests.requests[iovec_count].fid)
              break;
            if (!should_append)
            {
              if (offset != write_requests.requests[iovec_count].offset)
                break;
              if (write_requests.requests[iovec_count].offset == append)
                break;
              offset += write_requests.requests[iovec_count].data.size;
            }
            else
            {
              if (write_requests.requests[iovec_count].offset != append)
                break;
            }
          }
        }

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::write, iovec_count);
        const size_t offset = write_requests.requests.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->iovecs[i].iov_len = write_requests.requests.front().data.size;
          q->iovecs[i].iov_base = write_requests.requests.front().data.data.release();
          q->write_states[i] = std::move(write_requests.requests.front().state);
          write_requests.requests.pop_front();
        }

        io_uring_prep_writev(sqe, fd, q->iovecs, iovec_count, offset);
        // add the append flags if necessary
        if (offset == append)
          sqe->rw_flags |= RWF_APPEND;
        io_uring_sqe_set_data(sqe, q);

        check::unx::n_check_success(io_uring_submit(&ring));
        ++write_requests.in_flight;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->write_states[i].on_cancel([q, this]
          {
            cancel_operation(*q);
          });
        }
      }
    } // lock scope

    // complete the failed states outside the readlock so that they can queue read operations
    for (auto& state : failed_states)
    {
      state.complete(false);
    }
  }

  void context::queue_accept_operations()
  {
    std::lock_guard _al(accept_requests.lock);
    if (accept_requests.requests.empty())
      return;
    //cr::out().debug("queue_writev_operations: {} pending write operations", pending_writes.size());

    bool has_done_any_accepts = false;

    while (accept_requests.requests.size() > 0)
    {
      if (accept_requests.requests.front().state.is_canceled())
      {
        accept_requests.requests.pop_front();
        continue;
      }

      const id_t fid = accept_requests.requests.front().fid;
      const int fd = accept_requests.requests.front().sock_fd;

      // Get a SQE
      io_uring_sqe* const sqe = get_sqe(has_done_any_accepts);
      if (!sqe)
        return;

      has_done_any_accepts = true;

      // Allocate + fill the query structure:
      query* q = query::allocate(fid, query::type_t::accept, 0);

      *(q->accept_state) = std::move(accept_requests.requests.front().state);
      accept_requests.requests.pop_front();

      io_uring_prep_accept(sqe, fd, nullptr, nullptr, 0);

      // add the append flags if necessary
      io_uring_sqe_set_data(sqe, q);

      check::unx::n_check_success(io_uring_submit(&ring));
      ++accept_requests.in_flight;

      q->accept_state->on_cancel([q, this]
      {
        cancel_operation(*q);
      });
    }
  }

  void context::queue_connect_operations()
  {
    // so we can call the failed states outside the write lock
    std::vector<connect_chain::state> failed_states;

    {
      std::lock_guard _al(connect_requests.lock);
      if (connect_requests.requests.empty())
        return;
      //cr::out().debug("queue_writev_operations: {} pending write operations", pending_writes.size());

      bool has_done_any_connects = false;

      while (connect_requests.requests.size() > 0)
      {
        if (connect_requests.requests.front().state.is_canceled())
        {
          connect_requests.requests.pop_front();
          continue;
        }

        const id_t fid = connect_requests.requests.front().fid;
        const int fd = connect_requests.requests.front().sock_fd;

        addrinfo* result;
        if (check::unx::n_check_success(getaddrinfo(connect_requests.requests.front().addr.c_str(), nullptr, nullptr, &result)) < 0)
        {
          failed_states.push_back(std::move(connect_requests.requests.front().state));
          connect_requests.requests.pop_front();
          continue;
        }

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe(has_done_any_connects);
        if (!sqe)
        {
          freeaddrinfo(result);
          return;
        }

        has_done_any_connects = true;

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::connect, 0);

        *(q->connect_state) = std::move(connect_requests.requests.front().state);
        connect_requests.requests.pop_front();

        io_uring_prep_connect(sqe, fd, result->ai_addr, result->ai_addrlen);

        // add the append flags if necessary
        io_uring_sqe_set_data(sqe, q);

        check::unx::n_check_success(io_uring_submit(&ring));
        ++connect_requests.in_flight;

        q->connect_state->on_cancel([q, this]
        {
          cancel_operation(*q);
        });
      }
    } // lock scope

    // complete the failed states outside the readlock so that they can queue read operations
    for (auto& state : failed_states)
    {
      state.complete(false);
    }
  }

  void context::process_deferred_operations()
  {
    while (deferred_requests.requests.size() > 0)
    {
      if (!deferred_requests.requests.front().state.is_canceled())
        deferred_requests.requests.front().state.complete();

      deferred_requests.requests.pop_front();
    }
  }

  void context::process_write_completion(query& q, bool success, size_t sz)
  {
    if (success)
      stats_total_written_bytes.fetch_add(sz, std::memory_order_relaxed);

    for (unsigned i = 0; i < q.iovec_count; ++i)
    {
#if N_ASYNC_USE_TASK_MANAGER
      q.write_states[i].set_default_deferred_info(task_manager, group_id);
#endif

      q.write_states[i].complete(success);
    }
  }

  void context::process_read_completion(query& q, bool success, size_t sz)
  {
    if (success)
      stats_total_read_bytes.fetch_add(sz, std::memory_order_relaxed);

    for (unsigned i = 0; i < q.iovec_count; ++i)
    {
#if N_ASYNC_USE_TASK_MANAGER
      q.read_states[i].set_default_deferred_info(task_manager, group_id);
#endif
      if (!success)
      {
        q.read_states[i].complete({raw_data::unique_ptr(), 0}, false);
      }
      else
      {
        // FIXME: Should be dispatched on other threads
        size_t data_len = std::min(sz, q.iovecs[i].iov_len);
        q.read_states[i].complete({raw_data::unique_ptr(q.iovecs[i].iov_base), data_len}, true);
        sz -= data_len;

        // We have transfered the ownership to the callback, remove the pointer
        q.iovecs[i].iov_base = nullptr;
      }
    }
  }

  void context::process_accept_completion(query& q, bool success, int fd)
  {
#if N_ASYNC_USE_TASK_MANAGER
    q.accept_state->set_default_deferred_info(task_manager, group_id);
#endif

    // add the new id as a socket supporting read/writes:
    if (success)
    {
      const id_t id = register_socket(fd, false);
      q.accept_state->complete(id);
    }
    else
    {
      q.accept_state->complete(id_t::invalid);
    }
  }

  void context::process_connect_completion(query& q, bool success)
  {
#if N_ASYNC_USE_TASK_MANAGER
    q.connect_state->set_default_deferred_info(task_manager, group_id);
#endif
    q.connect_state->complete(success);
  }


  const char* context::get_query_type_str(query::type_t t)
  {
    switch (t)
    {
      case query::type_t::close: return "close";
      case query::type_t::read: return "read";
      case query::type_t::write: return "write";
      case query::type_t::accept: return "accept";
      case query::type_t::connect: return "connect";
    }
    return "unknown";
  }
}

