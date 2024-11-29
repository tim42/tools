
#include "logger.hpp"

#include "../container_utils.hpp"

#if __has_include(<fmt/format.h>)
  #include <filesystem>
  #include <fmt/format.h>
  #include <fmt/color.h>
  #include "../chrono.hpp"
  #include "../tracy.hpp"
#endif

namespace neam::cr
{
  logger& get_global_logger()
  {
    static logger out;
    return out;
  }
  logger::log_location_helper out(bool skip_lock, std::source_location loc)
  {
    return get_global_logger()(skip_lock, loc);
  }


  void logger::log_str(severity s, const std::string& str, std::source_location loc)
  {
    if (!can_log(s))
      return;

    const auto lines = split_string(str, "\n");
    for (const auto& line : lines)
    {
      if (callbacks.empty())
        print_log_to_console(nullptr, s, line, loc);
      for (auto&& it : callbacks)
      {
        it.fnc(it.data, s, line, loc);
      }
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
  std::string format_log_to_string(neam::cr::logger::severity s, const std::string& msg, std::source_location loc)
  {
    static neam::cr::chrono chr;
    std::string path = (std::string)std::filesystem::proximate(loc.file_name(), std::filesystem::path(std::source_location::current().file_name()) / "../../../..") + " ";
    return fmt::format("[{:>12.6f}] [{:>4}] {:.<55}:{:>4}: {}", std::max(0.0, chr.now_relative()), neam::cr::logger::severity_abbr(s), path, loc.line(), msg);
  }

  void print_log_to_console(void*, neam::cr::logger::severity s, const std::string& msg, std::source_location loc)
  {
    static const fmt::text_style critical = fmt::emphasis::bold | fmt::fg(fmt::color::crimson) | fmt::bg(fmt::color::gainsboro);
    static const uint32_t critical_color = 0xFF0000;
    static const fmt::text_style error = fmt::emphasis::bold | fmt::fg(fmt::color::red);
    static const uint32_t error_color = 0xFF5500;
    static const fmt::text_style warn = fmt::emphasis::bold | fmt::fg(fmt::color::orange);
    static const uint32_t warn_color = 0xFFBB00;
    static const fmt::text_style normal;
    static const uint32_t normal_color = 0xFFFFFF;
    static const fmt::text_style debug = fmt::fg(fmt::color::gray);
    static const uint32_t debug_color = 0x555555;
    fmt::text_style style;
    [[maybe_unused]] uint32_t color = normal_color;
    switch (s)
    {
      case neam::cr::logger::severity::debug: style = debug; color = debug_color; break;
      case neam::cr::logger::severity::message: style = normal; color = normal_color; break;
      case neam::cr::logger::severity::warning: style = warn; color = warn_color; break;
      case neam::cr::logger::severity::error: style = error; color = error_color;break;
      case neam::cr::logger::severity::critical: style = critical; color = critical_color; break;
    }

    std::string msg_str = format_log_to_string(s, msg, loc);
    TRACY_LOG_COLOR(msg_str.data(), msg_str.size(), color);
    fmt::print(style, "{}", msg_str);
    fmt::print(fmt::text_style{}, "\n"); // reset style to the term default (avoid leaving with red everywhere in case of a critical error)
  }
#endif
}
