//
// created by : Timothée Feuillet
// date: 2021-12-11
//
//
// Copyright (c) 2021 Timothée Feuillet
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

#include "arg_struct.hpp"
#include "parse.hpp"

// cmdline parsing:
//  args can either be booleans (flags) or strings (everything else).
// The supported format is the following:
//
// [--opts] [--] [params]
//
// --opts are either strings or bools, params are strings (strings are std::string_view)
//
// boolean --opts can either be the --opts or --no-opts (as well as --opts=true / --opts=false)
//
// params are put in an optional `std::vector/...<std::string_view> parameters` member.
// There's optional shorthands support (via a `std::{unordered_}map<char, std::string_view> shorthand` member)
//
namespace neam::cmdline
{
}

