
#define IS_IN_TARGET_B 1
#include "rpc_target_a.cpp"

namespace rpc_b
{
  // We simply implement some of the functions here
  void log_str(std::string str, bool rep)
  {
    neam::cr::out().log("rpc::b::log_str: {}", str);
    if (rep)
      rpc_a::log_str("rpc_b: received this string: " + str, false);
  }
}

