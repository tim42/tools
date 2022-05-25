
#include <filesystem>

#include "../io/io.hpp"
#include "../threading/threading.hpp"

#include "../logger/logger.hpp"
#include "../cmdline/cmdline.hpp"
#include "../ring_buffer.hpp"

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


// Connection state data. Held alive while a connection is alive.
struct connection_state
{
  static inline spinlock lock;
  static inline io::context::accept_chain last_accept;
  static inline bool should_stop = false;

  neam::id_t socket;
  cr::ring_buffer<char, 2048> read_buffer;
  bool is_closed = false;
};
using connection_state_ptr = std::unique_ptr<connection_state>;

void handle_line(io::context& ctx, neam::id_t connection, connection_state& state, std::string_view line)
{
  if (line.size() == 0) return;
  if (line[0] == '/')
  {
    // handle commands: (should be better done, but hey, it works this way)
    if (line == "/quit")
    {
      cr::out().warn("[{}]: quitting server", connection);
      {
        std::lock_guard _lg(connection_state::lock);
        connection_state::last_accept.cancel();
        connection_state::should_stop = true;
      }
      ctx.queue_write(connection, io::context::append, raw_data::allocate_from(std::string("[goodbie]\n")))
      .then([ =, &ctx, socket = state.socket](bool)
      {
        ctx.close(connection);
        ctx.close(socket);
      });
    }
    else if (line == "/close")
    {
      cr::out().warn("[{}]: closing connection", connection);
      state.is_closed = true;
      ctx.queue_write(connection, io::context::append, raw_data::allocate_from(std::string("[goodbie]\n")))
      .then([ =, &ctx](bool)
      {
        ctx.close(connection);
      });
    }
    else
    {
      cr::out().warn("[{}]: unknown command: {}", connection, line);
      ctx.queue_write(connection, io::context::append, raw_data::allocate_from(fmt::format("[unknown command {}]\n", line)));
    }
    return;
  }

  // simulate work:
  std::this_thread::sleep_for(std::chrono::microseconds(50));
  // else: send the line back:
  cr::out().debug("[{}]: line: {}", connection, line);
  ctx.queue_write(connection, io::context::append, raw_data::allocate_from(fmt::format("[{}]\n", line)));
}

// crude """"parser""""
// (find a /n then move that line to a string_view)
void split_buffer(io::context& ctx, neam::id_t connection, connection_state& state)
{
  bool found;

  do
  {
    found = false;
    if (state.read_buffer.size() == 0)
      return;
    for (uint32_t i = 0; i < state.read_buffer.size(); ++i)
    {
      if (state.read_buffer.at(i) == '\n')
      {
        if (i > 0)
        {
          raw_data line_data = raw_data::allocate(i);
          line_data.size = state.read_buffer.pop_front((char*)line_data.data.get(), i);
          const std::string_view line {(const char*)line_data.data.get(), line_data.size};

          handle_line(ctx, connection, state, line);
        }
        // remove the ending \n
        state.read_buffer.pop_front();
        found = true;
        break;
      }
    }
  }
  while (found); // multiple lines may be sent togethers
}

void handle_read(io::context& ctx, neam::id_t connection, connection_state_ptr state)
{
  // We purposefully do small read to demonstrate the ring buffer thingy and have a lot more cpu time dedicated to io than necessary
  // A real server would have bigger reads
  static constexpr size_t k_read_size = 32;

  ctx.queue_read(connection, 0, k_read_size)
  .then([=, &ctx, state = std::move(state)](raw_data&& data, bool success) mutable
  {
    if (success && data.size > 0)
    {
      const size_t inserted = state->read_buffer.push_back((const char*)data.data.get(), data.size);
      if (inserted != data.size)
      {
        // "handle" the case where there's no space left in the ring buffer: (send a message then close the connection)
        // (there is a better way to deal with it, like calling split_buffer and inserting what needs to be inserted, but...)
        cr::out().warn("[{}]: Ring buffer is full: closing connection", connection);
        ctx.queue_write(connection, io::context::append, raw_data::allocate_from(std::string("[connection is being closed: ring buffer is full]\n")))
        .then([=, &ctx](bool)
        {
          ctx.close(connection);
        });
        return;
      }

      split_buffer(ctx, connection, *state.get());

      // queue the next read:
      // (a better way would be to make the state thread-safe and queue the read at the very top of the function
      //  and cancel it when we have to close the connection)
      if (!connection_state::should_stop && !state->is_closed)
        handle_read(ctx, connection, std::move(state));
    }
    else
    {
      cr::out().warn("[{}]: Connection closed", connection);
      ctx.close(connection);
    }
  });
}

void on_connection(io::context& ctx, neam::id_t socket, neam::id_t connection)
{
  cr::out().log("new connection: {}", connection);

  // send the hello message:
  ctx.queue_write(connection, io::context::append,
                  raw_data::allocate_from(std::string("[hello. To close the connection: /close, to quit the server: /quit]\n")));

  // queue reads:
  auto state = std::make_unique<connection_state>();
  state->socket = socket;
  handle_read(ctx, connection, std::move(state));
}

void queue_accept(io::context& ctx, neam::id_t socket)
{
  std::lock_guard _lg(connection_state::lock);
  connection_state::last_accept = ctx.queue_accept(socket);
  connection_state::last_accept.then([=, &ctx](neam::id_t connection)
  {
    if (connection != neam::id_t::invalid)
    {
      on_connection(ctx, socket, connection);

      // queue the next accept (keep the server alive)
      queue_accept(ctx, socket);
    }
    else
    {
      if (connection_state::should_stop)
        cr::out().warn("closing server (waiting for pending connections to close)");
      else
        cr::out().warn("accept failed");
    }
  });
}

int main(int argc, char** argv)
{
  cr::out.min_severity = neam::cr::logger::severity::message;
  cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

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
    cr::out.min_severity = neam::cr::logger::severity::debug;

  static constexpr uint32_t k_thread_count = 7;
  if (gbl_opt.multithreaded)
    cr::out().log("using {} threads", k_thread_count);

  {
    // create a task manager:
    threading::task_manager tm;
    std::deque<std::thread> thr;
    if (gbl_opt.multithreaded)
    {
      {
        // very simple group data:
        neam::threading::task_group_dependency_tree tgd;
        tgd.add_task_group("io"_rid, "io");
        tm.add_compiled_frame_operations(tgd.compile_tree());
      }
      // spawn some worker thread:
      for (unsigned i = 0; i < k_thread_count /*thread count*/; ++i)
      {
        thr.emplace_back([&tm]
        {
          while (!connection_state::should_stop)
          {
            tm.wait_for_a_task();
            tm.run_a_task();
          }
        });
      }
    }

    io::context ctx;

    if (gbl_opt.multithreaded)
    {
      // we use the non-transient task group for the actual tasks so that we avoid any sync at "frame ends"
      // note: this may not be a very good idea, as without the 50us sleep the multi-threaded version is at best as fast as the single-threaded one
      //       (overheads of very very small tasks)
      //       The better way to do mt io is a case-by-case (only for long processing of stuff).
      ctx.force_deferred_execution(&tm, threading::k_non_transient_task_group);
//       ctx.force_deferred_execution(&tm, tm.get_group_id("io"_rid));
    }

    const neam::id_t socket = ctx.create_listening_socket(port);
    cr::out().log("listening on port {}", ctx.get_socket_port(socket));

    queue_accept(ctx, socket);

    if (gbl_opt.multithreaded)
    {
      // setup the io/thread stuff:
      tm.set_start_task_group_callback(tm.get_group_id("io"_rid), [&]
      {
        // launch a task to allow the io group to be used for other purposes
        // (if we put our deferred tasks in the io group, this allows to start working on them while
        //  the processing of io is done)
        tm.get_task([&]
        {
          ctx.process();
          ctx.process_completed_queries();

          if (!tm.has_pending_tasks() && !ctx.has_pending_operations())
          {
            // where in a game engine we would simply ignore this, we don't want to consume 100% of the cpu waiting for stuff
            // so we instead decide to stall a bit
            // note: if we didn't dispatch tasks from io::context, we could do the following instead:
            //  ctx._wait_for_queries();

            std::this_thread::sleep_for(std::chrono::microseconds(500));
          }
        });
      });

      // do some work:
      // (the main thread continues untill every thask has been run to avoid an assert)
      while (!connection_state::should_stop || tm.has_pending_tasks())
      {
        tm.wait_for_a_task();
        tm.run_a_task();
      }

      // wait for the threads to endL
      for (auto& it : thr)
      {
        if (it.joinable())
          it.join();
      }
    }

    // the context will process all pending stuff while trying to run its destructor, so we're kept alive untill the socket close
  }

  return 0;
}

