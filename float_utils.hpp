//
// file : float_utils.hpp
// in : file:///home/tim/projects/ntools/float_utils.hpp
//
// created by : Timothée Feuillet
// date: lun. juil. 2 17:47:55 2018 GMT-0400
//
//
// Copyright (c) 2018 Timothée Feuillet
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

#include <type_traits>
#include <limits>

#include "integer_tools.hpp"

/// \file
///  - to_unorm_fp and from_unorm_fp are utilities that convert floating point numbers to/from integers without precision loss (a round-trip will result in exactly the same number)
///


namespace neam
{
  namespace cr
  {
    /// \brief Convert an unsigned integer into a [0, 1[ floating point without loosing accuracy
    /// Use PadWithSelf to replicate the integer value in order to fill the mantissa
    /// The default is true, meaning that a value of (uint8_t)0xFF will have the floating point value the closest to 1 (1 minus 1 ulps (aka. (float)0.99999988)) instead of 0.996...
    /// Setting PadWithSelf to false will clear the remaining bits of the mantissa.
    ///
    /// The intended use it to provide huge values (8, 16, 23 bits), convert them to float, perform some operations (*) then retrieve the fixed point value (in 8, 16, 23 bits)
    ///  * dividing by the floating point value will have a one bit error, because a range of [0, 1] would need either one extra bit, not having a 0 or having non constant steps between increments
    ///    my choice here is to have a 0, constant steps between increments (although, if PadWithSelf is true, steps are of 1.1 not 1) but lacking a true 1.
    ///    0xFF is not 256, but 255 and because 255 isn't a power of two, it isn't possible to have constant steps in your increments when dividing by 255.
    ///    (this is most prominently visible for uint16_t, where you will have a one-bit error in some cases when doing the division/multiplication conversion).
    ///
    /// TODO: replace PadWithSelf with an enum class (a flag ?).
    template<typename FPType, bool PadWithSelf = true, typename Type>
    FPType to_unorm_fp(Type v)
    {
      static_assert(std::is_floating_point_v<FPType>);
      static_assert(std::is_integral_v<Type>);

      static_assert(sizeof(v) * 8 <= (std::numeric_limits<FPType>::digits - 1), "Cannot convert this type without loosing precision");
      static_assert(std::numeric_limits<FPType>::is_iec559, "Destination type does not have the required format (must be a iec559 floating point)");

      using fpint_t = ct::integer_t<sizeof(FPType), false>;
      static_assert(std::is_integral_v<fpint_t>);

      constexpr fpint_t mantissa_bit_count = std::numeric_limits<FPType>::digits - 1;
      constexpr fpint_t sign_inv_mask = (~fpint_t(0)) >> 2;
      constexpr fpint_t mantissa_mask = (~fpint_t(0)) >> (sizeof(FPType) * 8 - mantissa_bit_count);
      constexpr fpint_t mantissa_shift = mantissa_bit_count - sizeof(Type) * 8;
      constexpr fpint_t exponent = (~fpint_t(0)) & (sign_inv_mask & ~mantissa_mask);

      fpint_t mantissa = (fpint_t(v) << (mantissa_shift));

      if constexpr (PadWithSelf)
      {
        constexpr int sub = (sizeof(FPType) * 8);
        for (int i = int(mantissa_bit_count) - sub; i > -sub; i -= sub)
        {
          if (i >= 0)
            mantissa |= fpint_t(v) << i;
          else
            mantissa |= fpint_t(v) >> (-i);
        }
      }

      const fpint_t result = exponent | mantissa;
      return *reinterpret_cast<const FPType*>(&result) - 1;
    }

    /// \brief Convert floating point in [0, 1] to an integer without loosing accuracy
    template<typename Type, typename FPType>
    Type from_unorm_fp(FPType v)
    {
      static_assert(std::is_floating_point_v<FPType>);
      static_assert(std::is_integral_v<Type>);

      static_assert(std::numeric_limits<FPType>::is_iec559, "Destination type does not have the required format (must be a iec559 floating point)");

      using fpint_t = ct::integer_t<sizeof(FPType), false>;
      static_assert(std::is_integral_v<fpint_t>);

      constexpr fpint_t mantissa_bit_count = std::numeric_limits<FPType>::digits - 1;
      constexpr fpint_t sign_inv_mask = (~fpint_t(0)) >> 2;
      constexpr fpint_t mantissa_mask = (~fpint_t(0)) >> (sizeof(FPType) * 8 - mantissa_bit_count);
      constexpr fpint_t mantissa_shift = mantissa_bit_count - sizeof(Type) * 8;
      constexpr fpint_t exponent = (~fpint_t(0)) & (sign_inv_mask & ~mantissa_mask);

      v += 1; // shift the range to [1, 2[
      const fpint_t fpvalue = *reinterpret_cast<fpint_t*>(&v);
      const fpint_t actual_exponent = fpvalue & ~mantissa_mask;

      // if, because of rounding the result is in fact 2 or more, will return ~0 in the corresponding type
      // (this is why the comment says 'range [0, 1]' and not 'range [0, 1[')
      if (actual_exponent != exponent)
        return ~Type(0);

      return Type((fpvalue & mantissa_mask) >> mantissa_shift);
    }
  } // namespace cr
} // namespace neam
