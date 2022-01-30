
# neam/tools

A common set of tools and utilities used on most (if not all) of my C++ projects.

The scale and type ranges from a simple backtrace-printing function to a fully asynchronous IO or a task-manager for multi-threaded projects.

# list of tools

## ASYNC

A variation on the promise/future concept (or the coroutines maybe) but easy to understand and use.
The asynchrounous IO make extensive use the the async utility.

A more complex example usage: (from hydra's resource system)

```
  io::context::read_chain context::read_raw_resource(id_t rid)
  {
    // ...
    return io_context.queue_read(pack_file, offset, io::context::whole_file)
           .then([rid](raw_data && data, bool success)
    {
      // [do stuff]...

      return io::context::read_chain::create_and_complete(std::move(data), success);
    })
    .then([](raw_data&& data, bool success)
    {
      // uncompress() is also an asynchronous operation, but does not returns/forward a success marker
      // so we chain an operation to correctly forward both the success marker and the uncompressed data
      // the return automatically chains with the final operation
      return uncompress(std::move(data))
             .then([success](raw_data && data)
      {
        return io::context::read_chain::create_and_complete(std::move(data), success);
      });
    });
  }
```

## IO

A fully/truly asynchrounous IO manager based on liburing and async.
(could be made to use something other than liburing, but currently I don't have a dev mahcine with anything other than linux).

See the Async example for what can be done.

## DEBUG

A set of assertions / checks which are mutlithread-aware and somewhat efficient.

Also provide openCL, unix and vulkan error handling capabilities (in an extensible way)

```
  check::debug::n_assert(some_stuff != some_other_stuff, "Both some_stuff and some_other_stuff are equal (value: {}).", some_stuff);
```

```
  // Will output the error name, description and callstack
  check::on_vulkan_error::n_assert_success(dev._vkCreateDescriptorPool(&create_info, nullptr, &vk_dpool));
```

## LOGGER

Logger facility using either `libfmt` or `std::format` (mostly `libfmt` as `std::format` is not availlable on my system)
Can be made to ensure that no other thread will interrupt multiple output operations.
Also provide output-handler capabilities (for writing both on stdout and in a file, or forwarding the output via network)

```
  neam::cr::out().log("Hello {}", "World");
```

## CMDLINE

Tired of writing cmdline parsing stuff or having crappy args handling?

```
struct global_options
{
  // options
  bool force = false;
  bool verbose = false;
  bool silent = false;

  std::vector<std::string_view> parameters;
};

N_METADATA_STRUCT(global_options)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(force),
    N_MEMBER_DEF(verbose),
    N_MEMBER_DEF(silent)
  >;
};

int main(int argc, char **argv)
{
  cmdline::parse cmd(argc, argv);
  bool success;
  global_options gbl_opt = cmd.process<global_options>(success);

  if (!success)
  {
    // output the different options and exit:
    cmdline::arg_struct<global_options>::print_options();
    return 1;
  }

  // Do stuff:
}
```

## RLE

Previously persistence, now a fully C++23 RLE encoder with much less code and optional version handling (including automatic upgrades) capabilities.

## Threading

A nice, simple to use task-manager tailored for game-engines.
It provide AOT task-group dependency compilation and can handle independent execution of multiple task-groups.

Task can also have dependencies with other tasks from the same task-group.

```
template<StlIndexableContainer T, typename Func>
void for_each(neam::threading::task_manager& tm, neam::threading::group_t group, T&& array, Func&& func, size_t entry_per_task = 256)
{
  const size_t size = array.size();

  // Used to synchronise all the tasks to a single point:
  neam::threading::task_wrapper final_task = tm.get_task(group, []() {});

  std::atomic<size_t> index = 0;

  // inner for-each function
  std::function<void()> inner_for_each;
  inner_for_each = [&tm, &array, &index, &func, &inner_for_each, &final_task = *final_task, size, entry_per_task, group]()
  {
    const size_t base_index = index.fetch_add(entry_per_task, std::memory_order_acquire);
    for (size_t i = 0; i < entry_per_task && base_index + i < size; ++i)
    {
      func(array[base_index + i]);
    }

    // Add a new task if necessary:
    if (index < size)
    {
      auto task = tm.get_task(group, std::function<void()>(inner_for_each));
      final_task.add_dependency_to(task);
    }
  };

  // avoid spending too much time by creating too many tasks
  // the additional tasks will be created on a case-by-case basis on different threads
  const size_t max_task_count = (std::thread::hardware_concurrency() + 2) * 2;
  size_t dispatch_count = size / entry_per_task;
  if (dispatch_count > max_task_count)
    dispatch_count = max_task_count;

  // dispatch the initial tasks:
  for (size_t i = 0; i < dispatch_count; ++i)
  {
    auto task = tm.get_task(group, std::function<void()>(inner_for_each));
    final_task->add_dependency_to(task);
  }

  // Prevent leaving the function before final_task is done,
  // thus keeping alive all the variables that are in the lambda capture
  tm.actively_wait_for(*final_task);
}
```

## Other:

 - a compile-time FNV 1A hash function for strings (in `hash`, extended in `id`)
 - a compile-time type/value-hash / type/value-to-compile-time-string conversion (`type_id.hpp`)
 - a full type-list handling library (including merge, flatten, map, filter, find, ...) (`ct_list.hpp`)
 - some memory allocators for different use-cases (`memory_allocator.hpp`, `memory_pool.hpp`, `frame_allocation.hpp`, `ring_buffer.hpp`)
 - a spinlock with extra debug capabilities (`spinlock.hpp`)
 - a generic way to provide struct metadata (used by `rle` and `cmdline`) (`struct_metadata`)
 - ...

## author
Timothée Feuillet (_neam_ or _tim42_).
