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

// the purpose of this header is to allow 'embedding' of values as types in templates
// (really useful for the construct "typename... Args" in the base class)
// for embedding objects, see <tools/constructor_call.hpp> (neam::ct::constructor::call::embed).
// for embedding floating point numbers, see <tools/tpl_float.hpp>
// Metrik Ft. Reija Lee - Freefall Vip
namespace neam
{
  namespace embed
  {
    // the generic one...
    template<typename EmbeddedType, EmbeddedType Value>
    class embed
    {
      public:
        constexpr embed() {}

        operator EmbeddedType &()
        {
          return value;
        }
        constexpr operator EmbeddedType () const
        {
          return Value;
        }

        // the preferred way to use this class :)
        constexpr static EmbeddedType get()
        {
          return Value;
        }

      public:
        using type = EmbeddedType;
        static constexpr EmbeddedType value = Value;
    };
    template<typename EmbeddedType, EmbeddedType Value>
    constexpr EmbeddedType embed<EmbeddedType, Value>::value;

    // some internal things
    namespace internal
    {
      template<typename ArrayType, size_t Size>
      constexpr size_t _array_size(const ArrayType (&)[Size])
      {
        return Size;
      }
    } // namespace internal

    // defs (for faster/clearer use ;) )
#define N_EMBED_USING(name, type)    template<type Value> using name = neam::embed::embed<type, Value>
#define N_EMBED_ARRAY(type, obj)     embed<const type (&)[neam::embed::internal::_array_size(obj)], obj>

    // POD types
    N_EMBED_USING(int8, int8_t);
    N_EMBED_USING(int16, int16_t);
    N_EMBED_USING(int32, int32_t);
    N_EMBED_USING(int64, int64_t);

    N_EMBED_USING(uint8, uint8_t);
    N_EMBED_USING(uint16, uint16_t);
    N_EMBED_USING(uint32, uint32_t);
    N_EMBED_USING(uint64, uint64_t);

    // pointers
    N_EMBED_USING(int8_ptr, const int8_t *);
    N_EMBED_USING(int16_ptr, const int16_t *);
    N_EMBED_USING(int32_ptr, const int32_t *);
    N_EMBED_USING(int64_ptr, const int64_t *);

    N_EMBED_USING(uint8_ptr, const uint8_t *);
    N_EMBED_USING(uint16_ptr, const uint16_t *);
    N_EMBED_USING(uint32_ptr, const uint32_t *);
    N_EMBED_USING(uint64_ptr, const uint64_t *);

    N_EMBED_USING(float_ptr, const float *);
    N_EMBED_USING(double_ptr, const double *);

    // arrays refs (to conserve ct size acces)
    // NOTE: those are macros, not 'using's
#define int8_array(array)       N_EMBED_ARRAY(int8_t, array)
#define int16_array(array)      N_EMBED_ARRAY(int16_t, array)
#define int32_array(array)      N_EMBED_ARRAY(int32_t, array)
#define int64_array(array)      N_EMBED_ARRAY(int64_t, array)

#define uint8_array(array)      N_EMBED_ARRAY(uint8_t, array)
#define uint16_array(array)     N_EMBED_ARRAY(uint16_t, array)
#define uint32_array(array)     N_EMBED_ARRAY(uint32_t, array)
#define uint64_array(array)     N_EMBED_ARRAY(uint64_t, array)

#define float_array(array)      N_EMBED_ARRAY(float, array)
#define double_array(array)     N_EMBED_ARRAY(double, array)
#define GLfloat_array(array)    N_EMBED_ARRAY(GLfloat, array)

    // other
    N_EMBED_USING(string, const char *);

// a macro to simply embed something when the type doesn't matter
#define N_EMBED(val)            neam::embed::embed<decltype(val), val>
  } // namespace embed
} // namespace neam

#endif /*__N_95287358664168721_1073738764__EMBED_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

