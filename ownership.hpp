//
// file : ownership.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/ownership.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 08/01/2014 21:08:48
//
//
// Copyright (C) 2013-2014 Timothée Feuillet
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


#ifndef __N_2119167279615582259_1717942984__OWNERSHIP_HPP__
# define __N_2119167279615582259_1717942984__OWNERSHIP_HPP__

namespace neam
{
  class assume_ownership_t {} assume_ownership __attribute__((unused)); // do not change the "link" type of the previous object

  class stole_ownership_t {} stole_ownership __attribute__((unused)); // the previous object become a "link"
} // namespace neam

#endif /*__N_2119167279615582259_1717942984__OWNERSHIP_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

