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

#include <filesystem>
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

#include "ip.hpp"

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
      static constexpr uint8_t k_max_cycle_to_close = 6;
      static constexpr size_t k_max_open_file_count = 384;

      // Necessary for queue_multi_receive. Only allocated on the first receive process.
      static constexpr size_t k_prealloc_buffer_count = 8;
      static constexpr size_t k_prealloc_buffer_size = 6 * 1024 * 1024;

    public:
      using read_chain = async::chain<raw_data&& /*data*/, bool /*success*/, size_t /*read_size*/>;
      using write_chain = async::chain<raw_data&& /*data*/, bool /*success*/, size_t /*write_size*/>;
      using connect_chain = async::chain<bool /*success*/>;
      using accept_chain = async::chain<id_t /* connection id (or invalid)*/>;

      static constexpr size_t whole_file = ~uint64_t(0);
      static constexpr size_t everything = whole_file;
      static constexpr size_t append = ~uint64_t(0); // for writes only, indicate we want to append
      static constexpr size_t truncate = append - 1; // for writes only, indicate we want to truncate

      explicit context(const unsigned _queue_depth = k_max_open_file_count);

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

    public: // general IO stuff
      id_t stdin();
      id_t stdout();
      id_t stderr();

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
      [[nodiscard]] id_t map_file(const std::string& path)
      {
        id_t fid = get_file_id(path);
        std::string filename = fmt::format("{}{}{}", prefix_directory, (prefix_directory.empty() ? "" : "/"), path);

        std::lock_guard<spinlock> _ml(mapped_lock);
        mapped_files.insert_or_assign(fid, std::move(filename));
        return fid;
      }

      [[nodiscard]] id_t map_unprefixed_file(std::string path)
      {
        id_t fid = get_file_id(path);

        std::lock_guard<spinlock> _ml(mapped_lock);
        mapped_files.insert_or_assign(fid, std::move(path));
        return fid;
      }

      static id_t get_file_id(const std::string& path)
      {
        return (id_t)((uint64_t)((id_t)string_id::_runtime_build_from_string(path.data(), path.size())) >> k_id_shift);
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
      /// \warning order of reads is not guaranteed unless reads are on contiguous chunks of data
      ///          (contiguous: offset + size = next_read_offset)
      ///          (in this case the order will be from the lowest offset to the highest one, not the submission order)
      [[nodiscard]] read_chain queue_read(id_t fid, size_t offset, size_t size);

      /// \brief queue a read operation using pre-existing data
      [[nodiscard]] read_chain queue_read(id_t fid, size_t offset, size_t size, raw_data&& data, uint32_t offset_in_data = 0);

      /// \brief queue a write operation
      /// \note an offset of 0 will truncate the file if it isn't already opened)
      /// \warning order of writes are not guranteed unless writes are on contiguous chunks of data
      ///          (contiguous: offset + data.size = next_write_offset)
      ///          (in this case the order will be from the lowest offset to the highest one, not the submission order)
      write_chain queue_write(id_t fid, size_t offset, raw_data&& data, uint32_t offset_in_data = 0, uint32_t size_to_write = 0);

      static constexpr size_t k_invalid_file_size = ~size_t(0);
      /// \brief returns on-disk size of the file
      [[nodiscard]] size_t get_file_size(id_t fid) const;

      /// \brief return the most recent of either the modified time or the created time
      /// (some copy utilities seems to keep the modified date but only update the created date)
      [[nodiscard]] std::filesystem::file_time_type get_modified_or_created_time(id_t fid) const;

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

      /// \brief Returns a string the indicates what the id is.
      /// It returns the filename for file-mapped ids, or a descriptive info for sockets, pipes, ...
      std::string get_string_for_id(id_t fid) const;

    public: // network stuff
      /// \brief 4 bytes to a uint representing an ipv4
      static constexpr uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      {
        return (uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d);
      }

      /// \brief Create a socket + call bind/listen on it. tcp/IPV4 version.
      [[nodiscard]] id_t create_listening_socket(uint16_t port = 0, uint32_t listen_addr = ipv4(0, 0, 0, 0) /*INADDR_ANY*/, uint16_t backlog_connection_count = 16);
      /// \brief Create a socket + call bind/listen on it. tcp/IPV6 version.
      [[nodiscard]] id_t create_listening_socket(uint16_t port = 0, const ipv6& ip = ipv6::any(), uint16_t backlog_connection_count = 16, bool allow_ipv4 = true);

      /// \brief Create a socket + (for use with queue_connect)
      [[nodiscard]] id_t create_socket(bool ipv6 = false);

      /// \brief connect to a host. Return whether the connect has succeeded or not.
      [[nodiscard]] connect_chain queue_connect(id_t fid, std::string host, uint32_t port, bool do_not_call_process = false)
      {
        connect_chain ret;
        const size_t pending_size = connect_requests.add_request({fid, _get_fd(fid), std::move(host), port, ret.create_state()});
        if (!is_called_on_multiple_threads && !do_not_call_process && pending_size >= k_max_pending_queue_size)
        {
          process();
        }
        return ret;
      }


      /// \brief Returns the socket's port or 0 if not valid
      /// Usefull when passing 0 to the create_listening_socket port
      [[nodiscard]] uint16_t get_socket_port(id_t sid) const;

      /// \brief Accept a connection.
      /// Return the new connection id (or invalid)
      [[nodiscard]] accept_chain queue_accept(id_t fid)
      {
        accept_chain ret;
        accept_requests.add_request({fid, _get_fd(fid), false, ret.create_state()});
        return ret;
      }

      /// \brief Accept multiple connections.
      /// Return the new connection id (or invalid)
      /// \note the completion chain will be triggered more than once, until it is cancelled or an error occurs
      [[nodiscard]] accept_chain queue_multi_accept(id_t fid)
      {
        accept_chain ret;
        accept_requests.add_request({fid, _get_fd(fid), true, ret.create_state(true)});
        return ret;
      }

      [[nodiscard]] read_chain queue_receive(id_t fid, size_t size, raw_data&& data = {}, uint32_t offset_in_data = 0);
      [[nodiscard]] read_chain queue_full_receive(id_t fid, size_t size, raw_data&& data = {}, uint32_t offset_in_data = 0);
      [[nodiscard]] read_chain queue_multi_receive(id_t fid);

      [[nodiscard]] write_chain queue_send(id_t fid, raw_data&& data, uint32_t offset_in_data = 0, size_t size = 0);
      [[nodiscard]] write_chain queue_full_send(id_t fid, raw_data&& data, uint32_t offset_in_data = 0, size_t size = 0);

    public: // misc stuff:
      /// \brief Create a pipe, with a read-end and a write-end
      /// \note if the return value is false, both read and write are unchanegd
      bool create_pipe(id_t& read, id_t& write);

      /// \brief Cancel all the pending operations on the given id.
      /// \note The id must be flagged as a non-automanaged id (pipe / socket / std{in,out,err})
      void cancel_all_pending_operations_for(id_t eid);

    public: // deferred operations (not async, only deferred. Can be canceled)
      async::continuation_chain _queue_deferred_operation()
      {
        async::continuation_chain ret;
        deferred_requests.add_request({ ret.create_state() });
        return ret;
      }

      /// \brief Queue a filesystem remove of the file
      async::chain<bool> queue_deferred_remove(id_t file_id)
      {
        return _queue_deferred_operation().then([=, this]
        {
          if (!is_file_mapped(file_id))
          {
            cr::out().debug("io::context: could not remove {}: file not mapped", get_string_for_id(file_id));
            return false;
          }
          std::error_code ec;
          cr::out().debug("io::context: removing {}", get_string_for_id(file_id));
          std::filesystem::remove(get_string_for_id(file_id), ec);
          return !!ec;
        });
      }

    public: // advanced fd handling:
      /// \brief Force a close operation on all the fd (sockets and files)
      void force_close_all_fd(bool include_sockets = false);

      /// \brief Close a fid. No further operation can be done on it.
      /// \note On sockets, this implies a shutdown call
      void close(id_t fid);
      void _unlocked_close(id_t fid);

      /// \brief return the underlying fd. Present here in the event stuff needs to be done on the socket itself that neam::io is not aware of
      int _get_fd(id_t fid) const;

    public: // query handling:
      /// \brief Only include in-flight operations: operations submit to liburing
      /// \see has_pending_operations
      /// \see get_in_flight_operations_count (slower)
      bool has_in_flight_operations() const
      {
        return write_requests.has_any_in_flight()
               || read_requests.has_any_in_flight()
               || accept_requests.has_any_in_flight()
               || connect_requests.has_any_in_flight()
               || recv_requests.has_any_in_flight()
               || send_requests.has_any_in_flight()
               || deferred_requests.has_any_in_flight()
               ;
      }

      /// \brief Return if the context has any pending operations (operations not yet sent to liburing)
      /// \see get_pending_operations_count (slower)
      bool has_pending_operations() const
      {
        return write_requests.has_any_pending()
               || read_requests.has_any_pending()
               || accept_requests.has_any_pending()
               || connect_requests.has_any_pending()
               || recv_requests.has_any_pending()
               || send_requests.has_any_pending()
               || deferred_requests.has_any_pending()
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

    public: // stats:
      /// \brief Return the total number of operations sent to liburing
      /// \note slower than has_in_flight_operations
      /// \see has_in_flight_operations
      unsigned get_in_flight_operations_count() const
      {
        return write_requests.get_in_flight_count()
               + read_requests.get_in_flight_count()
               + accept_requests.get_in_flight_count()
               + connect_requests.get_in_flight_count()
               + recv_requests.get_in_flight_count()
               + send_requests.get_in_flight_count()
               + deferred_requests.get_in_flight_count()
               ;
      }

      /// \brief Return the total number of operations queued but not sent yet to liburing
      /// \note slower than has_pending_operations
      /// \see has_pending_operations
      unsigned get_pending_operations_count() const
      {
        return write_requests.get_in_queued_count()
               + read_requests.get_in_queued_count()
               + accept_requests.get_in_queued_count()
               + connect_requests.get_in_queued_count()
               + recv_requests.get_in_queued_count()
               + send_requests.get_in_queued_count()
               + deferred_requests.get_in_queued_count()
               ;
      }

      uint64_t get_total_written_bytes() const { return stats_total_written_bytes.load(std::memory_order_relaxed); }
      uint64_t get_total_read_bytes() const { return stats_total_read_bytes.load(std::memory_order_relaxed); }

      uint32_t get_opened_file_descriptors() const { return opened_fd.size(); }
      bool has_too_many_file_descriptors() const { return get_opened_file_descriptors() >= k_max_open_file_count; }

    private: // data structure:
      struct read_request
      {
        id_t fid;
        size_t offset;
        size_t size;

        raw_data data;
        uint32_t offset_in_data;

        read_chain::state state;
      };

      struct write_request
      {
        id_t fid;
        size_t offset;
        raw_data data;
        uint32_t offset_in_data;
        uint32_t size_to_write;

        write_chain::state state;
      };

      struct accept_request
      {
        id_t fid;
        int sock_fd;

        bool multi_accept;

        accept_chain::state state;
      };

      struct connect_request
      {
        id_t fid;
        int sock_fd;
        std::string addr;
        uint32_t port;

        connect_chain::state state;
      };

      struct recv_request
      {
        id_t fid;
        int sock_fd;
        raw_data data;
        uint32_t offset_in_data;
        size_t size_to_recv;

        bool wait_all;
        bool multishot;

        read_chain::state state;
      };

      struct send_request
      {
        id_t fid;
        int sock_fd;
        raw_data data;
        uint32_t offset_in_data;
        size_t size_to_send;

        bool wait_all;

        write_chain::state state;
      };

      struct deferred_request
      {
        async::continuation_chain::state state;
      };
      struct cancel_request
      {
        uint64_t data;
        bool is_fd;
      };

      template<typename RequestType>
      struct request
      {
        cr::queue_ts<cr::queue_ts_wrapper<RequestType>> requests;
        std::atomic<unsigned> in_flight = 0;

        size_t add_request(RequestType&& rq)
        {
          requests.push_back(std::move(rq));
          return requests.size();
        }

        bool has_any_in_flight() const
        {
          return in_flight.load(std::memory_order_acquire);
        }
        bool has_any_pending() const
        {
          return !requests.empty();
        }
        void decrement_in_flight()
        {
          in_flight.fetch_sub(1, std::memory_order_release);
        }

        unsigned get_in_flight_count() const
        {
          return in_flight.load(std::memory_order_acquire);
        }
        unsigned get_in_queued_count() const
        {
          return (unsigned)requests.size();
        }
      };

      // user data sent to io-uring
      struct query
      {
        enum class type_t : uint8_t
        {
          // generic:
          read,
          write,

          // network:
          accept,
          connect,
          recv,
          send,
        };

        id_t fid;
        type_t type;

        bool multishot : 1;

        unsigned iovec_count;
        unsigned data_offet_array_offset;
        union
        {
          read_chain::state* read_states;
          write_chain::state* write_states;
          accept_chain::state* accept_state;
          connect_chain::state* connect_state;
        };
        iovec iovecs[];

        unsigned* get_data_offset_for_iovec(unsigned index);
        unsigned* get_data_size_for_iovec(unsigned index);

        ~query();

        static query* allocate(id_t fid, type_t t, unsigned iovec_count);
      };

      static constexpr uint64_t k_external_id_flag = 0x8000000000000000;
      static constexpr uint64_t k_id_shift = 1;
      struct alignas(id_t) file_descriptor
      {
        int fd;

        uint8_t counter = 0;

        // type:
        bool socket: 1 = false;
        bool pipe: 1 = false;
        bool file: 1 = false;

        // capabilities:
        bool read: 1 = false;
        bool write: 1 = false;
        bool accept: 1 = false; // cannot read or write
      };
      static_assert(sizeof(file_descriptor) == sizeof(id_t));
      static_assert(alignof(file_descriptor) == alignof(id_t));

    private: // functions:
      io_uring_sqe* get_sqe();
      void return_sqe(io_uring_sqe* sqe);

      int open_file(id_t fid, bool read, bool write, bool truncate, bool force_truncate, bool& try_again);

      id_t register_fd(file_descriptor fd, bool skip_if_already_registered = false);
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
      void queue_recv_operations();
      void queue_send_operations();

      void process_deferred_operations();


      void process_write_completion(query& q, bool success, size_t sz);
      void process_read_completion(query& q, bool success, size_t sz);
      void process_accept_completion(query& q, bool success, int ret);
      void process_connect_completion(query& q, bool success);
      void process_recv_completion(query& q, bool success, size_t ret, bool is_using_buffer, uint16_t buffer_index);
      void process_send_completion(query& q, bool success, size_t ret);

      static const char* get_query_type_str(query::type_t t);

      bool stat_file(id_t fid, struct stat& st) const;

      // NOTE: Must be called during a process or queue
      void allocate_buffers_for_multi_ops();

    private: // members:
      unsigned queue_depth;
      io_uring ring;
      io_uring_sqe* returned_sqe = nullptr;

      std::string prefix_directory;

      // avoid re-entering in stuff we should never re-enter:
      spinlock process_lock;
      spinlock completion_lock;


      mutable spinlock mapped_lock;
      std::unordered_map<id_t, std::string> mapped_files;


      mutable spinlock fd_lock;
      std::unordered_map<id_t, file_descriptor> opened_fd;
      std::unordered_set<int> fd_to_be_closed; // list of fd to be closed. fd must not be present in opened_fd.
      mutable spinlock cancel_lock;
      std::deque<cancel_request> to_be_canceled; // list of operations to cancel

      request<read_request> read_requests;
      request<write_request> write_requests;
      request<accept_request> accept_requests;
      request<connect_request> connect_requests;
      request<recv_request> recv_requests;
      request<send_request> send_requests;
      request<deferred_request> deferred_requests;

      raw_data prealloc_multi_buffers[k_prealloc_buffer_count];
      uint32_t buffer_count = 0;

      std::atomic<uint64_t> stats_total_read_bytes = 0;
      std::atomic<uint64_t> stats_total_written_bytes = 0;

      static constexpr unsigned k_max_pending_queue_size = 20; // above this, it will trigger a process call()

      bool is_called_on_multiple_threads = true;

#if N_ASYNC_USE_TASK_MANAGER
      threading::task_manager* task_manager = nullptr;
      threading::group_t group_id = threading::k_invalid_task_group;
#endif
  };
}
