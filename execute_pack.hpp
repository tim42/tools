//
// file : execute_pack.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/execute_pack.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 03/03/2014 11:53:24
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

#ifndef __N_99570474794081008_1534814754__EXECUTE_PACK_HPP__
# define __N_99570474794081008_1534814754__EXECUTE_PACK_HPP__


// this execute 'instr' for each value in the pack (instr must depends of an variadic template / argument pack)
// The compiler is smart enought to just remove the array and keep the calls. (and inlining thems, if possible).
// So this produce no overhead !! :) (and avoid a recursive proxy function).
#define NEAM_EXECUTE_PACK(instr)    void((int []){((instr), 0)...})

// -- usage exemple:
//
// // return the sum of the sizes of all types
// template<typename... Types>
// size_t sizeof_all()
// {
//  size_t size = 0;
//  NEAM_EXECUTE_PACK(size += sizeof(Types));
//  return size;
// }
//

#endif /*__N_99570474794081008_1534814754__EXECUTE_PACK_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

