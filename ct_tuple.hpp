//
// file : ct_tuple.hpp
// in : file:///home/tim/projects/nsched/nsched/tools/ct_tuple.hpp
//
// created by : Timothée Feuillet on linux.site
// date: 24/07/2014 11:59:53
//
//
// Copyright (C) 2014 Timothée Feuillet
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#ifndef __N_379709138846091261_1376240583__CT_TUPLE_HPP__
# define __N_379709138846091261_1376240583__CT_TUPLE_HPP__

namespace neam
{
  namespace ct
  {
    // hardcoded tuple. (where types and values are part of the type of the tuple).
    // NOTE: is that possible ??
    template<typename... Types, Types... Values>
    struct tuple
    {
    };

// a macro to go with it. (it simply create the tuple type from the value list).
// use it like this: neam::ct::N_CT_TUPLE_TYPE(1, my_object(), &this_variable).
// Doesn't works with strings (this won't works: neam::ct::N_CT_TUPLE_TYPE("coucou")).
#define N_CT_TUPLE_TYPE(...) tuple

  } // namespace ct
} // namespace neam

#endif /*__N_379709138846091261_1376240583__CT_TUPLE_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

