
// Not the test for that. It's done in the threading test
#define N_ASYNC_USE_TASK_MANAGER true
#include "../async/async.hpp"
#include "../threading/task_manager.hpp"
#include "../threading/utilities.hpp"

#include "../id/string_id.hpp" // for _rid

#include "../logger/logger.hpp"
#include "../chrono.hpp"

constexpr size_t frame_count = 10000;
constexpr size_t thread_count = 6;


using namespace neam;

struct move_only_thingy
{
  move_only_thingy() = delete;
  move_only_thingy(const move_only_thingy& o) = delete;
  move_only_thingy& operator = (const move_only_thingy& o) = delete;

  move_only_thingy(int _i) : i(_i) {};
  move_only_thingy(move_only_thingy&& o) : i(o.i) { o.i = -1; };
  move_only_thingy& operator = (move_only_thingy&& o)
  {
    if (this == &o) return *this;
    i = o.i;
    o.i = -1;
    return *this;
  };
  ~move_only_thingy() { i = -1; }

  int i = -1;
};


using mo_chain = async::chain < move_only_thingy && >;

// This is simply a huge stress-test for the task-manager and async::chain
// it does the following operations:
// recurse(false, n)  ..                        ..  (task)recurse(false, n + 1) --then-> (task)recurse(true, n + 1)   [ n < 6 ]
//   \-> (task)[ recurse(false, n + 1) --then--> async-trigger ^ ]
//
// recurse(true, n)  ..  (task)recurse(true, n + 1) --then-> (task)recurse(true, n + 1)    [ n < 256 ]
//   \-> (task)[ async-trigger ^ ]
//
mo_chain recurse(threading::task_manager& tm, move_only_thingy&& o, bool second_branch)
{
  if (o.i >= (second_branch ? 256 : 6))
  {
    if (second_branch)
      return mo_chain::create_and_complete(std::move(o));
    else
      return mo_chain::create_and_complete({ o.i - 1});
  }

  o.i += 1;

  mo_chain ch;

  tm.get_task([st = ch.create_state(), o = std::move(o), &tm, second_branch] mutable
  {
    TRACY_SCOPED_ZONE_COLOR(0x7FFF7F);
    auto i = o.i + 1;
    if (!second_branch)
    {
      recurse(tm, {i}, false)
      .then([ o = std::move(o), st = std::move(st)](move_only_thingy&&) mutable
      {
        st.complete(std::move(o));
      });
    }
    else
    {
      st.complete(std::move(o));
    }
  });

  return ch.then(tm, [&tm, second_branch](move_only_thingy&& o) -> mo_chain
  {
    TRACY_SCOPED_ZONE_COLOR(0xFF7F00);
    return recurse(tm, std::move(o), second_branch);
  })
  .then(tm, [&tm](move_only_thingy&& o) -> mo_chain
  {
    TRACY_SCOPED_ZONE_COLOR(0x00FF7F);
    return recurse(tm, std::move(o), true);
  });
}


int main(int, char**)
{
  cr:: get_global_logger().min_severity = neam::cr::logger::severity::debug;
  cr:: get_global_logger().register_callback(neam::cr::print_log_to_console, nullptr);

  threading::task_manager tm;
  {
    neam::threading::task_group_dependency_tree tgd;
    tgd.add_task_group("init-group"_rid);
    tgd.add_task_group("async-group"_rid);
    tgd.add_task_group("for_each-group"_rid);

    // async and for_each bot depend on init
    // It will generate something like:
    //
    //        async
    // init <
    //        for_each
    //
    tgd.add_dependency("async-group"_rid, "init-group"_rid);
    tgd.add_dependency("for_each-group"_rid, "init-group"_rid);

    auto tree = tgd.compile_tree();
    tree.print_debug();

    tm.add_compiled_frame_operations(std::move(tree), {});
  }

  unsigned frame_index = 0;

  // spawn threads:
  neam::cr::out().log("Spawning {} threads...", thread_count);
  std::deque<std::thread> thr;
  for (unsigned i = 0; i < thread_count; ++i)
  {
    thr.emplace_back([&frame_index, &tm]
    {
      TRACY_NAME_THREAD("Worker");
      while (frame_index < frame_count)
      {
        tm.wait_for_a_task();
        tm.run_a_task();
      }
    });
  }

  neam::cr::out().log("Setting up task manager...");

  // add some setup:
  cr::chrono chr;
  tm.set_start_task_group_callback("init-group"_rid, [&frame_index, chr]
  {
    ++frame_index;
    static unsigned old_pct = 0;
    static double last_dt = 0;
    static unsigned frame_count_since_last_print = 0;


    unsigned pct = (frame_index * 100 / frame_count);

    const double dt = chr.get_accumulated_time();
    const double ms_frame = dt * 1000 / frame_index;

    if (pct % 10 == 0 && old_pct != pct)
    {
      old_pct = pct;

      neam::cr::out().log(" progress: {}% | [{} frames | {:.6}ms/frame]", pct, frame_index, ms_frame);
    }
    if (last_dt + 2 <= dt) // log every seconds
    {
      const double ddt = dt - last_dt;
      const double imm_ms_frame = ddt * 1000 / frame_count_since_last_print;
      last_dt = dt;
      frame_count_since_last_print = 0;
      neam::cr::out().debug(" progress: {}% | [{} frames | {:.6}ms/frame]", pct, frame_index, imm_ms_frame);
    }

    ++frame_count_since_last_print;
  });

  // run a simple for-each over a large collection of element:
  tm.set_start_task_group_callback("for_each-group"_rid, [&tm]
  {
    tm.get_task("for_each-group"_rid, [&tm] mutable
    {
      std::vector<uint32_t> src_array;
      std::vector<uint32_t> out_data;

      // launch tasks for the allocation/init as it can be quite slow
      {
        auto init_a = tm.get_task([&]
        {
          src_array.resize(300);
        }).create_completion_marker();
        auto init_b = tm.get_task([&]
        {
          out_data.resize(32 * 1024);
        }).create_completion_marker();

        tm.actively_wait_for(std::move(init_a));
        tm.actively_wait_for(std::move(init_b));
      }

      // init the array:
      const threading::group_t gid = tm.get_current_group();
      threading::for_each(tm, gid, src_array, [](uint32_t& e, size_t idx)
      {
        // some random value: (invwk)
        e = (idx + (idx * idx | 5)) >> 32;
      }, 256);

      // do some weird stuff:
      threading::for_each(tm, gid, out_data, [&](uint32_t& e, size_t idx)
      {
        e = idx;
        // you can do a for-each in a for-each
        // It's NOT a good idea as it does have bad/slower outcomes
        // (a thread recursively dispatching all the tasks, then stalling every other threads),
        // but it's possible to have nested disptaches.
        if (e % 512 == 0)
        {
          threading::for_each(tm, gid, out_data, [&e](uint32_t& sub_e, size_t /*idx*/)
          {
            e += sub_e;
          }, 64);
        }
      }, 256);
    });
  });

  // some async tasks (using async::chain)
  tm.set_start_task_group_callback("async-group"_rid, [&tm]
  {
    recurse(tm, { 0 }, false);
  });

  neam::cr::out().log("Enrolling main thread");

  // "Spin" the task manager
  tm._advance_state();

  // run everything...
  for (size_t i = 0; frame_index < frame_count; ++i)
  {
    tm.wait_for_a_task();
    tm.run_a_task();
  }

  // wait for the threads to end
  neam::cr::out().log("Waiting for threads...");
  for (auto& it : thr)
  {
    if (it.joinable())
      it.join();
  }

  // properly end the task manager:
  // (destructing it while there's pending tasks or when not in a frame boundary is incorrect)
  // tm.request_stop is called while the whole task manager is stalled at a frame boundary.
  // Not unlocking the task manager in the request_stop callback would require to call _advance_state to get operations back to normal
  // We should also run the remaning long duration taks, if there's any.
  //
  // NOTE: In this sample, we exit the threads before performing the stall, but that's not always possible. (a thread can be stuck in wait_for_a_task).
  //       To unstuck the thread:  tm.should_threads_exit_wait(true); (It only works when the task manager is stalled)
  bool exit_loop = false;
  tm.request_stop([&] { exit_loop = true; }, true);
  while (!exit_loop)
    tm.run_a_task();
  tm.get_frame_lock()._unlock();

  return 0;
}
