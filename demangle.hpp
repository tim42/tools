//
// file : demangle.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/demangle.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 19/03/2014 14:56:08
//
//
// Copyright (c) 2014-2016 Timothée Feuillet
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

#ifndef __N_11013861961332692490_320117215__DEMANGLE_HPP__
# define __N_11013861961332692490_320117215__DEMANGLE_HPP__

#include <string>
#include "compiler_detection.hpp"

// clang only
#ifdef NEAM_COMPILER_CLANG
#include "type_id.hpp"
#endif

#ifndef __has_feature
#define __has_feature(x) false
#endif

#if !defined(NEAM_COMPILER_CLANG) && (defined(__GXX_RTTI) || __has_feature(cxx_rtti))
#include <typeinfo>
#endif

// GCC only
#if defined(NEAM_COMPILER_GCC)
#include <cxxabi.h>
#include <stdlib.h>

namespace neam
{
  static inline std::string demangle(const std::string &symbol)
  {
    int status = 0;
    char *realname = nullptr;

    realname = abi::__cxa_demangle(symbol.data(), 0, 0, &status);
    std::string ret;
    if (status)
      ret = symbol;
    else
    {
      ret = realname;
      free(realname);
    }
    return ret;
  }
} // neam
#else // !__GNUC__
namespace neam
{
  static inline std::string demangle(const std::string &symbol)
  {
    return symbol;
  }
} // neam
#endif

namespace neam
{
  template<typename Type>
  static inline std::string demangle()
  {
#if defined(NEAM_COMPILER_CLANG)
    return ct::type_name<Type>.str;
#elif defined(__GXX_RTTI) || __has_feature(cxx_rtti)
    return demangle(typeid(Type).name());
#else
    return "[unknow symbol: rtti disabled]";
#endif
  }
} // neam

#endif /*__N_11013861961332692490_320117215__DEMANGLE_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

