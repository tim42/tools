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
    signal(SIGPIPE, SIG_IGN); // ignore sigpipe, we handle that with the return value of the syscall
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
    for (int it : fd_to_be_closed)
      check::unx::n_check_success(::close(it));

    // exit uring
    io_uring_queue_exit(&ring);
  }

  context::read_chain context::queue_read(id_t fid, size_t offset, size_t size)
  {
    check::debug::n_assert(size > 0, "Reads of size 0 are invalid");
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid read operation");

    if (size == whole_file)
      size = get_file_size(fid);
    if (size == k_invalid_file_size)
      return read_chain::create_and_complete({}, false, 0);

    read_chain ret;
    read_requests.add_request({fid, offset, size, {}, 0, ret.create_state()});
    return ret;
  }

  context::read_chain context::queue_read(id_t fid, size_t offset, size_t size, raw_data&& data, uint32_t offset_in_data)
  {
    check::debug::n_assert(size > 0, "Reads of size 0 are invalid");
    check::debug::n_assert(!!data.data, "Invalid data");
    check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
    check::debug::n_assert(data.size - offset_in_data >= size, "Provided data does not have enough space for read (data size: {}, read-size: {})",
                            data.size - offset_in_data, size);
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid read operation");

    if (size == whole_file)
      size = get_file_size(fid);
    if (size == k_invalid_file_size)
      return read_chain::create_and_complete(std::move(data), false, 0);

    read_chain ret;
    read_requests.add_request({fid, offset, size, std::move(data), offset_in_data, ret.create_state()});
    return ret;
  }

  context::write_chain context::queue_write(id_t fid, size_t offset, raw_data&& data, uint32_t offset_in_data, uint32_t size_to_write)
  {
    check::debug::n_check(data.size > 0, "Writes of size 0 are invalid");
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid write operation");
    check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
    if (data.size == 0)
      return write_chain::create_and_complete(std::move(data), false, 0);
    if (size_to_write == 0)
      size_to_write = data.size - offset_in_data;
    size_to_write = std::min((uint32_t)data.size - offset_in_data, size_to_write);

    write_chain ret;
    write_requests.add_request({fid, offset, std::move(data), offset_in_data, size_to_write, ret.create_state()});
    return ret;
  }

  std::string context::get_string_for_id(id_t fid) const
  {
    if (fid == id_t::invalid)
      return "id:[invalid]";
    if (fid == id_t::none)
      return "id:[none]";
    const uint64_t u64id = (uint64_t)fid;
    if (u64id & k_external_id_flag)
    {
      const file_descriptor fd = *reinterpret_cast<file_descriptor*>(&fid);
      return fmt::format("external:[fd: {} | type: {}{}{} | caps: {}{}{}]",
                                     fd.fd,
                                     fd.socket ? "s" : "", fd.pipe ? "p" : "", fd.file ? "f" : "",
                                     fd.read ? "r" : "", fd.write ? "w" : "", fd.accept ? "a" : "");
    }
    {
      std::lock_guard<spinlock> _ml(mapped_lock);
      if (auto it = mapped_files.find(fid); it != mapped_files.end())
        return fmt::format("mapped-file:[{}]", it->second);
    }
    return "id:[unknown]";
  }

  bool context::stat_file(id_t fid, struct stat& st) const
  {
    // try to get a fd for the file if it is already open:
    int fd = -1;
    {
      std::lock_guard _fdl(fd_lock);
      if (const auto it = opened_fd.find(fid); it != opened_fd.end())
      {
        if (!it->second.file)
          return false;
        fd = it->second.fd;
        if (check::unx::n_check_success(fstat(fd, &st)) < 0)
          return false;
      }
    }

    if (fd == -1) // use stat in place of fstat
    {
      // FIXME: NOT THREADSAFE

      if (!is_file_mapped(fid))
      {
        check::debug::n_check(is_file_mapped(fid), "Failed to open {}: file not mapped", fid);
        return false;
      }

//       if (check::unx::n_check_code(stat(get_c_filename(fid), &st), "stat failed on file: {}", get_c_filename(fid)) < 0)
      if (stat(get_c_filename(fid), &st) < 0)
        return false;
    }
    return true;
  }

  size_t context::get_file_size(id_t fid) const
  {
    struct stat st;
    if (!stat_file(fid, st))
      return k_invalid_file_size;

    // get the actual size from the stat structure:
    check::debug::n_check(S_ISREG(st.st_mode), "Failed to get file size {}: file is not a regular file", fid);
    if (S_ISREG(st.st_mode))
    {
      return st.st_size;
    }

    return k_invalid_file_size;
  }

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

  std::filesystem::file_time_type context::get_modified_or_created_time(id_t fid) const
  {
    struct stat st;
    if (!stat_file(fid, st))
      return {};

    std::filesystem::file_time_type m = timespec_to_fstime(st.st_mtim);
    std::filesystem::file_time_type c = timespec_to_fstime(st.st_ctim);
    return c > m ? c : m;
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
      if (include_sockets || (!it.second.socket && !it.second.pipe))
        fd_to_be_closed.insert(it.second.fd);
      else
        opened_fd.emplace_hint(opened_fd.end(), it.first, it.second);
    }
  }

  void context::clear_mapped_files()
  {
    force_close_all_fd();
    std::lock_guard<spinlock> _ml(mapped_lock);
    neam::cr::out().debug("io::context: forcefully clearing all mapped files ({})", mapped_files.size());
    mapped_files.clear();
  }



  id_t context::stdin()
  {
    return register_fd(
    {
      .fd = 0,
      .pipe = true, // well... not always, but well...
      .read = true,
    }, true);
  }

  id_t context::stdout()
  {
    return register_fd(
    {
      .fd = 1,
      .pipe = true, // well... not always, but well...
      .write = true,
    }, true);
  }

  id_t context::stderr()
  {
    return register_fd(
    {
      .fd = 2,
      .pipe = true, // well... not always, but well...
      .write = true,
    }, true);
  }


  // network thingies:




  id_t context::register_fd(file_descriptor fd, bool skip_if_already_registered)
  {
    // create an ID for the socket
//     const id_t id = (id_t)(k_external_id_flag | fd.fd);
    const id_t id = (id_t)(k_external_id_flag | reinterpret_cast<uint64_t&>(fd));

    std::lock_guard<spinlock> _sl(fd_lock);
    if (skip_if_already_registered)
    {
      if (opened_fd.contains(id))
        return id;
    }
    opened_fd.insert_or_assign(id, fd);

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

  context::read_chain context::queue_receive(id_t fid, size_t size, raw_data&& data, uint32_t offset_in_data)
  {
    check::debug::n_assert(size > 0, "Receives of size 0 are invalid");
    if (size == everything && data.data)
      size = data.size - offset_in_data;
    if (data.data)
    {
      check::debug::n_assert(!!data.data, "Invalid data");
      check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
      check::debug::n_assert(data.size - offset_in_data >= size, "Provided data does not have enough space for read (data size: {}, read-size: {})",
                              data.size - offset_in_data, size);
    }
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid receive operation");

    read_chain ret;
    recv_requests.add_request(
    {
      .fid = fid,
      .sock_fd = _get_fd(fid),
      .data = std::move(data),
      .offset_in_data = offset_in_data,
      .size_to_recv = size,
      .wait_all = false,
      .multishot = false,
      .state = ret.create_state()
    });
    return ret;
  }

  context::read_chain context::queue_full_receive(id_t fid, size_t size, raw_data&& data, uint32_t offset_in_data)
  {
    check::debug::n_assert(size > 0, "Receives of size 0 are invalid");
    check::debug::n_assert(size != everything, "Full receive of `everything` are not possible. Please provide a valid size or use the non-full receive");

    if (data.data)
    {
      check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
      check::debug::n_assert(data.size - offset_in_data >= size, "Provided data does not have enough space for read (data size: {}, read-size: {})",
                              data.size - offset_in_data, size);
    }
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid receive operation");

    read_chain ret;
    recv_requests.add_request(
    {
      .fid = fid,
      .sock_fd = _get_fd(fid),
      .data = std::move(data),
      .offset_in_data = offset_in_data,
      .size_to_recv = size,
      .wait_all = true,
      .multishot = false,
      .state = ret.create_state()
    });
    return ret;
  }

  context::read_chain context::queue_multi_receive(id_t fid)
  {
    read_chain ret;
    recv_requests.add_request(
    {
      .fid = fid,
      .sock_fd = _get_fd(fid),
      .data = {},
      .offset_in_data = 0,
      .size_to_recv = 0,
      .wait_all = false,
      .multishot = true,
      .state = ret.create_state(true)
    });
    return ret;
  }

  context::write_chain context::queue_send(id_t fid, raw_data&& data, uint32_t offset_in_data, size_t size)
  {
    check::debug::n_assert(!!data.data, "Invalid data");
    check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid send operation");
    if (size == 0)
      size = data.size - offset_in_data;
    size = std::min((size_t)data.size - offset_in_data, size);

    write_chain ret;
    send_requests.add_request(
    {
      .fid = fid,
      .sock_fd = _get_fd(fid),
      .data = std::move(data),
      .offset_in_data = offset_in_data,
      .size_to_send = size,
      .wait_all = false,
      .state = ret.create_state()
    });
    return ret;
  }

  context::write_chain context::queue_full_send(id_t fid, raw_data&& data, uint32_t offset_in_data, size_t size)
  {
    check::debug::n_assert(!!data.data, "Invalid data");
    check::debug::n_assert(data.size > offset_in_data, "Invalid data / offset provided (data size: {}, offset: {})", data.size, offset_in_data);
    check::debug::n_check(fid != id_t::none && fid != id_t::invalid, "Invalid full-send operation");
    if (size == 0)
      size = data.size - offset_in_data;
    size = std::min((size_t)data.size - offset_in_data, size);

    write_chain ret;
    send_requests.add_request(
    {
      .fid = fid,
      .sock_fd = _get_fd(fid),
      .data = std::move(data),
      .offset_in_data = offset_in_data,
      .size_to_send = size,
      .wait_all = true,
      .state = ret.create_state()
    });
    return ret;
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

  id_t context::create_listening_socket(uint16_t port, const ipv6& ip, uint16_t backlog_connection_count, bool allow_ipv4)
  {
    const int sock = check::unx::n_check_success(socket(PF_INET6, SOCK_STREAM, 0));
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
    {
      int enable = allow_ipv4 ? 0 /* allow ipv4 */ : 1 /* only ipv6 */;
      check::unx::n_check_success(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(int)));
    }

    sockaddr_in6 srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin6_family = AF_INET6;
    srv_addr.sin6_port = htons(port);
    srv_addr.sin6_flowinfo = 0;
    srv_addr.sin6_scope_id = 0;
    srv_addr.sin6_scope_id = 0;
    memcpy(srv_addr.sin6_addr.s6_addr, ip.addr, sizeof(ip.addr));

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

  id_t context::create_socket(bool ipv6)
  {
    const int sock = check::unx::n_check_success(socket(ipv6 ? PF_INET6 : PF_INET, SOCK_STREAM, 0));
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
    if (pipe_fds[0] == -1 || pipe_fds[1] == -1)
    {
      check::debug::n_check(false, "invalid pipe FD");
      return false;
    }

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
      bool had_files = !opened_fd.empty();
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
      if (had_files && opened_fd.empty())
        cr::out().debug("io::context: all opened fd are now closed");
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
    queue_recv_operations();
    queue_send_operations();

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
      if (iovecs[i].iov_base != nullptr)
        raw_data::free_allocated_raw_memory((void*)((uint8_t*)iovecs[i].iov_base - *get_data_offset_for_iovec(i)));
      if (type == type_t::read || type == type_t::recv)
        read_states[i].~state();
      else if (type == type_t::write || type == type_t::send)
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
      case type_t::recv: [[fallthrough]];
      case type_t::read: callback_size = sizeof(read_chain::state); break;

      case type_t::send: [[fallthrough]];
      case type_t::write: callback_size = sizeof(write_chain::state); break;

      case type_t::accept: callback_size = sizeof(accept_chain::state); break;
      case type_t::connect: callback_size = sizeof(connect_chain::state); break;
    }
    const size_t offset_offset = sizeof(query) + sizeof(iovec) * iovec_count;
    const size_t unaligned_callback_offset = offset_offset + sizeof(unsigned) * iovec_count * 2;
    const size_t callback_offset = unaligned_callback_offset + (unaligned_callback_offset % 16 ? 16 - unaligned_callback_offset % 16 : 0);
    void* ptr = operator new(callback_offset + callback_size * std::max(1u, iovec_count));
    query* q = (query*)ptr;
    q->fid = fid;
    q->type = t;
    q->iovec_count = iovec_count;
    q->data_offet_array_offset = offset_offset;
    q->multishot = false;
    memset((uint8_t*)ptr + offset_offset, 0, sizeof(unsigned) * iovec_count * 2);
    if (t == type_t::read || t == type_t::recv)
    {
      q->read_states = (read_chain::state*)(((uint8_t*)ptr) + callback_offset);
      for (unsigned i = 0; i < iovec_count; ++i)
      {
        new (q->read_states + i) read_chain::state ();
      }
    }
    else if (t == type_t::write || t == type_t::send)
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

  unsigned* context::query::get_data_offset_for_iovec(unsigned index)
  {
    if (index >= iovec_count) return nullptr;
    return (uint32_t*)((uint8_t*)this + data_offet_array_offset + (index * 2 + 0) * sizeof(uint32_t));
  }
  unsigned* context::query::get_data_size_for_iovec(unsigned index)
  {
    if (index >= iovec_count) return nullptr;
    return (uint32_t*)((uint8_t*)this + data_offet_array_offset + (index * 2 + 1) * sizeof(uint32_t));
  }


  io_uring_sqe* context::get_sqe()
  {
    // check if we have an already "returned" sqe
    if (returned_sqe != nullptr)
    {
      io_uring_sqe* const sqe = returned_sqe;
      returned_sqe = nullptr;
      return sqe;
    }

    io_uring_sqe* const sqe = io_uring_get_sqe(&ring);
    if (sqe == nullptr)
    {
      // no space in queue, cannot do anything.
      neam::cr::out().debug("io::context::get_sqe: submit queue is full");
    }
    return sqe;
  }

  void context::return_sqe(io_uring_sqe* sqe)
  {
    check::debug::n_assert(returned_sqe == nullptr, "io::context: return sqe: a sqe has already been returned.");
    returned_sqe = sqe;
  }


  int context::open_file(id_t fid, bool read, bool write, bool truncate, bool force_truncate, bool& try_again)
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
        if (force_truncate)
          ftruncate(it->second.fd, 0);
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

      if (truncate || force_truncate)
        flags |= O_TRUNC;

//       const int fd = check::unx::n_check_code(open(it->second.c_str(), flags, 0644), "Failed to open {}", it->second.c_str());
      const int fd = open(it->second.c_str(), flags, 0644);
      if (offset != 0) // restore the offset
        lseek(fd, offset, SEEK_SET);
      neam::cr::out().debug("io::context::open_file: opening `{}` [read: {}, write: {}, fd: {}]", it->second.c_str(), read, write, fd);

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
        // just a debug 'cause it's a fire-and-forget query
        cr::out().debug("io::context::process_completed_queries: fire-and-forget query failed: {}: {}",
                        debug::errors::unix_errors::get_code_name(cqe->res), debug::errors::unix_errors::get_description(cqe->res));
      }
      else
      {
        cr::out().debug("io::context::process_completed_queries: {} query on fd {}: {}: {}",
                        get_query_type_str(data->type), get_string_for_id(data->fid),
                        debug::errors::unix_errors::get_code_name(cqe->res), debug::errors::unix_errors::get_description(cqe->res));
      }
    }

    const bool multishot_has_more = (cqe->flags & IORING_CQE_F_MORE) == IORING_CQE_F_MORE;
    const bool is_notif = (cqe->flags & IORING_CQE_F_NOTIF) == IORING_CQE_F_NOTIF;
    const bool is_using_buffer = (cqe->flags & IORING_CQE_F_BUFFER) == IORING_CQE_F_BUFFER;
    const uint16_t buffer_idx = is_using_buffer ? (uint32_t)(cqe->flags) >> 16 : 0;
#if 0 /* DEBUG */
    if (data)
      cr::out().debug("completed_queries: {} / {}: has-more: {}, notif: {}, buffer: {} [{}]", get_query_type_str(data->type), (void*)data, multishot_has_more, is_notif, is_using_buffer, buffer_idx);
    else
      cr::out().debug("completed_queries: [f-a-f]: has-more: {}, notif: {}, buffer: {} [{}]", multishot_has_more, is_notif, is_using_buffer, buffer_idx);
#endif
    // Queries that are pushed without data are 'fire and forget'queries where we don't care about the result, even if they failed
    // We also don't care about waiting for them (no state to complete)
    if (data != nullptr)
    {
      if (!is_notif)
      {
        switch (data->type)
        {
          case query::type_t::write: process_write_completion(*data, cqe->res >= 0, cqe->res);
            break;
          case query::type_t::read: process_read_completion(*data, cqe->res >= 0, cqe->res);
            break;
          case query::type_t::accept: process_accept_completion(*data, cqe->res >= 0, cqe->res);
            break;
          case query::type_t::connect: process_connect_completion(*data, cqe->res >= 0);
            break;
          case query::type_t::recv: process_recv_completion(*data, cqe->res >= 0, cqe->res, is_using_buffer, buffer_idx);
            break;
          case query::type_t::send: process_send_completion(*data, cqe->res >= 0, cqe->res);
            break;
        }
      }

      // cleanup:
      if (!multishot_has_more)
      {
        switch (data->type)
        {
          case query::type_t::write: write_requests.decrement_in_flight();
            break;
          case query::type_t::read: read_requests.decrement_in_flight();
            break;
          case query::type_t::accept: accept_requests.decrement_in_flight();
            break;
          case query::type_t::connect: connect_requests.decrement_in_flight();
            break;
          case query::type_t::recv: recv_requests.decrement_in_flight();
            break;
          case query::type_t::send: send_requests.decrement_in_flight();
            break;
        }
        data->~query();
        operator delete ((void*)data);
      }

      // re-allocate the lost buffer:
      if (is_using_buffer)
        allocate_buffers_for_multi_ops();
    }

    io_uring_cqe_seen(&ring, cqe);
  }

  bool context::queue_close_operations_fd()
  {
    std::lock_guard _fdl(fd_lock);
    while (fd_to_be_closed.size() > 0)
    {
      const int fd = *fd_to_be_closed.begin();
      // Get a SQE
      io_uring_sqe* sqe = get_sqe();
      if (!sqe)
        return false;

      fd_to_be_closed.erase(fd_to_be_closed.begin());

      io_uring_prep_close(sqe, fd);
      io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
    return true;
  }

  void context::cancel_operation(query& q)
  {
    std::lock_guard _fdl(cancel_lock);
    to_be_canceled.push_back({ .data = reinterpret_cast<uint64_t>(&q), .is_fd = false, });
  }

  void context::cancel_all_pending_operations_for(id_t eid)
  {
    const int fd = _get_fd(eid);
    if (fd < 0) return;

    std::lock_guard _fdl(cancel_lock);
    to_be_canceled.push_back({ .data = (uint64_t)fd, .is_fd = true, });
  }

  void context::queue_cancel_operations()
  {
    std::lock_guard _fdl(cancel_lock);
    while (to_be_canceled.size() > 0)
    {
      cancel_request rq = to_be_canceled.front();

      // Get a SQE
      io_uring_sqe* const sqe = get_sqe();
      if (!sqe)
        return;

      to_be_canceled.pop_front();

      if (rq.is_fd)
        io_uring_prep_cancel_fd(sqe, (int)rq.data, IORING_ASYNC_CANCEL_ALL);
      else
        io_uring_prep_cancel64(sqe, rq.data, IORING_ASYNC_CANCEL_ALL);

      io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);

      io_uring_sqe_set_data(sqe, nullptr);
      check::unx::n_check_success(io_uring_submit(&ring));
    }
  }

  void context::queue_readv_operations()
  {
    if (read_requests.requests.empty())
      return;
    std::deque<read_request> requests;
    {
      read_request rq;
      while (read_requests.requests.try_pop_front(rq))
        requests.emplace_back(std::move(rq));
    }

    {
      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(requests.begin(), requests.end(), [](const read_request& a, const read_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      while (requests.size() > 0)
      {
        if (requests.front().state.is_canceled())
        {
          requests.pop_front();
          continue;
        }

        const id_t fid = requests.front().fid;

        // check if the file is opened, else open it:
        bool try_again;
        const int fd = open_file(fid, true, false, false, false, try_again);
        if (fd < 0)
        {
          if (try_again)
            break;
          // fail all the queries for the same fid:
          while (!requests.empty() && fid == requests.front().fid)
          {
            requests.front().state.complete(std::move(requests.front().data), false, 0);
            requests.pop_front();
          }

          continue;
        }

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe();
        if (!sqe)
          break;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          constexpr size_t max_size_for_merging = 64 * 1024;
          size_t offset = requests.front().offset + requests.front().size;
          for (; iovec_count < requests.size() && iovec_count < k_max_iovec_merge; ++iovec_count)
          {
            if (fid != requests[iovec_count].fid)
              break;
            if (offset != requests[iovec_count].offset)
              break;
            if (offset + requests[iovec_count].size >= max_size_for_merging)
              break;
            offset += requests[iovec_count].size;
          }
        }

        // Allocate + fill the query structure:
        // FIXME: That would be nice to track this memory somewhere
        query* q = query::allocate(fid, query::type_t::read, iovec_count);
        const size_t offset = requests.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          q->iovecs[i].iov_len = requests.front().size;
          if (requests.front().data.size > 0)
          {
            q->iovecs[i].iov_base = (uint8_t*)requests.front().data.data.release() + requests.front().offset_in_data;
            *(q->get_data_offset_for_iovec(i)) = requests.front().offset_in_data;
            *(q->get_data_size_for_iovec(i)) = requests.front().data.size;
          }
          else
          {
            q->iovecs[i].iov_base = raw_data::allocate_raw_memory(requests.front().size);
          }
          q->read_states[i] = std::move(requests.front().state);

          memset(q->iovecs[i].iov_base, 0, requests.front().size); // so valgrind is happy, can be skipped

          requests.pop_front();
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

    // if we have remaining requests, push them back:
    for (auto& it : requests)
    {
      read_requests.add_request(std::move(it));
    }
  }

  void context::queue_writev_operations()
  {
    if (write_requests.requests.empty())
      return;
    std::deque<write_request> requests;
    {
      write_request rq;
      while (write_requests.requests.try_pop_front(rq))
        requests.emplace_back(std::move(rq));
    }
    {

      // sort reads by fid, so we have reads for the same file at the same place (better for readv)
      std::sort(requests.begin(), requests.end(), [](const write_request& a, const write_request& b)
      {
        if (a.fid == b.fid)
          return a.offset < b.offset;
        return std::to_underlying(a.fid) < std::to_underlying(b.fid);
      });

      while (requests.size() > 0)
      {
        if (requests.front().state.is_canceled())
        {
          requests.pop_front();
          continue;
        }

        const id_t fid = requests.front().fid;

        // check if the file is opened, else open it:
        bool try_again;
        const int fd = open_file(fid, false, true, requests.front().offset == 0, requests.front().offset == truncate, try_again);
        if (fd < 0)
        {
          if (try_again)
            break;
          // fail all the queries for the same fid:
          while (!requests.empty() && fid == requests.front().fid)
          {
            requests.front().state.complete(std::move(requests.front().data), false, 0);
            requests.pop_front();
          }

          continue;
        }

        // Get a SQE
        io_uring_sqe* const sqe = get_sqe();
        if (!sqe)
          break;

        // Count the queries for the same file w/ contiguous queries:
        unsigned iovec_count = 1;
        {
          const bool should_append = requests.front().offset == append;
          const bool should_truncate = requests.front().offset == truncate;
          size_t offset = requests.front().offset + requests.front().data.size;
          for (; !should_truncate && iovec_count < requests.size() && iovec_count < k_max_iovec_merge; ++iovec_count)
          {
            if (fid != requests[iovec_count].fid)
              break;
            if (!should_append)
            {
              if (offset != requests[iovec_count].offset)
                break;
              if (requests[iovec_count].offset == truncate)
                break;
              if (requests[iovec_count].offset == append)
                break;
              offset += requests[iovec_count].size_to_write;
            }
            else
            {
              if (requests[iovec_count].offset != append)
                break;
            }
          }
        }

        // Allocate + fill the query structure:
        query* q = query::allocate(fid, query::type_t::write, iovec_count);
        const size_t offset = requests.front().offset == truncate ? 0 : requests.front().offset;

        for (unsigned i = 0; i < iovec_count; ++i)
        {
          *(q->get_data_offset_for_iovec(i)) = requests.front().offset_in_data;
          *(q->get_data_size_for_iovec(i)) = requests.front().data.size;

          q->iovecs[i].iov_len = requests.front().size_to_write;
          q->iovecs[i].iov_base = (uint8_t*)requests.front().data.data.release() + requests.front().offset_in_data;
          q->write_states[i] = std::move(requests.front().state);
          requests.pop_front();
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

    // if we have remaining requests, push them back:
    for (auto& it : requests)
    {
      write_requests.add_request(std::move(it));
    }
  }

  void context::queue_accept_operations()
  {
    while (!accept_requests.requests.empty())
    {
      // Get a SQE
      io_uring_sqe* const sqe = get_sqe();
      if (!sqe)
        break;

      accept_request rq;
      if (!accept_requests.requests.try_pop_front(rq) || rq.state.is_canceled())
      {
        return_sqe(sqe);
        continue;
      }

      const id_t fid = rq.fid;
      const int fd = rq.sock_fd;

      // Allocate + fill the query structure:
      query* q = query::allocate(fid, query::type_t::accept, 0);

      *(q->accept_state) = std::move(rq.state);

      q->multishot = rq.multi_accept;

      if (rq.multi_accept)
        io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
      else
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
    while (!connect_requests.requests.empty())
    {
      // Get a SQE
      io_uring_sqe* const sqe = get_sqe();
      if (!sqe)
        break;

      connect_request rq;
      if (!connect_requests.requests.try_pop_front(rq) || rq.state.is_canceled())
      {
        return_sqe(sqe);
        continue;
      }

      const id_t fid = rq.fid;
      const int fd = rq.sock_fd;

      addrinfo* result;
      const auto port_str = fmt::format("{}", rq.port);
      if (check::unx::n_check_success(getaddrinfo(rq.addr.c_str(), port_str.c_str(), nullptr, &result)) < 0)
      {
        rq.state.complete(false);
        return_sqe(sqe);
        continue;
      }

      // Allocate + fill the query structure:
      query* q = query::allocate(fid, query::type_t::connect, 0);

      *(q->connect_state) = std::move(rq.state);

      io_uring_prep_connect(sqe, fd, result->ai_addr, result->ai_addrlen);

      // add the append flags if necessary
      io_uring_sqe_set_data(sqe, q);

      check::unx::n_check_success(io_uring_submit(&ring));
      ++connect_requests.in_flight;

      freeaddrinfo(result);

      q->connect_state->on_cancel([q, this]
      {
        cancel_operation(*q);
      });
    }
  }

  void context::queue_recv_operations()
  {
    // Allocate the buffers here, as we may (or may not) have a connection that require preallocated buffers
    if (!recv_requests.requests.empty())
      allocate_buffers_for_multi_ops();

    while (!recv_requests.requests.empty())
    {
      // Get a SQE
      io_uring_sqe* const sqe = get_sqe();
      if (!sqe)
        break;

      recv_request rq;
      if (!recv_requests.requests.try_pop_front(rq) || rq.state.is_canceled())
      {
        return_sqe(sqe);
        continue;
      }

      const id_t fid = rq.fid;
      const int fd = rq.sock_fd;

      const bool is_using_prealloc_buffer = (rq.multishot || (!rq.data.data && (!rq.wait_all && rq.size_to_recv == everything)));

      // Allocate + fill the query structure:
      query* q = query::allocate(fid, query::type_t::recv, 1);

      q->read_states[0] = std::move(rq.state);

      q->multishot = rq.multishot;

      if (!is_using_prealloc_buffer)
      {
        if (!rq.data.data)
        {
          rq.data = raw_data::allocate(rq.size_to_recv);
          rq.offset_in_data = 0;
          memset(rq.data.get(), 0, rq.size_to_recv); // so valgrind is happy, can be skipped
        }
        *(q->get_data_offset_for_iovec(0)) = rq.offset_in_data;
        *(q->get_data_size_for_iovec(0)) = rq.data.size;

        q->iovecs[0].iov_len = rq.size_to_recv;
        q->iovecs[0].iov_base = (uint8_t*)rq.data.data.release() + rq.offset_in_data;
      }
      else
      {
        q->iovecs[0].iov_base = nullptr;
        q->iovecs[0].iov_len = 0;
      }

      if (rq.multishot)
        io_uring_prep_recv_multishot(sqe, fd, nullptr, 0, 0);
      else
        io_uring_prep_recv(sqe, fd, q->iovecs[0].iov_base, q->iovecs[0].iov_len, rq.wait_all ? MSG_WAITALL : 0);

      cr::out().debug("queue_recv: multishot: {}, wait-all: {}, use-prealloc-buffers: {}", rq.multishot, rq.wait_all, is_using_prealloc_buffer);
      if (is_using_prealloc_buffer)
        io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);

      io_uring_sqe_set_data(sqe, q);

      check::unx::n_check_success(io_uring_submit(&ring));
      ++recv_requests.in_flight;

      q->read_states->on_cancel([q, this]
      {
        cancel_operation(*q);
      });
    }
  }

  void context::queue_send_operations()
  {
    while (!send_requests.requests.empty())
    {
      // Get a SQE
      io_uring_sqe* const sqe = get_sqe();
      if (!sqe)
        break;

      send_request rq;
      if (!send_requests.requests.try_pop_front(rq) || rq.state.is_canceled())
      {
        return_sqe(sqe);
        continue;
      }

      const id_t fid = rq.fid;
      const int fd = rq.sock_fd;

      // Allocate + fill the query structure:
      query* q = query::allocate(fid, query::type_t::send, 1);

      q->write_states[0] = std::move(rq.state);

      *(q->get_data_offset_for_iovec(0)) = rq.offset_in_data;
      *(q->get_data_size_for_iovec(0)) = rq.data.size;

      q->iovecs[0].iov_len = rq.size_to_send;
      q->iovecs[0].iov_base = (uint8_t*)rq.data.data.release() + rq.offset_in_data;

      const int flags = rq.wait_all ? MSG_WAITALL : 0;
      if (q->iovecs[0].iov_len < 2 * 1024 * 1024)
      {
        // we can do zero-copy, as we have ownership of the data during the write and we keep it alive
        // It seems performing big zero-copy send generate ENOMEM, failing the send. We only allow buffer less than 2Mib to perform zero copy sends.
        io_uring_prep_send_zc(sqe, fd, q->iovecs[0].iov_base, q->iovecs[0].iov_len, flags, 0);
      }
      else
      {
        io_uring_prep_send(sqe, fd, q->iovecs[0].iov_base, q->iovecs[0].iov_len, flags);
      }

      io_uring_sqe_set_data(sqe, q);

      check::unx::n_check_success(io_uring_submit(&ring));
      ++send_requests.in_flight;

      q->write_states->on_cancel([q, this]
      {
        cancel_operation(*q);
      });
    }
  }

  void context::process_deferred_operations()
  {
    deferred_request rq;
    while (deferred_requests.requests.try_pop_front(rq))
    {
      if (!rq.state.is_canceled())
        rq.state.complete();
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
      void* base_data = (uint8_t*)q.iovecs[i].iov_base - *(q.get_data_offset_for_iovec(i));
      raw_data data {raw_data::unique_ptr(base_data), *(q.get_data_size_for_iovec(i))};
      size_t write_size = std::min(sz, q.iovecs[i].iov_len);
      if (!success)
      {
        q.write_states[i].complete(std::move(data), false, 0);
      }
      else
      {
        q.write_states[i].complete(std::move(data), true, write_size);
      }

      sz -= write_size;
      // We have transfered the ownership to the callback, remove the pointer
      q.iovecs[i].iov_base = nullptr;
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
        q.read_states[i].complete({raw_data::unique_ptr(), 0}, false, 0);
      }
      else
      {
        // FIXME: Should be dispatched on other threads
        size_t read_size = std::min(sz, q.iovecs[i].iov_len);
        size_t data_len = *(q.get_data_size_for_iovec(i)) == 0 ? read_size : *(q.get_data_size_for_iovec(i));
        q.read_states[i].complete
        (
          {raw_data::unique_ptr((uint8_t*)q.iovecs[i].iov_base - *(q.get_data_offset_for_iovec(i))), data_len},
          true, (uint32_t)read_size
        );

        sz -= read_size;

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

  void context::process_recv_completion(query& q, bool success, size_t sz, bool is_using_buffer, uint16_t buffer_index)
  {
    raw_data data;
    if (!is_using_buffer)
    {
      if (!q.multishot)
      {
        void* base_data = (uint8_t*)q.iovecs[0].iov_base - *(q.get_data_offset_for_iovec(0));
        data = {raw_data::unique_ptr(base_data), *(q.get_data_size_for_iovec(0))};
        // We have transfered the ownership to the callback, remove the pointer
        q.iovecs[0].iov_base = nullptr;
      }
    }
    else
    {
      data = std::move(prealloc_multi_buffers[buffer_index]);
      data.size = sz;
    }

    q.read_states[0].complete(std::move(data), success, success ? sz : 0);
  }

  void context::process_send_completion(query& q, bool success, size_t sz)
  {
    void* base_data = (uint8_t*)q.iovecs[0].iov_base - *(q.get_data_offset_for_iovec(0));
    raw_data data {raw_data::unique_ptr(base_data), *(q.get_data_size_for_iovec(0))};
    // We have transfered the ownership to the callback, remove the pointer
    q.iovecs[0].iov_base = nullptr;

    q.write_states[0].complete(std::move(data), success, success ? sz : 0);
  }

  void context::allocate_buffers_for_multi_ops()
  {
    if (buffer_count < k_prealloc_buffer_count)
    {
      cr::out().debug("io::context: allocating {} buffers of {}Mib...", k_prealloc_buffer_count - buffer_count, k_prealloc_buffer_size / 1024 / 1024);
      while (buffer_count < k_prealloc_buffer_count)
      {
        io_uring_sqe* sqe = get_sqe();
        if (!sqe)
          return; // might be bad
        prealloc_multi_buffers[buffer_count] = raw_data::allocate(k_prealloc_buffer_size);
        memset(prealloc_multi_buffers[buffer_count].get(), 0, k_prealloc_buffer_size);

        io_uring_prep_provide_buffers(sqe, prealloc_multi_buffers[buffer_count].get(), k_prealloc_buffer_size, 1, 0, buffer_count);
        io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
        check::unx::n_check_success(io_uring_submit(&ring));

        ++buffer_count;
      }
    }
    else
    {
      for (uint32_t i = 0; i < k_prealloc_buffer_count; ++i)
      {
        if (prealloc_multi_buffers[i].data)
          continue;

        io_uring_sqe* sqe = get_sqe();
        if (!sqe)
          return; // might be bad
        prealloc_multi_buffers[i] = raw_data::allocate(k_prealloc_buffer_size);

        // for valgrind, could be skipped
        memset(prealloc_multi_buffers[i].get(), 0, k_prealloc_buffer_size);

        io_uring_prep_provide_buffers(sqe, prealloc_multi_buffers[i].get(), k_prealloc_buffer_size, 1, 0, i);
        io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
        check::unx::n_check_success(io_uring_submit(&ring));
      }
    }
  }

  const char* context::get_query_type_str(query::type_t t)
  {
    switch (t)
    {
      case query::type_t::read: return "read";
      case query::type_t::write: return "write";
      case query::type_t::accept: return "accept";
      case query::type_t::connect: return "connect";
      case query::type_t::recv: return "recv";
      case query::type_t::send: return "send";
    }
    return "unknown";
  }
}

