//
// file : tpl_float.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/tpl_float.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 24/07/2014 17:40:44
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
#ifndef __N_231085794123560898_1536835867__TPL_FLOAT_HPP__
# define __N_231085794123560898_1536835867__TPL_FLOAT_HPP__

#include <cstddef>
#include <cstdint>

//
// tpl_float: allow embedding floating point numbers in templates (see neam::embed)
// TODO: check for completeness of this implementation... (INFs, NaNs, ...)
// NOTE: doesn't support long doubles.
//

namespace neam
{
  namespace embed_helper
  {
    template<typename Type, int64_t Mantissa, int64_t Exp>
    struct _floating_point {};

    // for floats (Exp is a power of 2)
    template<int64_t Mantissa, int64_t Exp>
    struct _floating_point<float, Mantissa, Exp>
    {
      constexpr _floating_point() {}

      constexpr static float get_val()
      {
        return Exp < 0 ? static_cast<float>(Mantissa) / static_cast<float>(2ll << -Exp) : static_cast<float>(Mantissa) * static_cast<float>(2ll << Exp);
      }

      constexpr operator float() const
      {
        return get_val();
      }

      constexpr static float get()
      {
        return get_val();
      }

      using type = float;
    };

    // for double (Exp is a power of 2)
    template<int64_t Mantissa, int64_t Exp>
    struct _floating_point<double, Mantissa, Exp>
    {
      constexpr _floating_point() {}

      constexpr static double get_val()
      {
        return Exp < 0 ? static_cast<double>(Mantissa) / static_cast<double>(2ll << -Exp) : static_cast<double>(Mantissa) * static_cast<double>(2ll << Exp);
      }

      constexpr operator double() const
      {
        return get_val();
      }

      constexpr static double get()
      {
        return get_val();
      }

      using type = double;
    };

    // embedding for floats/doubles
    template<int64_t Mantissa, int64_t Exp>
    using floating_point = _floating_point< float, Mantissa, Exp >;
    template<int64_t Mantissa, int64_t Exp>
    using double_floating_point = _floating_point< double, Mantissa, Exp >;

#define N_EMBED_FLOAT(Mantissa, Exp)        neam::embed::floating_point<Mantissa, Exp>
#define N_EMBED_DOUBLE(Mantissa, Exp)        neam::embed::double_floating_point<Mantissa, Exp>

  } // namespace embed_helper
} // namespace neam

#endif /*__N_231085794123560898_1536835867__TPL_FLOAT_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

