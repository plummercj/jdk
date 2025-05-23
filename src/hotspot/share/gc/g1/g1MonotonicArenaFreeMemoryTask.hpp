/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_G1_G1MONOTONICARENAFREEMEMORYTASK_HPP
#define SHARE_GC_G1_G1MONOTONICARENAFREEMEMORYTASK_HPP

#include "gc/g1/g1CardSetMemory.hpp"
#include "gc/g1/g1HeapRegionRemSet.hpp"
#include "gc/g1/g1MonotonicArenaFreePool.hpp"
#include "gc/g1/g1ServiceThread.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/ticks.hpp"

// Task handling deallocation of free G1MonotonicArena memory.
class G1MonotonicArenaFreeMemoryTask : public G1ServiceTask {

  enum class State : uint {
    Inactive,
    CalculateUsed,
    ReturnToVM,
    ReturnToOS,
    Cleanup
  };

  static constexpr const char* _state_names[] = { "Invalid",
                                                  "CalculateUsed",
                                                  "ReturnToVM",
                                                  "ReturnToOS",
                                                  "Cleanup" };

  const char* get_state_name(State value) const;

  State _state;

  // Current total monotonic arena  memory usage.
  G1MonotonicArenaMemoryStats _total_used;

  using G1ReturnMemoryProcessor = G1MonotonicArenaFreePool::G1ReturnMemoryProcessor;
  using G1ReturnMemoryProcessorSet = G1MonotonicArenaFreePool::G1ReturnMemoryProcessorSet;

  G1ReturnMemoryProcessorSet* _return_info;

  // Returns whether the given deadline has passed.
  bool deadline_exceeded(jlong deadline);

  // Methods for the tasks to be done. They all return true if that step has
  // completed.
  bool calculate_return_infos(jlong deadline);
  bool return_memory_to_vm(jlong deadline);
  bool return_memory_to_os(jlong deadline);
  bool cleanup_return_infos();

  // Free excess monotonic arena memory, main method. Returns true if there is more work
  // to do.
  bool free_excess_arena_memory();

  void set_state(State new_state);
  // Returns whether we are currently processing a recent request.
  bool is_active() const;

  // The delay used to reschedule this task if not all work has been completed.
  jlong reschedule_delay_ms() const;

public:
  explicit G1MonotonicArenaFreeMemoryTask(const char* name);

  void execute() override;

  // Notify the task of new used remembered set memory statistics for the young
  // generation and the collection set candidate sets.
  void notify_new_stats(G1MonotonicArenaMemoryStats* young_gen_stats,
                        G1MonotonicArenaMemoryStats* collection_set_candidate_stats);
};

#endif // SHARE_GC_G1_G1MONOTONICARENAFREEMEMORYTASK_HPP
