//
// created by : Timothée Feuillet
// date: 2024-3-24
//
//
// Copyright (c) 2024 Timothée Feuillet
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

#include "mt_check_base.hpp"
#include "../debug/assert.hpp"

static constexpr uint64_t k_destructed_marker = 0xDEAD55AADEAD2F40;
static constexpr uint64_t k_valid_marker_mask = 0xFFFFFF00FFFF0000;

neam::cr::mt_checker_base::mt_checker_base()
{
  counters.store(k_valid_marker_mask & reinterpret_cast<uint64_t>(this), std::memory_order_release);
  check_no_access();
}

neam::cr::mt_checker_base::~mt_checker_base()
{
  check_no_access();
  counters.store(k_destructed_marker, std::memory_order_release);
}

uint32_t neam::cr::mt_checker_base::get_writer_count(uint64_t x)
{
  return (uint32_t)(x >> 32) & 0x000000FF;
}

uint32_t neam::cr::mt_checker_base::get_reader_count(uint64_t x)
{
  return (uint32_t)(x & 0x0000FFFF);
}

void neam::cr::mt_checker_base::check_no_access() const
{
  const uint64_t res = counters.load(std::memory_order_acquire);

  check::debug::n_assert(res != k_destructed_marker, "mt_checker_base: --: check_no_access: use-after-free: object has been destructed");
  check::debug::n_assert((res & k_valid_marker_mask) == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: check_no_access: use-while-uninit: object hasn't been initialized");
  check::debug::n_assert(get_reader_count(res) == 0, "mt_checker_base: {}: check_no_access: race-condition: reader count isn't 0 (is: {})", debug_container_string(), get_reader_count(res));
  check::debug::n_assert(get_writer_count(res) == 0, "mt_checker_base: {}: check_no_access: race-condition: writer count isn't 0 (is: {})", debug_container_string(), get_writer_count(res));
  check::debug::n_assert(res == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: check_no_access: unexpected state");
}

void neam::cr::mt_checker_base::enter_read_section() const
{
  // skip reader tests when the current thread is in a writer section
  if (writer_id == std::this_thread::get_id())
    return;

  const uint64_t res = counters.fetch_add(1, std::memory_order_acq_rel);

  check::debug::n_assert(res != k_destructed_marker, "mt_checker_base: --: enter_read_section: use-after-free: object has been destructed");
  check::debug::n_assert((res & k_valid_marker_mask) == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: enter_read_section: use-while-uninit: object hasn't been initialized");

  check::debug::n_assert(get_writer_count(res) == 0, "mt_checker_base: {}: enter_read_section: race-condition: writer count isn't 0 (is: {})", debug_container_string(), get_writer_count(res));
}
void neam::cr::mt_checker_base::leave_read_section() const
{
  // skip reader tests when the current thread is in a writer section
  if (writer_id == std::this_thread::get_id())
    return;

  const uint64_t res = counters.fetch_sub(1, std::memory_order_acq_rel);
  check::debug::n_assert(res != k_destructed_marker, "mt_checker_base: --: leave_read_section: use-after-free: object has been destructed");
  check::debug::n_assert((res & k_valid_marker_mask) == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: leave_read_section: use-while-uninit: object hasn't been initialized");

  check::debug::n_assert(get_reader_count(res) != 0, "mt_checker_base: {}: leave_read_section: corruption: reader count was 0 already, state is corrupted", debug_container_string());
  check::debug::n_assert(get_writer_count(res) == 0, "mt_checker_base: {}: leave_read__section: race-condition: writer count isn't 0", debug_container_string());
}

void neam::cr::mt_checker_base::enter_write_section() const
{
  const uint64_t res = counters.fetch_add(uint64_t(1) << 32, std::memory_order_acq_rel);
  check::debug::n_assert(res != k_destructed_marker, "mt_checker_base: --: enter_write_section: use-after-free: object has been destructed");
  check::debug::n_assert((res & k_valid_marker_mask) == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: enter_write_section: use-while-uninit: object hasn't been initialized");

  check::debug::n_assert(get_reader_count(res) == 0, "mt_checker_base: {}: enter_write_section: race-condition: reader count isn't 0", debug_container_string());
  check::debug::n_assert(get_writer_count(res) == 0 || writer_id == std::this_thread::get_id(), "mt_checker_base: {}: enter_write_section: race-condition: writer count isn't 0", debug_container_string());

  // mark the current thread as being in a write section
  writer_id = std::this_thread::get_id();
}
void neam::cr::mt_checker_base::leave_write_section() const
{
  const uint64_t res = counters.fetch_sub(uint64_t(1) << 32, std::memory_order_acq_rel);
  check::debug::n_assert(res != k_destructed_marker, "mt_checker_base: --: leave_write_section: use-after-free: object has been destructed");
  check::debug::n_assert((res & k_valid_marker_mask) == (k_valid_marker_mask & reinterpret_cast<uint64_t>(this)), "mt_checker_base: --: leave_write_section: use-while-uninit: object hasn't been initialized");

  check::debug::n_assert(writer_id == std::this_thread::get_id(), "mt_checker_base: {}: leave_write_section: race-condition: a different writer thread took ownership of the class", debug_container_string());
  if (get_writer_count(res) == 1)
    writer_id = {};

    // check::debug::n_assert(get_writer_count(res) == 1, "mt_checker_base: {}: leave_write_section: corruption: writer count was 0 already, state is corrupted", debug_container_string());
  check::debug::n_assert(get_reader_count(res) == 0, "mt_checker_base: {}: leave_write_section: race-condition: reader count wasn't 0", debug_container_string());
}

