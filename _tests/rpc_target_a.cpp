

#include "../io/io.hpp"
#include "../io/network_helper.hpp"
#include "../io/connections.hpp"

#include "../logger/logger.hpp"
#include "../cmdline/cmdline.hpp"

#include "rpc_decl.hpp"
using namespace neam;

#ifndef IS_IN_TARGET_B
namespace rpc_a
{
  // We simply implement some of the functions here
  void log_str(std::string str, bool rep)
  {
    cr::out().log("rpc::a::log_str: {}", str);
    if (rep)
      rpc_b::log_str("rpc_a: received a string: " + str, true);
  }
}
#endif

struct global_options
{
  // options
  bool server = false;
  bool debug = false;
  uint32_t port = 0;

  // positional params:
  std::vector<std::string_view> parameters;
};
N_METADATA_STRUCT(global_options)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(debug),
    N_MEMBER_DEF(server),
    N_MEMBER_DEF(port)
  >;
};

struct header_t
{
  static constexpr uint32_t k_key_value = 0xCACACACA;
  uint32_t key = k_key_value;
  uint32_t size;
};

struct connection_state : public io::network::header_connection_t<connection_state, 2u * 1024 * 1024 * 1024>
{
  using packet_header_t = header_t;

  cr::event_token_t on_close_tk;

  void on_connection_setup()
  {
    cr::out().debug("new connection: {}", ioctx->get_string_for_id(socket));

    if (server_base == nullptr)
    {
      on_close_tk = on_close.add([this]
      {
        cr::out().log("connection closed");
        ioctx->cancel_all_pending_operations_for(ioctx->stdin());
        on_close_tk.release();
      });
    }
  }

  bool is_header_valid(const packet_header_t& ph)
  {
    if (ph.key == packet_header_t::k_key_value)
      return true;
    cr::out().error("invalid packet received (bad header)");
    return false;
  }
  uint32_t get_size_of_data_to_read(const packet_header_t& ph) { return ph.size; }
  void on_packet_oversized(const packet_header_t& ph) { cr::out().error("oversized packet received: {}kio", ph.size / 1024); }

  void on_packet(const packet_header_t& ph, raw_data&& packet_data)
  {
    cr::out().debug("received rpc [size: {}]", packet_data.size);
    rpc::dispatcher::local_call(std::move(packet_data));
  }
};

struct adapter_t : public rpc::basic_adapter<adapter_t>
{
  adapter_t(io::context& _ctx, io::network::base_server<connection_state>& _server) : ctx(_ctx), server(&_server) {}
  adapter_t(io::context& _ctx, connection_state& _connection) : ctx(_ctx), connection(&_connection) {}

  using header_t = ::header_t;

  void init_rpc_header(header_t& ft, const raw_data& rd)
  {
    ft.key = header_t::k_key_value;
    ft.size = (uint32_t)rd.size - sizeof(header_t);
  }

  void send_rpc(raw_data&& rd)
  {
    cr::out().debug("sending rpc [size: {}]", rd.size);
    if (server != nullptr)
    {
      server->for_each_connection([&rd](io::network::connection_t& st, cr::token_counter::ref&&) mutable
      {
        st.queue_full_send(rd.duplicate()).then([tk = st.in_flight_operations.get_token()](raw_data&& rd, bool success, uint32_t write_size){});
      });
    }
    if (connection != nullptr)
    {
      connection->queue_full_send(std::move(rd)).then([tk = connection->in_flight_operations.get_token()](raw_data&& rd, bool success, uint32_t write_size){});
    }
  }

  io::context& ctx;
  io::network::base_server<connection_state>* server = nullptr;
  connection_state* connection = nullptr;
};

template<typename Fnc>
void read_fd(neam::id_t fd, io::context& ctx, Fnc&& fnc)
{
  ctx.queue_read(fd, 0, 1024).then([fd, &ctx, fnc = std::move(fnc)](raw_data&& rd, bool success, uint32_t) mutable
  {
    if (!success)
      return;
    if (fnc(std::move(rd)))
      read_fd(fd, ctx, std::move(fnc));
  });
}

int main(int argc, char** argv)
{
  cr:: get_global_logger().register_callback(neam::cr::print_log_to_console, nullptr);

  rpc::internal::log_functions();
  // parse command line args:
  cmdline::parse cmd(argc, argv);
  bool success = false;
  const global_options gbl_opt = cmd.process<global_options>(success, 0);

  if (!success)
  {
    // output the different options and exit:
    cr::out().warn("usage: {} [options]", argv[0]);
    cr::out().log("possible options:");
    cmdline::arg_struct<global_options>::print_options();
    return 1;
  }

  {
    std::optional<io::network::base_server<connection_state>> bs;
    std::optional<connection_state> connection;
    io::context ctx;
    std::optional<adapter_t> ada;

    // server or connection:
    if (gbl_opt.server)
    {
      bs.emplace(ctx);
      const neam::id_t socket = ctx.create_listening_socket(gbl_opt.port, ctx.ipv4(0, 0, 0, 0));
      bs->set_socket(socket);
      bs->async_accept();
      cr::out().log("listening on port {}", ctx.get_socket_port(socket));
      ada.emplace(ctx, *bs);
    }
    else
    {
      connection.emplace();
      connection->ioctx = &ctx;
      connection->queue_connect("127.0.0.1", gbl_opt.port)
      .then([&](bool success, cr::token_counter::ref&& tk)
      {
        if (success)
        {
          cr::out().log("Connected to 127.0.0.1:{}", gbl_opt.port);
          connection->on_connection_setup();
          connection->read_packet_header(std::move(tk));
        }
        else
        {
          cr::out().error("Failed to connect to 127.0.0.1:{}", gbl_opt.port);
          ctx.cancel_all_pending_operations_for(ctx.stdin());
        }
      });
      ada.emplace(ctx, *connection);
    }

    rpc::scoped_adapter _sa(*ada);

    // queue the read loop:
    const neam::id_t stdin = ctx.stdin();
    read_fd(stdin, ctx, [&](raw_data&& rd) -> bool
    {
      std::string s { (char*) rd.get(), rd.size };
      cr::out().debug("> {}", s);
#ifndef IS_IN_TARGET_B // A
      rpc_b::log_str(s, true);
#else // B
      rpc_a::log_str(s, true);
#endif
      return true;
    });

    // stall until everything is done:
    ctx._wait_for_submit_queries();
  }
}
