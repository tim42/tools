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

#include "task_group_graph.hpp"
#include "../logger/logger.hpp"

namespace neam::threading
{
  group_t task_group_dependency_tree::add_task_group(id_t id)
  {
    const group_t key = task_group_id;
    if (key == 0 || key == 0xFF)
    {
      cr::out().critical("threading::dependency-graph: overflow in group id");
      return 0xFF;
    }

    ++task_group_id;

    if (group_names.contains(id))
    {
      cr::out().warn("threading::dependency-graph: Skipping add_task_group call as a group with the name {} is already added (group skipped: {})", id, key);
      return key;
    }

    if (dependencies.contains(key))
    {
      cr::out().warn("threading::dependency-graph: Skipping add_task_group call as {} is already added", key);
      return key;
    }
    group_names.emplace(id, key);
    roots.emplace(key);
    dependencies.emplace(key, links{});
    return key;
  }

  void task_group_dependency_tree::add_dependency(group_t group, group_t dependency)
  {
    if (!dependencies.contains(group) || !dependencies.contains(dependency))
    {
      cr::out().warn("threading::dependency-graph: Skipping add_dependency call as either {} or {} are not added as a group", group, dependency);
      return;
    }

    // group is no longer a root as it depend on another task-group:
    roots.erase(group);

    // link the groups:
    dependencies[group].from.emplace(dependency);
    dependencies[dependency].to.emplace(group);
  }

  void task_group_dependency_tree::add_dependency(id_t group, id_t dependency)
  {
    auto group_it = group_names.find(group);
    auto dep_it = group_names.find(dependency);
    if (group_it != group_names.end() && dep_it != group_names.end())
    {
      add_dependency(group_it->second, dep_it->second);
    }
    else
    {
      cr::out().warn("threading::dependency-graph: Skipping add_dependency call as either {} or {} are not valid group names", group, dependency);
    }
  }

  resolved_graph task_group_dependency_tree::compile_tree()
  {
    // first, canonicalize the graph so we only have relevant dependencies
    if (!canonicalize())
      return {};

    resolved_graph ret;
    ret.groups = group_names;
    std::vector<std::vector<ir_opcode>> chains;

    std::set<group_t> launched_groups;
    struct priority_group { group_t group; uint32_t depth; };
    std::vector<priority_group> groups_to_launch;

    // simple primitives to manage vectors as sets:
    const auto find = [](std::vector<priority_group>& v, group_t key)
    {
      return std::find_if(v.begin(), v.end(), [key](const priority_group& grp) { return key == grp.group; });
    };
    const auto remove = [find](std::vector<priority_group>& v, group_t key)
    {
      if (auto it = find(v, key); it != v.end())
        v.erase(it);
    };
    const auto push_or_update = [find](std::vector<priority_group>& v, group_t key, uint32_t depth)
    {
      if (auto it = find(v, key); it != v.end())
        it->depth = std::min(it->depth, depth);
      else
        v.push_back({key, depth});
    };
    const auto priority_sort = [](std::vector<priority_group>& v)
    {
      // reserse sort, so back() is the lowest one (so we can use pop_back())
      std::sort(v.begin(), v.end(), [](const priority_group& a, const priority_group& b) { return a.depth == b.depth ? a.group > b.group : a.depth > b.depth; });
    };

    const auto create_chain = [&](std::vector<ir_opcode>& chain, group_t root)
    {
      // walk through the dependencies, taking one and pushing the rest to the to-execute list:
      links* l = &dependencies[root];
      group_t next = root;
      group_t prev = root;
      uint32_t depth = 0;
      do
      {
        if (l->to.size() == 0)
          break;
        next = root; // known invalid:
        for (auto it : l->to)
        {
          if (launched_groups.contains(it))
            continue;
          if (next == root)
          {
            // we got our group to execute:
            next = it;
          }
          else
          {
            // add the rest to the to execute list
            push_or_update(groups_to_launch, it, depth);
          }
        }
        if (next != root)
        {
          remove(groups_to_launch, next);
          launched_groups.insert(next);
          l = &dependencies[next];
          prev = next;

          // add the wait operations:
          for (auto it : l->from)
          {
            chain.push_back({ir_opcode::wait_task_group, it});
          }
          // run the task group:
          chain.push_back({ir_opcode::execute_task_group, next});
        }
        ++depth;
      } while (next != root);
      {
        // our last operation is to wait for the last group (so when we reach end_chain there's no more group is the chain)
        chain.push_back({ir_opcode::wait_task_group, prev});
      }

    };

    // go through all the roots, and create a chain per root
    {
      chains.reserve(roots.size());
      for (group_t root : roots)
      {
        std::vector<ir_opcode> chain;

        // start by executing the root:
        chain.push_back({ir_opcode::execute_task_group, root});
        launched_groups.emplace(root);

        create_chain(chain, root);

        chain.push_back({ir_opcode::end_chain});

        chains.push_back(std::move(chain));
      }
    }

    // go through all the remaining "to launch" and create a chain for each of them
    // (this is not the most compact in term of number of chains, way to do it, but it's the simplest)
    {
      while (groups_to_launch.size())
      {
        priority_sort(groups_to_launch);
        group_t root = groups_to_launch.back().group;
        groups_to_launch.pop_back();
        std::vector<ir_opcode> chain;

        // add the wait operations:
        const auto& l = dependencies[root];
        for (auto it : l.from)
          chain.push_back({ir_opcode::wait_task_group, it});
        // execute the task group:
        chain.push_back({ir_opcode::execute_task_group, root});
        launched_groups.emplace(root);

        // walk the remaining entries:
        create_chain(chain, root);

        chain.push_back({ir_opcode::end_chain});

        chains.push_back(std::move(chain));
      }
    }

    ret.chain_count = (uint32_t)chains.size();

    // consolidate the chains in a single vector
    {
      uint16_t count = 0;
      // write the jump table:
      for (auto& it : chains)
      {
        ret.opcodes.push_back({ir_opcode::declare_chain_index, count + (uint16_t)ret.chain_count});
        count += (uint16_t)it.size();
      }

      ret.opcodes.reserve(count);

      // insert the opcodes:
      for (auto& it : chains)
      {
        ret.opcodes.insert(ret.opcodes.end(), it.begin(), it.end());
      }
    }
    return ret;
  }

  bool task_group_dependency_tree::canonicalize()
  {
    // brute-force the process as it's not perf sensitive and we won't have that much nodes
    // can be improved by having a cache of indirect connections

    std::set<group_t> stack;
    bool has_loops = false;

    auto rec_search_from = [&, this](auto& rec_search_from, group_t to_find, group_t group) -> bool
    {
      if (to_find == group) return true; // found
      stack.emplace(group);
      for (auto& it : dependencies[group].from)
      {
        if (it == to_find)
          return true;
        if (stack.contains(it))
        {
          has_loops = true;
          return true; // true is the shortest path of return. The graph is invalid so we won't touch it anyway.
        }
        if (rec_search_from(rec_search_from, to_find, it))
          return true;
      }
      stack.erase(group);
      return false;
    };

    for (auto& it : dependencies)
    {
      group_t tg = it.first;
      links& n = it.second;

      // for each dependency
      for (auto depit = n.from.begin(); depit != n.from.end(); ++depit)
      {
        bool to_remove = false;
        // search if any other dependency indirectly depend on it
        for (auto to_test : n.from)
        {
          if (to_test == *depit) continue;

          stack.clear();
          stack.emplace(*depit);
          if (rec_search_from(rec_search_from, *depit, to_test))
          {
            to_remove = true;
            break;
          }
        }

        if (has_loops)
        {
          cr::out().error("threading::dependency-graph: refusing to canonicalize a graph that has loops");
          return false;
        }

        // if it does, remove the direct dependency
        if (to_remove)
        {
          dependencies[*depit].to.erase(tg);
          depit = n.from.erase(depit);
        }
      }
    }
    return true;
  }

  void resolved_graph::print_debug() const
  {
    cr::out().debug("----resolved graph debug----");
    cr::out().debug(" groups:");
    for (const auto& it : groups)
      cr::out().debug("  group {}: {}", it.second, it.first);
    cr::out().debug(" chain counts: {}", chain_count);
    cr::out().debug(" opcodes:");
    unsigned entry = 0;
    for (const auto& it : opcodes)
    {
      switch (it.opcode)
      {
        case ir_opcode::declare_chain_index:
          cr::out().debug("   {:5}: start_chain {}", entry, it.arg);
          break;
        case ir_opcode::end_chain:
          cr::out().debug("   {:5}: end_chain", entry);
          break;
        case ir_opcode::execute_task_group:
          cr::out().debug("   {:5}: execute_task_group {}", entry, it.arg);
          break;
        case ir_opcode::wait_task_group:
          cr::out().debug("   {:5}: wait_task_group {}", entry, it.arg);
          break;
      }
      ++entry;
    }
    cr::out().debug("----resolved graph debug----");
  }
}
