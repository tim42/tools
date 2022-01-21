
#include "logger.hpp"

namespace neam::cr
{
  logger out;

  void logger::log_str(severity s, const std::string& str, std::source_location loc)
  {
    if (!can_log(s))
      return;
    for (auto&& it : callbacks)
    {
      it.fnc(it.data, s, str, loc);
    }
  }

  void logger::register_callback(callback_t cb, void* data)
  {
    callbacks.push_back({cb, data});
  }

  void logger::unregister_callback(callback_t cb, void* data)
  {
    for (auto it = callbacks.begin(); it < callbacks.end(); ++it)
    {
      if (it->fnc == cb && it->data == data)
        it = callbacks.erase(it);
    }
  }
}
