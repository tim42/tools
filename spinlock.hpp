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

namespace neam
{
  /// \brief a simple spinlock class
  class spinlock
  {
    public:
      spinlock() noexcept(noexcept(std::atomic_flag{})) = default;
      ~spinlock() noexcept = default;
      spinlock(spinlock&) = delete;
      spinlock& operator = (spinlock&) = delete;

      inline void lock() noexcept
      {
        unsigned it = 0;
        while (lock_flag.test_and_set(std::memory_order_acquire))
        {
          if (++it < 1000)
          {
            std::this_thread::yield();
            it = 0;
          }
        }
      }

      bool try_lock() noexcept
      {
        return !lock_flag.test_and_set(std::memory_order_acquire);
      }

      void unlock() noexcept
      {
        lock_flag.clear(std::memory_order_release);
      }

    private:
      std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;
  };
} // namespace neam

#endif /*__N_343181348955182040_1435630634__SPINLOCK_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

