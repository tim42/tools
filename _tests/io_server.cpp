
#include <filesystem>

#include "../io/io.hpp"
#include "../io/network_helper.hpp"
#include "../io/connections.hpp"
#include "../threading/threading.hpp"

#include "../logger/logger.hpp"
#include "../cmdline/cmdline.hpp"
#include "../ring_buffer.hpp"

#include "task_manager_helper.hpp"

using namespace neam;

struct global_options
{
  // options
  bool debug = false;
  bool multithreaded = true;

  // positional params:
  std::vector<std::string_view> parameters;
};
N_METADATA_STRUCT(global_options)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(debug),
    N_MEMBER_DEF(multithreaded)
  >;
};

struct connection_state;
void split_buffer(connection_state& state, uint32_t from = 0);
void broadcast(io::network::base_server_interface& server_base, std::string&& line)
{
  server_base.for_each_connection([line = std::move(line)](auto & connection, cr::token_counter::ref&&)
  {
    connection.queue_full_send(raw_data::allocate_from(line));
  });
}

// Connection state data. Held alive while a connection is alive.
struct connection_state : public io::network::ring_buffer_connection_t<connection_state>
{
  cr::event_token_t on_close_tk;

  void on_connection_setup()
  {
    cr::out().warn("[{}]: new connection (connection count: {})", socket, server_base->get_connection_count() + 1);
    queue_full_send(raw_data::allocate_from(std::string("[hello. To close the connection: /close, to quit the server: /quit or /force-quit]\n")));
    broadcast(*server_base, fmt::format("[{} has entered the chat]\n", socket));

    on_close_tk = on_close.add([this, old_socket = socket]
    {
      cr::out().warn("[{}]: connection closed", old_socket);
      broadcast(*server_base, fmt::format("[{} has left the chat]\n", old_socket));
      ioctx->cancel_all_pending_operations_for(ioctx->stdin());
      on_close_tk.release();
    });
  }

  void on_buffer_full()
  {
    cr::out().warn("[{}]: Ring buffer is full: closing connection", socket);

    queue_full_send(raw_data::allocate_from(std::string("[connection is being closed: ring buffer is full]\n")))
    .then([this](raw_data&& /*data*/, bool /*success*/, size_t /*write_size*/)
    {
      close();
    });
  }

  void on_read(uint32_t start_offset, uint32_t size)
  {
    split_buffer(*this, start_offset);
  }
};

void handle_line(connection_state& state, std::string_view line)
{
  if (line.size() == 0) return;
  if (line[0] == '/')
  {
    // handle commands: (should be better done, but hey, it works this way)
    if (line == "/quit")
    {
      cr::out().warn("[{}]: quitting server (waiting for other connections to close)", state.socket);
      {
        state.server_base->close_listening_socket();
        // We close the socket, but keep all the other connections alive
      }
      broadcast(*state.server_base, fmt::format("[server is being closed by {}]", state.socket));
      state.queue_full_send(raw_data::allocate_from(std::string("[goodbie]\n")))
      .then([&state](raw_data&& /*data*/, bool /*success*/, size_t /*write_size*/)
      {
        state.close();
      });
    }
    else if (line == "/force-quit")
    {
      cr::out().warn("[{}]: quitting server and closing all connections", state.socket);
      {
        state.server_base->close_listening_socket();
      }
      broadcast(*state.server_base, fmt::format("[server is being closed by {}, all connections will be closed]", state.socket));
      state.queue_full_send(raw_data::allocate_from(std::string("[goodbie]\n")))
      .then([&state](raw_data&& /*data*/, bool /*success*/, size_t /*write_size*/)
      {
        state.server_base->close_all_connections();
      });
    }
    else if (line == "/close")
    {
      cr::out().warn("[{}]: closing connection", state.socket);
      state.queue_full_send(raw_data::allocate_from(std::string("[goodbie]\n")))
      .then([&state](raw_data&& /*data*/, bool /*success*/, size_t /*write_size*/)
      {
        state.close();
      });
    }
    else
    {
      cr::out().warn("[{}]: unknown command: {}", state.socket, line);
      state.queue_full_send(raw_data::allocate_from(fmt::format("[unknown command {}]\n", line)));
    }
    return;
  }

  // simulate work:
  std::this_thread::sleep_for(std::chrono::microseconds(50));
  // then: broadcast the line:
  cr::out().debug("[{}]: msg: {}", state.socket, line);

  broadcast(*state.server_base, fmt::format("[{}]: {}\n", state.socket, line));
}

// crude line splitter
// (find a \n then move that line to a string_view)
void split_buffer(connection_state& state, uint32_t from)
{
  bool found;

  do
  {
    found = false;
    if (state.read_buffer.size() == 0)
      return;
    for (uint32_t i = from; i < state.read_buffer.size(); ++i)
    {
      if (state.read_buffer.at(i) == '\n')
      {
        if (i > 0)
        {
          raw_data line_data = raw_data::allocate(i);
          line_data.size = state.read_buffer.pop_front((uint8_t*)line_data.data.get(), i);
          const std::string_view line {(const char*)line_data.data.get(), line_data.size};

          handle_line(state, line);
        }
        // remove the ending \n
        state.read_buffer.pop_front();
        found = true;
        break;
      }
    }
    // because we removed entries from the buffer, we start from 0 next time
    from = 0;
  }
  while (found); // multiple lines may be sent togethers
}

int main(int argc, char** argv)
{
  cr:: get_global_logger().min_severity = neam::cr::logger::severity::message;
  cr:: get_global_logger().register_callback(neam::cr::print_log_to_console, nullptr);

  // parse command line args:
  cmdline::parse cmd(argc, argv);
  bool success = false;
  const global_options gbl_opt = cmd.process<global_options>(success, 1);

  // print a help message and exit:
  if (!success || (gbl_opt.parameters.size() > 0 && gbl_opt.parameters[0] == "help") || (gbl_opt.parameters.size() > 1))
  {
    // output the different options and exit:
    cr::out().warn("usage: {} [options] [port-number]", argv[0]);
    cr::out().log("possible options:");
    cmdline::arg_struct<global_options>::print_options();
    return 1;
  }

  uint16_t port = 0; // let the os chose one for us
  if (gbl_opt.parameters.size() > 0)
  {
    port = ::strtoul(gbl_opt.parameters[0].data(), nullptr, 10);
  }

  if (gbl_opt.debug)
    cr:: get_global_logger().min_severity = neam::cr::logger::severity::debug;

  static constexpr uint32_t k_thread_count = 7;
  if (gbl_opt.multithreaded)
    cr::out().log("using {} threads", k_thread_count);

  {
    // create a task manager:
    tm_helper_t tmh;
    std::deque<std::thread> thr;
    if (gbl_opt.multithreaded)
    {
      // very simple group data:
      neam::threading::task_group_dependency_tree tgd;
      tgd.add_task_group("io"_rid);
      tmh.setup(k_thread_count, std::move(tgd));
    }

    // We need it to be destructed after io::context (in the case of the single-threaded version)
    std::optional<io::network::base_server<connection_state>> bs;

    io::context ctx;

    if (gbl_opt.multithreaded)
    {
      // we use the non-transient task group for the actual tasks so that we avoid any sync at "frame ends"
      // note: this may not be a very good idea, as without the 50us sleep the multi-threaded version is at best as fast as the single-threaded one
      //       (overheads of very very small tasks)
      //       The better way to do mt io is a case-by-case (only for long processing of stuff).
      ctx.force_deferred_execution(&tmh.tm, threading::k_non_transient_task_group);
//       ctx.force_deferred_execution(&tm, tm.get_group_id("io"_rid));
    }

    bs.emplace(ctx);
    const neam::id_t socket = ctx.create_listening_socket(port, ctx.ipv4(0, 0, 0, 0));
    bs->set_socket(socket);
    bs->async_accept();
    cr::out().log("listening on port {}", ctx.get_socket_port(socket));


    if (gbl_opt.multithreaded)
    {
      // setup the io/thread stuff:
      tmh.tm.set_start_task_group_callback(tmh.tm.get_group_id("io"_rid), [&]
      {
        // launch a task to allow the io group to be used for other purposes
        // (if we put our deferred tasks in the io group, this allows to start working on them while
        //  the processing of io is done)
        tmh.tm.get_task([&]
        {
          ctx.process();

          if (!tmh.tm.has_pending_tasks() && !ctx.has_pending_operations())
          {
            // where in a game engine we would simply ignore this, we don't want to consume 100% of the cpu waiting for stuff
            // so we instead decide to stall a bit
            // note: if we didn't dispatch tasks from io::context, we could do the following instead:
            //  ctx._wait_for_queries();

            std::this_thread::sleep_for(std::chrono::microseconds(500));
          }

          // The stopping condition:
          //  - all sockets must be closed
          //  - io::context doesn't have any:
          //    - non-queued operations
          //    - in-flight operations
          // Because we configured io::context to queue tasks for all io operations,
          // if there's any remaining operation, it destructor will queue and wait for them,
          // launching tasks that have no chance of running, which will trigger an assert
          if (!bs->is_socket_valid() && !bs->has_any_connections() && !ctx.has_pending_operations() && !ctx.has_in_flight_operations())
            tmh.request_stop();
        });
      });

      tmh.enroll_main_thread();

      tmh.join_all_threads();
    }

    // the context will process all pending stuff while trying to run its destructor, so we're kept alive untill the socket close
  }

  return 0;
}

