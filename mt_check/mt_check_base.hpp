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

#pragma once

#include <atomic>
#include <thread>
#include "../type_id.hpp"
#include "../macro.hpp"

namespace neam::cr
{
#ifndef N_ENABLE_MT_CHECK
# define N_ENABLE_MT_CHECK false
#endif

  class mt_checker_base
  {
    public:
      mt_checker_base();
      ~mt_checker_base();
      mt_checker_base(const mt_checker_base& ) : mt_checker_base() {}
      mt_checker_base(mt_checker_base&& ) : mt_checker_base() {}
      mt_checker_base& operator = (const mt_checker_base& ) { return *this; }
      mt_checker_base& operator = (mt_checker_base&& ) { return *this; }

      void enter_read_section() const;
      void leave_read_section() const;

      void enter_write_section() const;
      void leave_write_section() const;

      void check_no_access() const;

    protected:
      virtual const char* debug_container_string() const { return "<unnamed>"; }

    private:
      static uint32_t get_writer_count(uint64_t x);
      static uint32_t get_reader_count(uint64_t x);

    private:
      mutable std::atomic<uint64_t> counters;
      mutable std::thread::id writer_id; // allows reentrancy (easier in-place usage of mt-check)
  };

  /// \brief Add this as a member in your class (with [[no_unique_address]]) to enable mt-checking capabilities
  /// \note Most macros expect a _get_mt_instance_checker()
  template<typename Child>
  class mt_checker : public mt_checker_base
  {
    protected:
#if N_ENABLE_MT_CHECK
      // no need to have strings polluting the exec if it's disabled
      const char* debug_container_string() const final override { return ct::type_name<Child>.str; }
#endif
  };

  /// \brief Inherit from this class to enable multi-threaded checking in your class
  /// \note Will resolve to an empty base when the checks are not enabled
  template<typename Child>
  class mt_checked
  {
#if N_ENABLE_MT_CHECK
    public:
      const mt_checker_base& _get_mt_instance_checker() const { return mt_instance_checker; }
    protected:
      mt_checker<Child> mt_instance_checker;
#endif
  };


  class mt_checker_read_guard_adapter
  {
    public:
      static mt_checker_read_guard_adapter& adapt(const mt_checker_base& sl) { return (mt_checker_read_guard_adapter&)(sl); }

      void lock() { reinterpret_cast<mt_checker_base*>(this)->enter_read_section(); }
      void unlock() { reinterpret_cast<mt_checker_base*>(this)->leave_read_section(); }
  };
  class mt_checker_write_guard_adapter
  {
    public:
      static mt_checker_write_guard_adapter& adapt(const mt_checker_base& sl) { return (mt_checker_write_guard_adapter&)(sl); }

      void lock() { reinterpret_cast<mt_checker_base*>(this)->enter_write_section(); }
      void unlock() { reinterpret_cast<mt_checker_base*>(this)->leave_write_section(); }
  };

#if N_ENABLE_MT_CHECK
# define N_MTC_READER_SCOPE std::lock_guard N_GLUE(_n_mtc_r_lg_, __COUNTER__) { neam::cr::mt_checker_read_guard_adapter::adapt(this->mt_instance_checker) }
# define N_MTC_WRITER_SCOPE std::lock_guard N_GLUE(_n_mtc_w_lg_, __COUNTER__) { neam::cr::mt_checker_write_guard_adapter::adapt(this->mt_instance_checker) }
# define N_MTC_SCOPE(MODE) std::lock_guard N_GLUE(_n_mtc_##MODE##_lg_, __COUNTER__) { neam::cr::mt_checker_##MODE##_guard_adapter::adapt(this->mt_instance_checker) }

# define N_MTC_READER_SCOPE_OBJ(X) std::lock_guard N_GLUE(_n_mtc_r_lg_, __COUNTER__) { neam::cr::mt_checker_read_guard_adapter::adapt((X)._get_mt_instance_checker()) }
# define N_MTC_WRITER_SCOPE_OBJ(X) std::lock_guard N_GLUE(_n_mtc_w_lg_, __COUNTER__) { neam::cr::mt_checker_write_guard_adapter::adapt((X)._get_mt_instance_checker()) }
# define N_MTC_SCOPE_OBJ(MODE, X) std::lock_guard N_GLUE(_n_mtc_##MODE##_lg_, __COUNTER__) { neam::cr::mt_checker_##MODE##_guard_adapter::adapt((X)._get_mt_instance_checker()) }
#else
# define N_MTC_READER_SCOPE
# define N_MTC_WRITER_SCOPE
# define N_MTC_SCOPE(MODE)

# define N_MTC_READER_SCOPE_OBJ(X)
# define N_MTC_WRITER_SCOPE_OBJ(X)
# define N_MTC_SCOPE_OBJ(MODE, X)
#endif


#define N_MTC_WRAP_METHOD_CHK(NAME, MODE, EXTRA, CHK) \
      template<typename... Args> auto NAME(Args&&... args) EXTRA -> decltype(parent_t::NAME(std::forward<Args>(args)...)) \
      { \
        N_MTC_SCOPE(MODE); \
        CHK; \
        return parent_t::NAME(std::forward<Args>(args)...); \
      }
#define N_MTC_WRAP_METHOD_FIRST_PARAM_CHK(NAME, X, MODE, EXTRA, CHK) \
      template<typename... Args> auto NAME(X x, Args&&... args) EXTRA -> decltype(parent_t::NAME(std::forward<X>(x), std::forward<Args>(args)...)) \
      { \
        N_MTC_SCOPE(MODE); \
        CHK; \
        return parent_t::NAME(std::forward<X>(x), std::forward<Args>(args)...); \
      }

#define N_MTC_WRAP_METHOD(NAME, MODE, EXTRA)  N_MTC_WRAP_METHOD_CHK(NAME, MODE, EXTRA, )

#define N_MTC_WRAP_METHOD_FIRST_PARAM(NAME, X, MODE, EXTRA) N_MTC_WRAP_METHOD_FIRST_PARAM_CHK(NAME, X, MODE, EXTRA, )

}
