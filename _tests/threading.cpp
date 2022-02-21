
// Not the test for that. It's done in the threading test
#define N_ASYNC_USE_TASK_MANAGER true
#include "../async/async.hpp"
#include "../threading/task_manager.hpp"
#include "../threading/utilities.hpp"

#include "../id/string_id.hpp" // for _rid

#include "../logger/logger.hpp"
#include "../chrono.hpp"

constexpr size_t frame_count = 1000000;
constexpr size_t thread_count = 7;


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
      recurse(tm, {i}, false).then([ o = std::move(o), st = std::move(st)](move_only_thingy&&) mutable
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
  cr::out.min_severity = neam::cr::logger::severity::debug;
  cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

  threading::task_manager tm;
  {
    neam::threading::task_group_dependency_tree tgd;
    tgd.add_task_group("init-group"_rid, "init-group");
    tgd.add_task_group("async-group"_rid, "async-group");
    tgd.add_task_group("for_each-group"_rid, "for_each-group");

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

    tm.add_compiled_frame_operations(std::move(tree));
  }

  unsigned frame_index = 0;

  // spawn threads:
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

  // add some setup:
  cr::chrono chr;
  tm.set_start_task_group_callback("init-group"_rid, [&frame_index, chr]
  {
    ++frame_index;
    static unsigned old_pct = 0;
    unsigned pct = (frame_index * 100 / frame_count);

    const double dt = chr.get_accumulated_time();
    const double ms_frame = dt * 1000 / frame_index;

    if (pct % 10 == 0 && old_pct != pct)
    {
      old_pct = pct;

      neam::cr::out().log(" progress: {}% | [{} frames | {:.6}ms/frame]", pct, frame_index, ms_frame);
    }
    if (frame_index % 100 == 0)
    {
      neam::cr::out().debug(" progress: {}% | [{} frames | {:.6}ms/frame]", pct, frame_index, ms_frame);
    }
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
        threading::task& init_a = *tm.get_task([&]
        {
          src_array.resize(300);
        });
        threading::task& init_b = *tm.get_task([&]
        {
          out_data.resize(16 * 1024);
        });

        tm.actively_wait_for(init_a);
        tm.actively_wait_for(init_b);
      }
return;
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
          threading::for_each(tm, gid, out_data, [&e](uint32_t& sub_e, size_t idx)
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

  // run everything...
  for (size_t i = 0; frame_index < frame_count; ++i)
  {
    tm.wait_for_a_task();
    tm.run_a_task();
  }

  // wait for the threads to endL
  for (auto& it : thr)
  {
    if (it.joinable())
      it.join();
  }

  return 0;
}
