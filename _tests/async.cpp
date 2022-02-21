
// Not the test for that. It's done in the threading test
#define N_ASYNC_USE_TASK_MANAGER false
#include "../async/async.hpp"

#include "../logger/logger.hpp"
#include "../chrono.hpp"

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

move_only_thingy my_function(move_only_thingy v)
{
  return { v.i + 1 };
}

using mo_chain = async::chain < move_only_thingy && >;
using moc_chain = async::chain < move_only_thingy &&, size_t& >;

moc_chain recurse(move_only_thingy&& o, size_t& counter, bool second_branch)
{
  ++counter;

  if (o.i >= 4096) // it will call around (limit + 1) * (limit + 1) the recurse function
  {
    if (second_branch)
      return moc_chain::create_and_complete(std::move(o), counter);
    else
      return moc_chain::create_and_complete({ o.i - 1}, counter);
  }

  o.i += 1;

  moc_chain ch;

  if (!second_branch)
  {
    recurse({o.i}, counter, true).then([st = ch.create_state(), o = std::move(o)](move_only_thingy&&, size_t& counter) mutable
    {
      st.complete(std::move(o), counter);
    });
  }
  else
  {
    ch.create_state().complete(std::move(o), counter);
  }

  return ch.then([second_branch](move_only_thingy&& o, size_t& counter)
  {
    return recurse(std::move(o), counter, second_branch);
  })
  .then([](move_only_thingy&& o, size_t& counter)
  {
    return recurse(std::move(o), counter, true);
  });
}

int main(int, char**)
{
  cr::out.min_severity = neam::cr::logger::severity::debug;
  cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

  mo_chain ch;
  mo_chain::state st = ch.create_state();

  auto cont = ch.then(my_function)
    .then(my_function)
    .then(my_function);

  st.complete({0});

  int val = 0;
  cont.then(my_function)
  .then([&](move_only_thingy&& o) { val = o.i; });

  cr::out().log("result: {} (expected: 4)", val);

  size_t counter = 0;
  cr::chrono chr;

  recurse({0}, counter, false).then([&](move_only_thingy&& o, size_t& counter)
  {
    double dt = chr.delta();
    cr::out().log("recurse result: total recurse calls: {} (depth: {})", counter, o.i);
    cr::out().log("              : total time: {:.6}ms ({:.3}us/call)", dt * 1000, dt * 1000000 / counter);
  });

  return 0;
}
