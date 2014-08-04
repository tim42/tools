//
// file : thread_pool.hpp
// in : file:///home/tim/projects/pfa/chochana/src/tool/thread_pool.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 01/07/2013 21:32:00
//

#ifndef __THREAD_POOL_HPP__
# define __THREAD_POOL_HPP__


#include <vector>
#include <list>

#include <chrono>
#include <atomic>
#include <stdexcept>

// thanks to: https://github.com/progschj/ThreadPool/
// this version is simplified because of the use of fncw::function
namespace neam
{

  class thread_pool
  {
    public:
      /// \brief create the thread pool
      /// \param[in] threads is the number of threads the thread pool will have
      thread_pool(size_t threads);

      /// \brief destruct the thread pool.
      /// \note wait for all the threads
      /// \see _kill()
      ~thread_pool();

      /// \brief push a function to the thread pool
      void enqueue(const fncw::function &fnc);

      /// \brief return the numbre of thread in the thread pool
      size_t get_thread_number() const
      {
        return workers.size();
      }

      /// \brief return the number of working threads
      size_t get_working_thread_number() const
      {
        return workers.size() - not_working_counter;
      }

      /// \brief is the thread pool stopped ?
      bool is_stopped() const
      {
        return stop;
      }

      /// \brief return the number of tasks enqueued in the tp.
      size_t get_task_number();

      /// \brief definitively kill the thread pool.
      /// wait for all the threads to end their tasks.
      /// used when the thread_pool is emebeded in another class and extra things remain to do *after* the thread_pool destruction.
      /// \attention \b marked \b as \b advanced.
      void _kill();

    private:
      // need to keep track of threads so we can join them
      std::vector< std::thread > workers;
      // the task queue
      std::list< fncw::function > tasks;


      // synchronization
      std::mutex queue_mutex;
      std::condition_variable condition;
      std::atomic<bool> stop;

      std::atomic<size_t> not_working_counter;
  };
} // namespace neam

#endif /*__THREAD_POOL_HPP__*/
