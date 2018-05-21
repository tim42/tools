

#include <iostream>
#include "type_id.hpp"
#include "embed.hpp"
#include "compiler_detection.hpp"
#include "logger/logger.hpp"

template<typename T>
void print_type_info()
{
  neam::cr::out.log() << neam::ct::type_name<T>.str << "  #" << std::hex << neam::ct::type_hash<T> << '\n';
}
template<auto V>
void print_val_info()
{
  neam::cr::out.log() << neam::ct::value_name<V>.str << "  #" << std::hex << neam::ct::value_hash<V> << '\n';
  print_type_info<decltype(V)>();
}

int main()
{
  print_type_info<int>();
  print_type_info<double>();
  print_type_info<decltype(&print_type_info<double>)>();
  print_type_info<neam::embed<&print_type_info<double>>>();
  [[maybe_unused]] const auto l = []() -> double { return 0; };
  print_type_info<decltype(l)>();
  print_type_info<decltype(+l)>();
  print_type_info<neam::string_t>();
  print_type_info<const neam::string_t>();
  print_type_info<neam::ct::string&>();
  print_type_info<neam::ct::string&&>();
  print_type_info<neam::ct::string*>();
  print_type_info<neam::ct::string*>();

  print_val_info<&main>();
  print_val_info<0>();
  print_val_info<nullptr>();
  enum class e { toto, tutu };
  print_val_info<e::toto>();
  print_val_info<e::tutu>();
  print_val_info<neam::compiler>();
  return 0;
}
