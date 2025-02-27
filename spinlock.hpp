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
#if N_ENABLE_LOCK_DEBUG
  #ifdef __linux__
    #include <execinfo.h>
  #endif
#endif
#include "tracy.hpp"
#if N_ENABLE_LOCK_DEBUG && N_USE_TRACY
  #include <source_location>
  #include <map>
  #include "hash/fnv1a.hpp"
  #include "ct_string.hpp"
#endif
namespace neam
{
  namespace spinlock_internal
  {
#if N_ENABLE_LOCK_DEBUG
  #ifdef __linux__
    inline void print_backtrace()
    {
      constexpr uint32_t k_buff_size = 100;
      void* buffer[k_buff_size];
      int nptrs = backtrace(buffer, k_buff_size);
      backtrace_symbols_fd(buffer, nptrs, 0);
    }
  #else
    static void print_backtrace() {}
  #endif
#endif
  }

  /// \brief a simple spinlock class
  class spinlock
  {
    public:
      spinlock() = default;

#if N_ENABLE_LOCK_DEBUG
      ~spinlock()
      {
        // poison the lock
        if (!try_lock())
        {
          printf("[spinlock: trying to destroy a locked lock]\n");
          spinlock_internal::print_backtrace();
          abort();
        }
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
          spinlock_internal::print_backtrace();
          abort();
        }
#endif
        while (true)
        {
          if (try_lock())
          {
            return;
          }
          while (_relaxed_test());
        }
      }

      /// \brief Lock the lock. Only return when the lock was acquired.
      /// Don't check if the thread has lock ownership
      void _lock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
#endif
        while (true)
        {
          if (try_lock())
          {
            return;
          }
          while (_relaxed_test());
        }
      }

      /// \brief Try to lock the lock, return whether the lock was acquired
      [[nodiscard]] bool try_lock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
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
          spinlock_internal::print_backtrace();
          abort();
        }
        if (owner_id != std::this_thread::get_id())
        {
          printf("[spinlock: invalid unlock detected in unlock() (unlocking a lock that the current thread did not lock)]\n");
          spinlock_internal::print_backtrace();
          abort();
        }

        owner_id = std::thread::id();
#endif
        lock_flag.clear(std::memory_order_release);
      }

      /// \brief unlock the lock, but don't check if the thread has lock ownership
      void _unlock()
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
        if (!lock_flag.test())
        {
          printf("[spinlock: invalid unlock detected in unlock() (unlocking an unlocked mutex)]\n");
          spinlock_internal::print_backtrace();
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
          spinlock_internal::print_backtrace();
          abort();
        }
#endif
        while (lock_flag.test(std::memory_order_acquire))
        {
          while (_relaxed_test());
        }
      }

      [[nodiscard]] bool _get_state() const
      {
#if N_ENABLE_LOCK_DEBUG
        check_for_key();
#endif
        return lock_flag.test(std::memory_order_acquire);
      }

      [[nodiscard]] bool _relaxed_test() const
      {
        return lock_flag.test(std::memory_order_relaxed);
      }

    public: // debug API
#if N_ENABLE_LOCK_DEBUG
      [[nodiscard]] bool _debug_is_lock_held_by_current_thread() const
      {
        return _get_state() && (owner_id == std::this_thread::get_id());
      }
#endif
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
          spinlock_internal::print_backtrace();
          abort();
        }
        if (key != k_key_value)
        {
          printf("[spinlock: invalid lock: memory area is not a lock]\n");
          spinlock_internal::print_backtrace();
          abort();
        }

      }
#endif
  };

  /// \brief A simple shared spinlock that favor writers
  class shared_spinlock
  {
    public:
      [[nodiscard]] bool try_lock_exclusive(bool wait_shared = true)
      {
        if (!exclusive_lock.try_lock())
        {
          return false;
        }

        if (shared_count.load(std::memory_order_seq_cst) != 0)
        {
          if (wait_shared)
          {
            do
            {
              while (shared_count.load(std::memory_order_relaxed) != 0);
            }
            while (shared_count.load(std::memory_order_acquire) != 0);
          }
          else
          {
            exclusive_lock.unlock();
            return false;
          }
        }
        return true;
      }

      void lock_exclusive()
      {
        exclusive_lock.lock();

        // wait for the shared lock to be released:
        if (shared_count.load(std::memory_order_seq_cst) != 0)
        {
          do
          {
            while (shared_count.load(std::memory_order_relaxed) != 0);
          }
          while (shared_count.load(std::memory_order_acquire) != 0);
        }
      }

      void unlock_exclusive()
      {
        exclusive_lock.unlock();
      }

      /// \brief Migrate the lock from exclusive to shared
      /// \note Guaranteed that no-one will grab an exclusive lock in between
      void lock_shared_unlock_exclusive()
      {
        // increment the shared lock:
        shared_count.fetch_add(1, std::memory_order_acquire);
        // release the exclusive lock:
        unlock_exclusive();
      }

      /// \brief Migrate the lock from shared to exclusive
      /// \note There is no guarantee that someone will not steal the exclusive lock first
      /// \return Indicate whether the operation was atomic or if someone acquired the lock before us.
      ///         If the function returns false, a repeat of the operation done when the shared lock was held is probably a good idea
      [[nodiscard]] bool lock_exclusive_unlock_shared()
      {
        if (!exclusive_lock.try_lock())
        {
          // Handle the case where someone already has the exclusive lock
          // we have to realse our shared lock here, as the one with the exclusive lock is waiting for us
          unlock_shared();
          lock_exclusive();
          // we did have to release the shared lock and wait for the exclusive lock
          return false;
        }

        // we have the exclusive lock, release our shared lock
        unlock_shared();

        // and wait for the remaining shared locks to be released
        if (shared_count.load(std::memory_order_seq_cst) != 0)
        {
          do
          {
            while (shared_count.load(std::memory_order_relaxed) != 0);
          }
          while (shared_count.load(std::memory_order_acquire) != 0);
        }

        // we got the exclusive lock in a single atomic operation
        return true;
      }

      bool try_lock_shared()
      {
        if (exclusive_lock._get_state())
          return false;
        shared_count.fetch_add(1, std::memory_order_acquire);
        if (exclusive_lock._get_state())
        {
          shared_count.fetch_sub(1, std::memory_order_release);
          return false;
        }
        return true;
      }

      void lock_shared()
      {
        // wait for the exclusive lock to be release:
        exclusive_lock._wait_for_lock();

        // try to increment the shared lock:
        shared_count.fetch_add(1, std::memory_order_acquire);
        if (exclusive_lock._get_state())
        {
          shared_count.fetch_sub(1, std::memory_order_release);

          // we got a race condition on the lock, so we release the shared lock and try again:
          // this is not recursion.
          return lock_shared();
        }
      }

      void unlock_shared()
      {
        [[maybe_unused]] const int32_t ret = shared_count.fetch_sub(1, std::memory_order_release);
#if N_ENABLE_LOCK_DEBUG
        if (ret < 0)
        {
          printf("[shared_spinlock: invalid unlock: double/invalid shared unlock detected]\n");
          spinlock_internal::print_backtrace();
          abort();
        }
#endif
      }

      /// \brief Return the state of the exclusive lock
      /// \note May also indicate that a thread is waiting to acquire the lock
      bool _get_exclusive_state() const { return exclusive_lock._get_state(); }
      bool _get_shared_state() const { return shared_count.load(std::memory_order_acquire) > 0; }
#if N_ENABLE_LOCK_DEBUG
      [[nodiscard]] bool _debug_is_exclusive_lock_held_by_current_thread() const
      {
        return exclusive_lock._debug_is_lock_held_by_current_thread();
      }
#endif
    private:
      spinlock exclusive_lock;
      std::atomic<int32_t> shared_count = 0;
  };

  /// \brief adapter for lock_guard (exclusive version)
  class spinlock_exclusive_adapter
  {
    public:

      template<typename T>
      struct adapter
      {
        void lock() { reinterpret_cast<T*>(this)->lock_exclusive(); }
        bool try_lock() { return reinterpret_cast<T*>(this)->try_lock_exclusive(); }
        void unlock() { reinterpret_cast<T*>(this)->unlock_exclusive(); }
      };

      template<typename T>
      static auto& adapt(T& sl) { return reinterpret_cast<spinlock_exclusive_adapter::adapter<T>&>(sl); }
  };

  /// \brief adapter for lock_guard (shared version)
  class spinlock_shared_adapter
  {
    public:
      template<typename T>
      struct adapter
      {
        void lock() { reinterpret_cast<T*>(this)->lock_shared(); }
        bool try_lock() { return reinterpret_cast<T*>(this)->try_lock_shared(); }
        void unlock() { reinterpret_cast<T*>(this)->unlock_shared(); }
      };

      template<typename T>
      static auto& adapt(T& sl) { return reinterpret_cast<spinlock_shared_adapter::adapter<T>&>(sl); }
  };

  /// \brief migrate a shared lock to an exclusive lock. The unlock return the lock to shared in an atomic operation
  /// \note please assume the lock operation wasn't atomic (there is sadlly no real way to provide the atomicity of the operation)
  class spinlock_shared_to_exclusive_adapter
  {
    public:
      template<typename T>
      struct adapter
      {
        void lock() { [[maybe_unused]] const bool _ = reinterpret_cast<T*>(this)->lock_exclusive_unlock_shared(); }
        void unlock() { reinterpret_cast<T*>(this)->lock_shared_unlock_exclusive(); }
      };

      template<typename T>
      static auto& adapt(T& sl) { return reinterpret_cast<spinlock_shared_to_exclusive_adapter::adapter<T>&>(sl); }
  };

} // namespace neam
