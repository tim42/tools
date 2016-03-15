//
// file : ownership.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/ownership.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 08/01/2014 21:08:48
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

#ifndef __N_2119167279615582259_1717942984__OWNERSHIP_HPP__
# define __N_2119167279615582259_1717942984__OWNERSHIP_HPP__

// ownership is a dumb resource management pattern: it's like a reference counting, but without the reference.
// There is an owner of the resource that is responsible of its destruction (or whatever)
// and there are 'links' objects: instances that share the pointer or the id of the resource,
// but won't destroy it at the end of their life time.
// Please note that not being the owner doesn't prevent from modifying the resource, but, mainly, from destructing it.
// This management pattern involve a bit more reflection than the reference counting one, but is lightweight, thread safe (nothing is shared)
// and gives absolutely no warranty: its the duty of the user to deal with a simple scoped memory management.
// But when used well, this is fast !!! (duplication is explicitly asked by the user)
//
// some common member functions are:
//  - give_up_ownership()       <- set the ownership flag to false
//  - assume_ownership()        <- set the ownership flag to true
//  - stole_ownership(obj &)    <- behave exactly the same as when constructed with the flag 'stole_ownership'
//
// and a number of constructor with those flags:

namespace neam
{
  // The object will assume that he is the only owner of the resource (set the 'owner' flag to true)
  static class assume_ownership_t {} assume_ownership __attribute__((unused));

  // Stole the ownership flag from the other object (the other will become a 'link').
  // If the object to which we steal the owner flag wasn't the owner, the ownership thief will **NOT** assume ownership.
  static class stole_ownership_t {} stole_ownership __attribute__((unused));

  // force the creation of a copy of the resource
  static class force_duplicate_t {} force_duplicate __attribute__((unused));
} // namespace neam

#endif /*__N_2119167279615582259_1717942984__OWNERSHIP_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

