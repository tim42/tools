
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

#if __has_include(<fmt/format.h>)
#include "../chrono.hpp"
  std::string format_log_to_string(neam::cr::logger::severity s, const std::string& msg, std::source_location loc)
  {
    static neam::cr::chrono chr;
    std::string path = std::filesystem::proximate(loc.file_name(), std::filesystem::path(std::source_location::current().file_name()) / "../../..");
    return fmt::format("[{:>12.6f}] [{:>4}] {:.<65}:{:>4}: {}", std::max(0.0, chr.now_relative()), neam::cr::logger::severity_abbr(s), path, loc.line(), msg);
  }

  void print_log_to_console(void*, neam::cr::logger::severity s, const std::string& msg, std::source_location loc)
  {
    static const fmt::text_style error = fmt::emphasis::bold | fmt::fg(fmt::color::red);
    static const fmt::text_style warn = fmt::emphasis::bold | fmt::fg(fmt::color::orange);
    static const fmt::text_style normal;
    static const fmt::text_style debug = fmt::fg(fmt::color::gray);
    fmt::text_style style;
    switch (s)
    {
      case neam::cr::logger::severity::debug: style = debug; break;
      case neam::cr::logger::severity::message: style = normal; break;
      case neam::cr::logger::severity::warning: style = warn; break;
      case neam::cr::logger::severity::error: style = error; break;
      case neam::cr::logger::severity::critical: style = error; break;
    }

    fmt::print(style, "{}", format_log_to_string(s, msg, loc));
    fmt::print(fmt::text_style{}, "\n"); // reset style to the term default (avoid leaving with red everywhere in case of a critical error)
  }
#endif
}
