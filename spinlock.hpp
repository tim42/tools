//
// file : spinlock.hpp
// in : file:///home/tim/projects/yaggler/yaggler/threads/spinlock.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 19/10/2013 05:02:34
//

#pragma once

#include <atomic>
#include <thread>
#include <new>

// Add a ton of checks on the locks, at the cost of performance
// Checks for:
//  - deadlocks
//  - invalid unlocks (both wrong ownership and already unlocked)
//  - invalid spinlock (mostly use-after-destruction)
#ifndef N_ENABLE_LOCK_DEBUG
  #define N_ENABLE_LOCK_DEBUG false
#endif

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
  class /*alignas(spinlock_internal::hardware_destructive_interference_size)*/ spinlock
  {
    public:
      spinlock() = default;
#if N_ENABLE_LOCK_DEBUG
      ~spinlock()
      {
        // poison the lock
        lock();
        owner_id = std::thread::id();
        key = k_destructed_key_value;
      }
#else
      ~spinlock() = default;
#endif
      spinlock(spinlock&) = delete;
      spinlock& operator = (spinlock&) = delete;

      /// \brief Lock the lock. Only return when the lock was acquired.
      void lock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
        if (owner_id == std::this_thread::get_id())
        {
          printf("[spinlock: deadlock detected in lock()]\n");
          abort();
        }
#endif
        while (true)
        {
          if (try_lock())
            return;
          while (lock_flag.test(std::memory_order_relaxed));
        }
      }

      /// \brief Try to lock the lock, return whether the lock was acquired
      bool try_lock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
        if (owner_id == std::this_thread::get_id())
        {
          printf("[spinlock: deadlock detected in try_lock()]\n");
          abort();
        }
        if (!lock_flag.test_and_set(std::memory_order_acquire))
        {
          owner_id = std::this_thread::get_id();
          return true;
        }
        return false;
#else
        return !lock_flag.test_and_set(std::memory_order_acquire);
#endif
      }

      /// \brief Unlock the lock
      void unlock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
        if (!lock_flag.test())
        {
          printf("[spinlock: invalid unlock detected in unlock() (unlocking an unlocked mutex)]\n");
          abort();
        }
        if (owner_id != std::this_thread::get_id())
        {
          printf("[spinlock: invalid unlock detected in unlock() (unlocking a lock that the current thread did not lock)]\n");
          abort();
        }

        owner_id = std::thread::id();
#endif
        lock_flag.clear(std::memory_order_release);
      }

      /// \brief Does not lock the lock, simply wait for the lock to be unlocked
      /// \note marked as advaced: the lock _was_ unlocked at some point,
      ///       but this operation does not prevent any other thread to lock the lock and modify the protected data
      void _wait_for_lock() const
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
        if (owner_id == std::this_thread::get_id())
        {
          printf("[spinlock: deadlock detected in _wait_for_lock()]\n");
          abort();
        }
#endif
        while (lock_flag.test(std::memory_order_acquire));
      }

      bool _get_state() const
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
#endif
        return lock_flag.test(std::memory_order_acquire);
      }

    private:
      std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;
#if N_ENABLE_LOCK_DEBUG
      std::atomic<std::thread::id> owner_id;
      static constexpr uint64_t k_key_value = 0xCACA00CACA;
      static constexpr uint64_t k_destructed_key_value = ~k_key_value;
      std::atomic<uint64_t> key = k_key_value;
      void check_for_key() const
      {
        if (key == k_destructed_key_value)
        {
          printf("[spinlock: invalid lock: trying to do operations after lock destruction]\n");
          abort();
        }
        if (key != k_key_value)
        {
          printf("[spinlock: invalid lock: memory area is not a lock]\n");
          abort();
        }

      }
#endif
  };
} // namespace neam
