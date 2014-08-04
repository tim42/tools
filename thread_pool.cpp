
#include <iostream>
#include "thread_pool.hpp"

// thanks to: https://github.com/progschj/ThreadPool/
namespace neam
{
  // the constructor just launches some amount of workers
  thread_pool::thread_pool(size_t threads)
    : queue_mutex(), condition(), stop(!threads), not_working_counter(0)
  {
    for (size_t i = 0; i < threads; ++i)
    {
      workers.emplace_back([this, i]()
      {
        while (true)
        {
          std::unique_lock<std::mutex> lock(this->queue_mutex);
          ++not_working_counter;
          while (!this->stop && this->tasks.empty())
            this->condition.wait(lock);
          --not_working_counter;

          if (this->stop && this->tasks.empty())
            return;

          auto task = this->tasks.front();
          this->tasks.pop_front();
          lock.unlock();
          try
          {
            task();
          }
          catch(std::exception &e)
          {
            std::cerr << "neam::thread_pool: exception catched: " << e.what() << std::endl;
          }
          catch(...)
          {
            std::cerr << "neam::thread_pool: exception catched will running the f*cking task." << std::endl;
          } // discard exceptions
        }
      });
    }
  }

  // add new work item to the pool
  void thread_pool::enqueue(const fncw::function &fnc)
  {
    // don't allow enqueueing after stopping the pool
    if (stop)
      throw std::runtime_error("enqueue on stopped ThreadPool");

    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.push_back(fnc);
    condition.notify_one();
    }
    return;
  }

  // the destructor joins all threads
  thread_pool::~thread_pool()
  {
    _kill();
  }

  void thread_pool::_kill()
  {
    try
    {
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
          return;
        stop = true;
      }
      condition.notify_all();
      for (size_t i = 0; i < workers.size(); ++i)
      {
        workers[i].join();
      }
    }
    catch (...)
    {
    }
    workers.clear();
    tasks.clear();
  }

  size_t thread_pool::get_task_number()
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
  }

} // namespace neam
