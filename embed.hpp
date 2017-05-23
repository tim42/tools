//
// file : embed.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/embed.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 17/10/2013 17:28:01
//
//
// Copyright (c) 2013-2016 Timothée Feuillet
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


#ifndef __N_95287358664168721_1073738764__EMBED_HPP__
# define __N_95287358664168721_1073738764__EMBED_HPP__

#include "ct_string.hpp"
#include "constructor_call.hpp"
#include "tpl_float.hpp"

namespace neam
{
  template<auto Value, typename Type = decltype(Value)>
  class embed
  {
    public:
      using type = Type;

      constexpr embed() {}

      constexpr operator Type () const
      {
        return Value;
      }

      // the preferred way to use this class
      constexpr static Type get()
      {
        return Value;
      }
  };
} // namespace neam

#endif /*__N_95287358664168721_1073738764__EMBED_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

