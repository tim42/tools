//
// file : macro.hpp (2)
// in : file:///home/tim/projects/nsched/nsched/tools/macro.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 03/08/2014 18:58:27
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

#pragma once
# define __N_514825144439029395_657528994__MACRO_HPP__2___

//
// defines some macros
//

// stringify and expand and stringify
#define N_STRINGIFY(s) #s
#define N_EXP_STRINGIFY(s) N_STRINGIFY(s)

#define N_ARGS(...) __VA_ARGS__

#define N_PARENS ()

#define N_EXPAND(...) _N_EXPAND1(_N_EXPAND1(_N_EXPAND1(_N_EXPAND1(__VA_ARGS__))))
#define _N_EXPAND1(...) _N_EXPAND2(_N_EXPAND2(_N_EXPAND2(_N_EXPAND2(__VA_ARGS__))))
#define _N_EXPAND2(...) _N_EXPAND3(_N_EXPAND3(_N_EXPAND3(_N_EXPAND3(__VA_ARGS__))))
#define _N_EXPAND3(...) _N_EXPAND4(_N_EXPAND4(_N_EXPAND4(_N_EXPAND4(__VA_ARGS__))))
#define _N_EXPAND4(...) N_ARGS(__VA_ARGS__)

// ARGUMENT COUNTING

#define N_CNT_ARGS(...) _N_CNT_ARGS(__VA_OPT__(__VA_ARGS__,)  \
                                    39,38,37,36,35,34,33,32,31,30,  \
                                    29,28,27,26,25,24,23,22,21,20,  \
                                    19,18,17,16,15,14,13,12,11,10,  \
                                    9,8,7,6,5,4,3,2,1,0)
#define _N_CNT_ARGS(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9, \
                    _10,_11,_12,_13,_14,_15,_16,_17,_18,_19, \
                    _20,_21,_22,_23,_24,_25,_26,_27,_28,_29, \
                    _30,_31,_32,_33,_34,_35,_36,_37, _38, \
                    n,...) n

// FOR-EACH

#define N_FOR_EACH(macro, ...)  \
  __VA_OPT__(N_EXPAND(_N_FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define _N_FOR_EACH_HELPER(macro, a1, ...)  \
  macro(a1) \
  __VA_OPT__(_N_FOR_EACH_AGAIN N_PARENS (macro, __VA_ARGS__))

#define _N_FOR_EACH_AGAIN() _N_FOR_EACH_HELPER

// FOR_EACH, with two args: param, remaining arg count
#define N_FOR_EACH_CNT(macro, ...)                                    \
  __VA_OPT__(N_EXPAND(_N_FOR_EACH_HELPER_CNT(macro, __VA_ARGS__)))

#define _N_FOR_EACH_HELPER_CNT(macro, a1, ...)                         \
  macro(a1, N_CNT_ARGS(__VA_ARGS__))                                   \
  __VA_OPT__(_N_FOR_EACH_AGAIN_CNT N_PARENS (macro, __VA_ARGS__))

#define _N_FOR_EACH_AGAIN_CNT() _N_FOR_EACH_HELPER_CNT

// FOR_EACH, with two args: remaining arg count, param, remaining args
#define N_FOR_EACH_CNT_VA(macro, ...)                                    \
  __VA_OPT__(N_EXPAND(_N_FOR_EACH_HELPER_CNT_VA(macro, __VA_ARGS__)))

#define _N_FOR_EACH_HELPER_CNT_VA(macro, a1, ...)                       \
  macro(a1, N_CNT_ARGS(__VA_ARGS__), __VA_ARGS__)                       \
  __VA_OPT__(_N_FOR_EACH_AGAIN_CNT_VA N_PARENS (macro, __VA_ARGS__))

#define _N_FOR_EACH_AGAIN_CNT_VA() _N_FOR_EACH_HELPER_CNT_VA

// TOKEN GLUE
#define _N_GLUE(x, y) x ## y
#define N_GLUE(x, y) _N_GLUE(x, y)

