//
// file : array_wrapper.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/array_wrapper.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 26/10/2013 17:19:39
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


#ifndef __N_750243581824727426_857942864__ARRAY_WRAPPER_HPP__
# define __N_750243581824727426_857942864__ARRAY_WRAPPER_HPP__

#include <cstddef>

namespace neam
{
  // a simple array wrapper (you could use it with yaggler::shader::uniform_variable)
  template<typename Type>
  struct array_wrapper
  {
    constexpr array_wrapper(Type *_array, size_t _size) noexcept : array(_array), size(_size) {}

    template<size_t Size>
    constexpr array_wrapper(const Type (&_array)[Size]) noexcept : array(const_cast<Type *>(_array)), size(Size) {}

    Type *array;
    size_t size;
  };
} // namespace neam

#endif /*__N_750243581824727426_857942864__ARRAY_WRAPPER_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

