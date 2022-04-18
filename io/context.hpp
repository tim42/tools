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
#include "../raw_memory_pool_ts.hpp"
#include "../spinlock.hpp"

namespace neam::io
{
  /// \brief manage async file access/network. Mostly optimized for reads.
  /// \warning only one thread should ever call process, process_completed_queries, and _wait_for_submit_queries
  ///
  /// FIXME: The flag handling is a mess.
  class context
  {
    private:
      // Max number of queries that can be merged together.
      // A too high value will cause the OS to outright reject the query.
      static constexpr unsigned k_max_iovec_merge = IOV_MAX;
      static constexpr uint8_t k_max_cycle_to_close = 64;

    public:
      using read_chain = async::chain<raw_data&& /*data*/, bool /*success*/>;
      using write_chain = async::chain<bool /*success*/>;
      using connect_chain = async::chain<bool /*success*/>;
      using accept_chain = async::chain<id_t /* connection id (or invald)*/>;

      static constexpr size_t whole_file = ~uint64_t(0);
      static constexpr size_t append = ~uint64_t(0); // for writes only, indicate we want to append

      explicit context(const unsigned _queue_depth = 32);

      ~context();

      /// \brief Tweak some of the behavior to avoid bad cases in multi-threaded contexts.
      /// \note if true, process() will have to be manually called (see the warning on the class comment)
      /// The default is true.
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
        is_used_across_threads(true);
        task_manager = tm;
        group_id = id;
      }
#endif

    public: // file stuff
      const std::string& get_prefix_directory() const { return prefix_directory; }
      void set_prefix_directory(std::string prefix)
      {
        // prefix change means conflicts, we force close all opened files:
        force_close_all_fd(false);

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
        const size_t pending_size = read_requests.add_request({fid, offset, size, ret.create_state()});
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
        const size_t pending_size = write_requests.add_request({fid, offset, std::move(data), ret.create_state()});

        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }

      /// \brief returns on-disk size of the file
      size_t get_file_size(id_t fid);

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

    public: // network stuff
      /// \brief 4 bytes to a uint representing an ipv4
      static constexpr uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      {
        return (uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d);
      }

      /// \brief Create a socket + call bind/listen on it.
      id_t create_listening_socket(uint16_t port = 0, uint32_t listen_addr = ipv4(0, 0, 0, 0) /*INADDR_ANY*/, uint16_t backlog_connection_count = 16);

      /// \brief Create a socket + (for use with queue_connect)
      id_t create_socket();

      /// \brief connect to a host. Return whether the connect has succeeded or not.
      connect_chain queue_connect(id_t fid, std::string host, bool do_not_call_process = false)
      {
        connect_chain ret;
        const size_t pending_size = connect_requests.add_request({fid, _get_fd(fid), std::move(host), ret.create_state()});
        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }


      /// \brief Returns the socket's port or 0 if not valid
      /// Usefull when passing 0 to the create_listening_socket port
      uint16_t get_socket_port(id_t sid) const;

      /// \brief Accept a connection.
      /// Return the new connection id (or invalid)
      accept_chain queue_accept(id_t fid, bool do_not_call_process = false)
      {
        accept_chain ret;
        const size_t pending_size = accept_requests.add_request({fid, _get_fd(fid), ret.create_state()});
        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }


    public: // advanced fd handling:
      /// \brief Force a close operation on all the fd (sockets and files)
      void force_close_all_fd(bool include_sockets = false);

      /// \brief Close a socket. No further operation can be done on it.
      void close(id_t fid);

      /// \brief return the underlying fd. Present here in the event stuff needs to be done on the socket itself that neam::io is not aware of
      int _get_fd(id_t fid) const;

    public: // query handling:
      /// \brief Only include in-flight operations: operations submit to liburing
      /// \see has_pending_operations (
      bool has_in_flight_operations() const
      {
        return write_requests.has_any_in_flight()
               || read_requests.has_any_in_flight()
               || accept_requests.has_any_in_flight()
               || connect_requests.has_any_in_flight()
               ;
      }

      /// \brief
      bool has_pending_operations() const
      {
        return write_requests.has_any_pending()
               || read_requests.has_any_pending()
               || accept_requests.has_any_pending()
               || connect_requests.has_any_pending()
               ;
      }

      /// \brief process the queue
      void process();

      /// \brief Only process completed queries
      /// \note Do not do any waiting
      void process_completed_queries();

      /// \brief Very much like process and process_completed_queries, but is instead blocking until _everything_ is done
      /// \note deafeats the purpose of using asynchronous IO.
      ///       Should only be used in contexts where synchronicity is required.
      ///       (like application destruction, where nothing else can be done, ...)
      /// \note Please use process_completed_queries when possible, and when not possible please make it possible
      ///       This is kind of a last-resort function
      void _wait_for_submit_queries(bool wait_for_everything = true);

      /// \warning stall until there's something to do.
      void _wait_for_queries();

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

      struct accept_request
      {
        id_t fid;
        int sock_fd;

        accept_chain::state state;
      };

      struct connect_request
      {
        id_t fid;
        int sock_fd;
        std::string addr;

        connect_chain::state state;
      };

      template<typename RequestType>
      struct request
      {
        mutable spinlock lock;
        std::deque<RequestType> requests;
        unsigned in_flight = 0;

        size_t add_request(RequestType&& rq)
        {
          std::lock_guard _l(lock);
          requests.emplace_back(std::move(rq));
          return requests.size();
        }

        bool has_any_in_flight() const
        {
          std::lock_guard _l(lock);
          return in_flight != 0;
        }
        bool has_any_pending() const
        {
          std::lock_guard _l(lock);
          return !requests.empty();
        }
        void decrement_in_flight()
        {
          std::lock_guard _l(lock);
          in_flight -= 1;
        }
      };

      // user data sent to io-uring
      struct query
      {
        enum class type_t : uint8_t
        {
          // generic:
          close,
          read,
          write,

          // network:
          accept,
          connect,
        };

        id_t fid;
        type_t type;

        bool is_canceled;

        unsigned iovec_count;
        union
        {
          read_chain::state* read_states;
          write_chain::state* write_states;
          accept_chain::state* accept_state;
          connect_chain::state* connect_state;
        };
        iovec iovecs[];


        ~query();

        static query* allocate(id_t fid, type_t t, unsigned iovec_count);
      };

      struct file_descriptor
      {
        int fd;

        uint8_t counter = 0;

        // type: (should be.. a 1bit enum ?)
        bool socket: 1;
        bool file: 1;

        // capabilities:
        bool read: 1;
        bool write: 1;
        bool accept: 1; // cannot read or write
      };

    private: // functions:
      io_uring_sqe* get_sqe(bool should_process);

      int open_file(id_t fid, bool read, bool write, bool truncate);

      id_t register_socket(int fd, bool accept);

      void process_completed_query(io_uring_cqe* cqe);


      bool queue_close_operations_fd();
      bool queue_close_operations_id();

      void cancel_operation(query& q);
      void queue_cancel_operations();

      void queue_readv_operations();
      void queue_writev_operations();
      void queue_accept_operations();
      void queue_connect_operations();


      void process_write_completion(query& q, bool success);
      void process_read_completion(query& q, bool success, size_t sz);
      void process_accept_completion(query& q, bool success, int ret);
      void process_connect_completion(query& q, bool success);

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


      mutable spinlock fd_lock;
      std::unordered_map<id_t, file_descriptor> opened_fd;
      std::unordered_set<int> fd_to_be_closed; // list of fd to be closed. fd must not be present in opened_fd.
      std::deque<query*> to_be_canceled; // list of operations to cancel

      request<read_request> read_requests;
      request<write_request> write_requests;
      request<accept_request> accept_requests;
      request<connect_request> connect_requests;


      static constexpr unsigned k_max_pending_queue_size = 20; // above this, it will trigger a process call()

      bool is_called_on_multiple_threads = true;

#if N_ASYNC_USE_TASK_MANAGER
      threading::task_manager* task_manager = nullptr;
      threading::group_t group_id = threading::k_invalid_task_group;
#endif
  };
}
