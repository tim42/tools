
#include <filesystem>

#include "../io/io.hpp"

#include "../logger/logger.hpp"
#include "../cmdline/cmdline.hpp"
#include "../chrono.hpp"

using namespace neam;

struct global_options
{
  // options
  bool keep_files = false;

  // positional params:
  std::vector<std::string_view> parameters;
};
N_METADATA_STRUCT(global_options)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(keep_files)
  >;
};


int main(int argc, char**argv)
{
  cr::out.min_severity = neam::cr::logger::severity::debug;
  cr::out.register_callback(neam::cr::print_log_to_console, nullptr);

  cmdline::parse cmd(argc, argv);
  bool success = false;
  global_options gbl_opt = cmd.process<global_options>(success, 0);

  if (!success)
  {
    // output the different options and exit:
    cr::out().log("possible options:");
    cmdline::arg_struct<global_options>::print_options();
    return 1;
  }


  std::filesystem::path current_binary = argv[0];
  std::filesystem::path temp_files_path = std::filesystem::canonical(current_binary).parent_path()/"temp_files";
  std::filesystem::create_directory(temp_files_path);

  std::vector<std::string> files_to_remove;
  {
    io::context ctx;

    ctx.set_prefix_directory(temp_files_path);

    // create some files:
    const auto map_file = [&](const std::string& file) { files_to_remove.push_back(file); return ctx.map_file(file); };
    const neam::id_t file_1 = map_file("file_1.txt");
    const neam::id_t file_2 = map_file("file_2.txt");
    const neam::id_t file_3 = map_file("file_3.txt");
    const neam::id_t file_4 = map_file("file_4.txt");

    // initial data:
    ctx.queue_write(file_1, 0, raw_data::allocate_from(std::string("FILE_1 CONTENT:\n")));
    ctx.queue_write(file_2, 0, raw_data::allocate_from(std::string("FILE_2 CONTENT:\n")));

    // file_1: simply queue all write operations at the same time:
    // (will effectively issue a single write as they are all append writes)
    // ((Note, this is quite slow, as there is a lot of small bit of data scattering everywhere in memory)
    constexpr unsigned k_entry_count = 1000000;
    {
      const std::string some_data("more and more data!\n");

      for (unsigned i = 0; i < k_entry_count; ++i)
      {
        ctx.queue_write(file_1, io::context::append, raw_data::allocate_from(some_data));
      }
    }

    cr::out().log("writing file_1...");
    ctx._wait_for_submit_queries();

    // file_2: queue each write after the previous is completed:
    // much lower than file_1 as each write wait for the previous write to end
    // NOTE: there is no recursion in there. All calls are flattened by async::chain
    std::function<io::context::write_chain()> file_2_queue_write; // must live during the _wait_for_submit_queries
    {
      unsigned i = 0;
      file_2_queue_write = [&]
      {
        if (i++ >= k_entry_count) return io::context::write_chain::create_and_complete(true);

        std::string some_data("more and more data!\n");
        return ctx.queue_write(file_2, io::context::append, raw_data::allocate_from(some_data))
        .then([&](bool)
        {
          // follow-up with the next write:
          return file_2_queue_write();
        });
      };

      // queue all the write operations
      file_2_queue_write()
      .then([&](bool)
      {
        // last write:
        ctx.queue_write(file_2, io::context::append, raw_data::allocate_from(std::string("[last operation !]\n")));
      });
    }

    cr::out().log("writing file_2...");
    ctx._wait_for_submit_queries();

    // file_3: do one big read/write op
    ctx.queue_read(file_1, 0, io::context::whole_file)
    .then([=, &ctx](raw_data&& data, bool /*success*/)
    {
      return ctx.queue_write(file_3, 0, std::move(data));
    })
    .then([&](bool) // instead of nesting async which is recursive and ugly,
                   // returning a chain flattens the calls
    {
      // file_3 contains all of file_1 data

      ctx.queue_write(file_3, io::context::append, raw_data::allocate_from(std::string("[last file_3 operation !]\n")));
    });

    cr::out().log("writing file_3...");
    ctx._wait_for_submit_queries();

    // file_4: we use the scatter/gather rw ops + read chunks at a time
    // this would be the normal read-a-chunk->write_a_chunk, except we use scatter/gather async iops
    // This is the best option for large amount of data (if the constants are tweaked a bit)
    std::function<io::context::read_chain(raw_data&& data, size_t)> file_4_queue_write_read; // must live during the _wait_for_submit_queries
    {
      // we don't have much data, so in order to have more reads/write, we use a smaller chunk size
      // the actual read at the filesystem level is k_chunk_size * k_chunk_to_read bytes,
      //   but split into k_chunk_to_read different buffers of k_chunk_size bytes
      constexpr unsigned k_chunk_size = 1024;
      constexpr unsigned k_chunk_to_read = 8;

      const size_t file_2_size = ctx.get_file_size(file_2);
      size_t current_offset = 0;

      // recursive-like function that write-then-read chunks
      file_4_queue_write_read = [&](raw_data&& data, size_t write_offset)
      {
        // write the data:
        ctx.queue_write(file_4, write_offset, std::move(data));

        // queue a follow-up read if there's still data to read:
        if (current_offset >= file_2_size)
          return io::context::read_chain::create_and_complete({/* no data */}, true); // done !

        io::context::read_chain ret = ctx.queue_read(file_2, current_offset, k_chunk_size)
        .then([&, read_offset = current_offset](raw_data&& data, bool /*success*/)
        {
          // not a recursive call, as it is flattened by the async_chain and _wait_for_submit_queries
          return file_4_queue_write_read(std::move(data), read_offset);
        });
        current_offset += k_chunk_size;
        return ret;
      };

      // read the first k_chunk_to_read chunks
      // io::context will recognise that they are contiguous and will issue a single read operation
      // but it will still call the k_chunk_to_read callbacks (sorted in the offset order).
      // each of those callbacks will then queue a write + folow-up read, which will still result in a single write and a single read
      // but the benefit VS a "standard" file copy is that the read is done at the same time as the write
      std::vector<async::continuation_chain> pending_ops;
      for (unsigned i = 0; i < k_chunk_to_read; ++i)
      {
        io::context::read_chain chain = ctx.queue_read(file_2, current_offset, k_chunk_size)
          .then([&, read_offset = current_offset](raw_data&& data, bool /*success*/)
          {
            return file_4_queue_write_read(std::move(data), read_offset);
          });
        current_offset += k_chunk_size;

        pending_ops.push_back(chain.to_continuation());
      }

      async::multi_chain(pending_ops).then([&]
      {
        // file_4 is now the copy of file_2.

        ctx.queue_write(file_4, io::context::append, raw_data::allocate_from(std::string("[last file_4 operation !]\n")));
      });
    }

    cr::out().log("writing file_4...");
    ctx._wait_for_submit_queries();
  }

  if (gbl_opt.keep_files)
    return 0;

  // cleanup
  cr::out().log("cleaning-up {} files...", files_to_remove.size());

  for (auto& it : files_to_remove)
  {
    std::filesystem::remove(temp_files_path / it);
  }
  std::filesystem::remove(temp_files_path);

  return 0;
}

