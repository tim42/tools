//
// file : spinlock.hpp
// in : file:///home/tim/projects/yaggler/yaggler/threads/spinlock.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 19/10/2013 05:02:34
//

#ifndef __N_343181348955182040_1435630634__SPINLOCK_HPP__
# define __N_343181348955182040_1435630634__SPINLOCK_HPP__

#include <atomic>
#include <thread>
#include <new>

namespace neam
{
  namespace spinlock_internal
  {
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif
  }

  /// \brief a simple spinlock class
  class alignas(spinlock_internal::hardware_destructive_interference_size) spinlock
  {
    public:
      spinlock() = default;
      ~spinlock() = default;
      spinlock(spinlock&) = delete;
      spinlock& operator = (spinlock&) = delete;

      /// \brief Lock the lock. Only return when the lock was acquired.
      void lock()
      {
        while (true)
        {
          if (try_lock())
            return;
          _wait_for_lock();
//           for (unsigned i = 0; !_get_state(); ++i)
//           {
//             if (i > 100000)
//               lock_flag.wait(true, std::memory_order_relaxed);
//           }
        }
      }

      /// \brief Try to lock the lock, return whether the lock was acquired
      bool try_lock()
      {
        return !lock_flag.test_and_set(std::memory_order_acquire);
      }

      /// \brief Unlock the lock
      void unlock()
      {
        lock_flag.clear(std::memory_order_release);
//         lock_flag.notify_all();
      }

      /// \brief Does not lock the lock, simply wait for the lock to be unlocked
      /// \note marked as advaced: the lock _was_ unlocked at some point,
      ///       but this operation does not prevent any other thread to lock the lock and modify the protected data
      void _wait_for_lock() const
      {
        while (_get_state());
//         lock_flag.wait(true, std::memory_order_acquire);
      }

      bool _get_state() const
      {
        return lock_flag.test(std::memory_order_relaxed);
      }

    private:
      std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;

//       uint8_t _padding[spinlock_internal::hardware_destructive_interference_size - sizeof(std::atomic_flag)];
  };
} // namespace neam

#endif /*__N_343181348955182040_1435630634__SPINLOCK_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

