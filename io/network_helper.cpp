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


#include "network_helper.hpp"


namespace neam::io::network
{
  void connection_t::close()
  {
    if (socket == id_t::none)
      return;

    ioctx->cancel_all_pending_operations_for(socket);
    ioctx->close(socket);
    socket = id_t::none;

    if (server_base != nullptr)
    {
      server_base->move_to_ended_connections(*this);
      in_flight_operations.set_callback([this]
      {
        server_base->remove_from_ended_connections(*this);
      });

      // avoid race conditions:
      if (in_flight_operations.get_count() == 0)
        server_base->remove_from_ended_connections(*this);
    }

    on_close();
  }

  context::write_chain connection_t::queue_send(raw_data&& data, uint32_t offset_in_data)
  {
    return ioctx->queue_send(socket, std::move(data), offset_in_data)
    .then([this](raw_data&& rd, bool success, uint32_t read_size)
    {
      if (!success || read_size == 0)
      {
        close();
        return context::write_chain::create_and_complete({}, false, 0);
      }

      return context::write_chain::create_and_complete(std::move(rd), true, read_size);
    });
  }
  context::write_chain connection_t::queue_full_send(raw_data&& data, uint32_t offset_in_data)
  {
    return ioctx->queue_full_send(socket, std::move(data), offset_in_data)
    .then([this](raw_data&& rd, bool success, uint32_t read_size)
    {
      if (!success || read_size == 0)
      {
        close();
        return context::write_chain::create_and_complete({}, false, 0);
      }

      return context::write_chain::create_and_complete(std::move(rd), true, read_size);
    });
  }

  context::read_chain connection_t::queue_receive(size_t size, raw_data&& data, uint32_t offset_in_data)
  {
    return ioctx->queue_receive(socket, size, std::move(data), offset_in_data)
    .then([this](raw_data&& rd, bool success, uint32_t read_size)
    {
      if (!success || read_size == 0)
      {
        close();
        return context::read_chain::create_and_complete({}, false, 0);
      }

      return context::read_chain::create_and_complete(std::move(rd), true, read_size);
    });
  }
  context::read_chain connection_t::queue_multi_receive()
  {
    return ioctx->queue_multi_receive(socket)
    .then([this](raw_data&& rd, bool success, uint32_t read_size)
    {
      if (!success || read_size == 0)
      {
        close();
        return context::read_chain::create_and_complete({}, false, 0);
      }

      return context::read_chain::create_and_complete(std::move(rd), true, read_size);
    });
  }
  context::read_chain connection_t::queue_full_receive(size_t size, raw_data&& data, uint32_t offset_in_data)
  {
    return ioctx->queue_full_receive(socket, size, std::move(data), offset_in_data)
    .then([this](raw_data&& rd, bool success, uint32_t read_size)
    {
      if (!success || read_size == 0)
      {
        close();
        return context::read_chain::create_and_complete({}, false, 0);
      }

      return context::read_chain::create_and_complete(std::move(rd), true, read_size);
    });
  }

  void base_server_interface::async_accept()
  {
    ioctx.queue_multi_accept(listen_socket).then([this](neam::id_t connection)
    {
      if (connection != neam::id_t::invalid)
      {
        // Prevent opening too many connections
        // This makes the server very vulnerable to DDoS, but in case of an engine,
        // this allows potentially necessary access to files to still take place
        if (ioctx.has_too_many_file_descriptors() || (uint32_t)active_connections.size() >= max_connection_count)
        {
          ioctx.close(connection);
          return;
        }

        {
          std::unique_ptr<connection_t> ptr = on_connection({this, &ioctx, connection});
          if (ptr && !ptr->is_closed())
          {
            std::lock_guard _lg(lock);
            active_connections.emplace(ptr.get(), std::move(ptr));
          }
        }
      }
      else
      {
        // Accept failed, we stop the server.
        close_listening_socket();
      }
    });
  }

  void base_server_interface::close_listening_socket()
  {
    ioctx.cancel_all_pending_operations_for(listen_socket);
    ioctx.close(listen_socket);
    listen_socket = id_t::none;
  }

  void base_server_interface::close_all_connections()
  {
    for_each_connection([](connection_t& c, cr::token_counter::ref&&) { c.close(); });
  }

  void base_server_interface::move_to_ended_connections(connection_t& connection)
  {
    std::lock_guard _lg(lock);
    if (auto it = active_connections.find(&connection); it != active_connections.end())
    {
      auto ret = std::move(it->second);
      active_connections.erase(it);
      ended_connections.emplace(ret.get(), std::move(ret));
    }
  }

  void base_server_interface::remove_from_ended_connections(connection_t& connection)
  {
    std::lock_guard _lg(lock);
    // avoid race conditions (like in a for-each)
    if (connection.in_flight_operations.get_count() == 0)
      ended_connections.erase(&connection);
  }
}

