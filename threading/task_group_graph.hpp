//
// created by : Timothée Feuillet
// date: 2022-1-2
//
//
// Copyright (c) 2022 Timothée Feuillet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include <set>
#include <vector>
#include <map>

#include "types.hpp"
#include "../id/string_id.hpp"

namespace neam::threading
{
  struct ir_opcode
  {
    enum opcode_type : uint16_t
    {
      declare_chain_index = 6,

      // NOTE: only one chain can have an execute_task_group for a given group
      execute_task_group = 16,
      // NOTE: unlike execute_task_group, as many chains as necessary can have a wait_task_group
      wait_task_group = 17,

      end_chain = 18,
    };

    opcode_type opcode;

    // execution chain is a set of linear dependencies
    // something like:
    //    exec group[1]
    //    exec group[2]
    //    wait group[1]
    //    exec group[3]
    //    wait group[3]
    //    wait group[4]
    //    exec group[5]
    //    exec group[6]
    //    ...
    //    end_chain chain[1]
    //    wait group[2]
    //    exec group[4]
    //    end_chain chain[2]
    // will produce the following:
    //    chain 1: group[1],group[2] -> group[3] + -> group[5],group[6]  ...
    //    chain 2:                  \->  group[4] /
    // where group[1] and group[3] can execute parallely to group[2] and group[4]
    // (so as to avoid contention on a specific group).
    // and is the result of the following calls:
    //  add_group(1);
    //  add_group(2);
    //  add_group(3);
    //  add_group(4);
    //  add_group(5);
    //  add_group(6);
    //  add_dependency(5, 3);
    //  add_dependency(5, 4);
    //  add_dependency(6, 3);
    //  add_dependency(6, 4);
    //  add_dependency(4, 2);
    //  add_dependency(3, 1);

    // a value of 0 is the invalid task_group/chain
    uint16_t arg = 0;

    static_assert(sizeof(arg) >= sizeof(group_t));
  };

  struct group_configuration
  {
    // Indicate whether that task-group is restricted on (a) specific thread(s)
    // If not id_t::none, only threads with that name will be able to run those tasks
    // If id_t::none, only untagged threads and named threads with can_run_other_tasks to true will be able to run those tasks
    // NOTE: Because there's only one thread (maybe more) that will participate on those tasks, special care should be taken to not make that group a bottleneck.
    id_t restrict_to_named_thread = id_t::none;
  };

  /// \brief Representation of the dependency graph that can be passed to the task-manager.
  struct resolved_graph
  {
    std::map<id_t, group_t> groups;
    uint32_t chain_count;
    std::vector<ir_opcode> opcodes;
    std::map<group_t, std::string> debug_names;
    std::map<group_t, group_configuration> configuration;

    void print_debug() const;
  };
  
  /// \brief class that takes the task-group dependency tree and outputs IR
  /// \note Must be a tree, not a graph
  class task_group_dependency_tree
  {
    public:
      /// \brief Add a task group. Cannot be task::k_non_transient_task_group.
      group_t add_task_group(string_id id, group_configuration conf = {});

      /// \brief makes \p group dependent on \p dependency
      /// (\p group will not execute before \p dependency has completed all its tasks)
      void add_dependency(group_t group, group_t dependency);

      void add_dependency(id_t group, id_t dependency);

      /// \brief Return the group for a given id
      /// \note Return 0 if not found
      group_t get_group(id_t group) const;

      group_t get_group_count() const { return task_group_id; }

      /// \brief output the IR
      resolved_graph compile_tree();

    private:
      /// \brief Only keep the longest dependency chains
      bool canonicalize();

    private:
      std::map<id_t, group_t> group_names;

      struct links
      {
        // from -> tg -> to
        std::set<group_t> from;
        std::set<group_t> to;
      };
      std::set<group_t> roots;
      std::map<group_t, links> dependencies;
      std::map<group_t, std::string> debug_names;
      std::map<group_t, group_configuration> configuration;

      group_t task_group_id = 1;
  };

  
}

// So we can serialize the IR
#include "../struct_metadata/struct_metadata.hpp"

N_METADATA_STRUCT(neam::threading::ir_opcode)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(opcode),
    N_MEMBER_DEF(arg)
  >;
};

N_METADATA_STRUCT(neam::threading::group_configuration)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(restrict_to_named_thread)
  >;
};

N_METADATA_STRUCT(neam::threading::resolved_graph)
{
  using member_list = neam::ct::type_list
  <
    N_MEMBER_DEF(groups),
    N_MEMBER_DEF(chain_count),
    N_MEMBER_DEF(opcodes),
    N_MEMBER_DEF(debug_names),
    N_MEMBER_DEF(configuration)
  >;
};

