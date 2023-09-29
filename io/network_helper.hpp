//
// created by : Timothée Feuillet
// date: 2023-9-21
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

#include <map>
#include "../id/id.hpp"
#include "../token_counting.hpp"
#include "io.hpp"

#include "../event.hpp"
namespace neam::io::network
{
  template<typename ConnectionType, typename ContextType = void>
  class base_server;

  /// \brief Represent a connection. Can / should be inherited to add more context/data to the connection.
  struct connection_t
  {
    class base_server_interface* server_base = nullptr;
    context* ioctx = nullptr;
    id_t socket = id_t::none;
    cr::token_counter in_flight_operations {}; // prevent destruction as long as there are in-flight operations

    bool is_closed() const { return socket == id_t::none; }

    /// \brief Close the connection
    /// \return the unique ptr of the connection (in the case of a server-owned connection)
    void close();

    /// \brief Called _after_ the socket has been closed and all requests cancelled
    cr::event<> on_close;

    /// \brief Helper for sending data through the connection
    context::write_chain queue_send(raw_data&& data, uint32_t offset_in_data = 0);
    context::write_chain queue_full_send(raw_data&& data, uint32_t offset_in_data = 0);

    /// \brief Queue a receive operation of a given size
    /// \note handle connection closing automatically
    /// \note queuing the next read from inside the chain is the duty of the caller
    context::read_chain queue_receive(size_t size, raw_data&& data = {}, uint32_t offset_in_data = 0);
    context::read_chain queue_multi_receive();
    context::read_chain queue_full_receive(size_t size, raw_data&& data = {}, uint32_t offset_in_data = 0);

    /// \brief Init a connection from... a connection (not from a server)
    async::chain<bool, cr::token_counter::ref&&> queue_connect(std::string host, uint32_t port, bool use_ipv6 = false)
    {
      // TODO: state check
      socket = ioctx->create_socket(use_ipv6);
      return ioctx->queue_connect(socket, std::move(host), port).then([this, tk = in_flight_operations.get_token()](bool success) mutable
      {
        if (!success)
        {
          ioctx->close(socket);
          socket = id_t::none;
        }

        return async::chain<bool, cr::token_counter::ref&&>::create_and_complete(success, std::move(tk));
      });
    }

  };

  /// \brief Handle most of the boilerplate of setting up and maintaining a server using neam::io
  /// Also handle connections and their status
  /// On sockets: accept facilities (automatic accept loop + filter callback)
  /// On connections: provide read facilities (automatic reading loop) and connection handling
  class base_server_interface
  {
    public:
      base_server_interface(io::context& _ioctx, uint32_t _max_connections = 32)
        : ioctx(_ioctx), max_connection_count(_max_connections)
      {}
      virtual ~base_server_interface() = default;

      void set_socket(id_t socket) { listen_socket = socket; }
      bool is_socket_valid() const { return listen_socket != id_t::none; }
      /// \brief Start the async accept loop
      void async_accept();

      /// \brief Close the listening socket + stop the accept loop (potentially cancel the last queued accept operation)
      /// \note The open connections are kept open and alive, and can still be used
      void close_listening_socket();

      /// \brief Return the status of the listening socket
      bool is_listening_socket_closed() const { return listen_socket == id_t::none; }

      /// \brief Forcefully close all the open / active connections
      void close_all_connections();

      bool has_any_connections() const { return !active_connections.empty(); }
      size_t get_connection_count() const { return active_connections.size(); }

    public:
      /// \brief Run something for all connections
      template<typename Func>
      void for_each_connection(Func&& fnc)
      {
        std::lock_guard _lg(lock);
        for (auto it = active_connections.begin(); it != active_connections.end();)
        {
          connection_t* ptr = nullptr;
          {
            // grab a token, so we don't get the task deleted under us
            // (this guarantees that the _pointer_ will stay valid for the call, not the iterator).
            auto token = it->second->in_flight_operations.get_token();
            ptr = it->first;
            lock.unlock();
            fnc(*ptr, std::move(token));
          }

          // we re-lock _after_ this scope, so the token can go out-of-scope without the lock held
          // If the operation was a call to close(), the instance is then deleted (we avoid the deadlock by not holding the lock).
          // We relock and assume the pointer is dead.
          lock.lock();

          // We have to perform a search every time, but this avoids complications
          it = active_connections.upper_bound(ptr);
        }
      }

    protected:
      /// \brief Handle connections. If returning nullptr, the connection has been either closed or should not be tracked by this class.
      virtual std::unique_ptr<connection_t> on_connection(connection_t&& connection) = 0;

      void move_to_ended_connections(connection_t& connection);
      void remove_from_ended_connections(connection_t& connection);

    protected:
      io::context& ioctx;
      uint32_t max_connection_count = 32;

      id_t listen_socket = id_t::none;

      spinlock lock;
      std::map<connection_t*, std::unique_ptr<connection_t>> active_connections;
      std::map<connection_t*, std::unique_ptr<connection_t>> ended_connections;

      friend connection_t;
  };

  /// \brief Handles some of the boilerplate of running the server.
  /// ConnectionType must be:
  ///  - constructible from connection_t&& + ContextType&
  ///  - have a \code static bool on_connection(std::unique_ptr<ConnectionType>&&) \endcode function
  /// \see struct buffered_connection_t
  template<typename ConnectionType, typename ContextType>
  class base_server : public base_server_interface
  {
    public:
      base_server(std::conditional_t<std::is_same_v<void, ContextType>, int, ContextType> _ctx, io::context& _ioctx, uint32_t _max_connections = 32)
      requires(!std::is_same_v<void, ContextType>)
        : base_server_interface(_ioctx, _max_connections), ctx(_ctx) {}
      base_server(io::context& _ioctx, uint32_t _max_connections = 32) requires(std::is_same_v<void, ContextType>)
        : base_server_interface(_ioctx, _max_connections) {}

      static std::unique_ptr<ConnectionType> create_connection(connection_t&& connection, std::conditional_t<std::is_same_v<void, ContextType>, int, ContextType> ctx = {})
      {
        std::unique_ptr<ConnectionType> connection_uptr;

        if constexpr (!std::is_same_v<void, ContextType>)
          connection_uptr = std::make_unique<ConnectionType>(ctx);
        else // ContextType is void:
          connection_uptr = std::make_unique<ConnectionType>();

        connection_uptr->server_base = connection.server_base;
        connection_uptr->ioctx = connection.ioctx;
        connection_uptr->socket = connection.socket;

        if (ConnectionType::on_connection(*connection_uptr))
        {
          return connection_uptr;
        }
        return {};
      }

      std::unique_ptr<connection_t> on_connection(connection_t&& connection) override
      {
        return create_connection(std::move(connection), ctx);
      }

    private:
      std::conditional_t<std::is_same_v<void, ContextType>, int, ContextType> ctx;
  };
}

