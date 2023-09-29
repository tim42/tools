//
// created by : Timothée Feuillet
// date: 2023-9-22
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

#include "network_helper.hpp"

namespace neam::io::network
{
  /// \brief Handle a buffered connection, where data is to be put in a ring-buffer until some kind of event
  /// \note the max memory is allocated. Please multiply this number with the max number of connection to see if it is a risk
  template<typename Child, size_t MaxRingBufferSize = 1024>
  struct ring_buffer_connection_t : public connection_t
  {
    cr::ring_buffer<uint8_t, MaxRingBufferSize> read_buffer;

    /// \brief Called when the connection has been completly setup
    /// (default behavior, overridable in the child class)
    void on_connection_setup() {}

    /// \brief What to do when the buffer is full
    /// (default behavior, overridable in the child class)
    void on_buffer_full() { close(); }

    /// \brief What to do when receiving data
    /// (default behavior, overridable in the child class)
    void on_read(uint32_t start_offset, uint32_t size) {}



    /// \brief Start the async read loop
    /// automatically called when the connection is initiated
    void async_read(cr::token_counter::ref&& tk)
    {
      queue_multi_receive().then([this, tk = std::move(tk)](raw_data&& rd, bool success, uint32_t read_size) mutable
      {
        if (!success)
          return;

        const uint32_t old_offset = read_buffer.size();
        size_t inserted = read_buffer.push_back((uint8_t*)rd.get(), rd.size);

        uint32_t size_before_on_read = read_buffer.size();
        static_cast<Child*>(this)->on_read(old_offset, (uint32_t)inserted);
        uint32_t size_after_on_read = read_buffer.size();

        if (is_closed()) return;

        // Handle the case where we could not insert everything _but_ the on-read made enough space to for part of another entry
        // (Can happen, in case ReadSize is bigger than MaxBufferSize or when there's an entry close to MaxBufferSize)
        while (inserted != rd.size)
        {
          if (size_before_on_read != size_after_on_read)
          {
            const uint32_t old_offset = read_buffer.size();

            size_t new_inserted = read_buffer.push_back((uint8_t*)rd.get(), rd.size);

            size_before_on_read = read_buffer.size();
            static_cast<Child*>(this)->on_read(old_offset, (uint32_t)new_inserted);
            size_after_on_read = read_buffer.size();

            inserted += new_inserted;
          }
          else
          {
            static_cast<Child*>(this)->on_buffer_full();
            break;
          }
          if (is_closed()) return;
        }
      });
    }

    static bool on_connection(Child& chld)
    {
      chld.async_read(chld.in_flight_operations.get_token());
      chld.on_connection_setup();
      return true;
    }
  };


  /// \brief Handle connections that are driven by headers (that can infer packet size)
  /// \note Doesn't support parallel/interleaved sends
  template<typename Child, size_t MaxDataSize = 1024 * 1024>
  struct header_connection_t : public connection_t
  {
    static constexpr uint32_t get_header_size() { return sizeof(typename Child::packet_header_t); }

    // Child API:

    /// \brief Called when the connection has been completly setup
    /// (default behavior, overridable in the child class)
    void on_connection_setup() {}

    // bool is_header_valid(const packet_header_t& ph) { return true; }
    // uint32_t get_size_of_data_to_read(const packet_header_t& ph) { return ph.size; }
    // void on_packet(const packet_header_t& ph, raw_data&& packet_data)
    // void on_packet_oversized(const packet_header_t& ph) {}


    void read_packet_header(cr::token_counter::ref&& tk)
    {
      if (is_closed())
        return;
      queue_full_receive(get_header_size()).then([this, tk = std::move(tk)](raw_data&& rd, bool success, uint32_t) mutable
      {
        if (!success)
          return;

        using packet_header_t = typename Child::packet_header_t;

        const packet_header_t header = *reinterpret_cast<const packet_header_t*>(rd.get());
        if (!static_cast<Child*>(this)->is_header_valid(header))
        {
          close();
          return;
        }
        const uint32_t read_size = static_cast<Child*>(this)->get_size_of_data_to_read(header);
        if (read_size > MaxDataSize)
        {
          static_cast<Child*>(this)->on_packet_oversized(header);
          close();
          return;
        }

        read_packet_data(std::move(tk), header);
      });
    }

    template<typename Header>
    void read_packet_data(cr::token_counter::ref&& tk, const Header& header)
    {
      queue_full_receive(static_cast<Child*>(this)->get_size_of_data_to_read(header))
      .then([header, this, tk = std::move(tk)](raw_data&& rd, bool success, uint32_t /*read_size*/) mutable
      {
        if (!success)
          return;

        // Read the next header
        read_packet_header(std::move(tk));

        // handle the data
        static_cast<Child*>(this)->on_packet(header, std::move(rd));
      });
    }

    static bool on_connection(Child& chld)
    {
      chld.read_packet_header(chld.in_flight_operations.get_token());
      chld.on_connection_setup();
      return true;
    }
  };
}

