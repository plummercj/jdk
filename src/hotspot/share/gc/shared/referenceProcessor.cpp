/*
 * Copyright (c) 2001, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "classfile/javaClasses.inline.hpp"
#include "compiler/compilerDefinitions.inline.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/collectedHeap.inline.hpp"
#include "gc/shared/gc_globals.hpp"
#include "gc/shared/gcTimer.hpp"
#include "gc/shared/gcTraceTime.inline.hpp"
#include "gc/shared/referencePolicy.hpp"
#include "gc/shared/referenceProcessor.inline.hpp"
#include "gc/shared/referenceProcessorPhaseTimes.hpp"
#include "logging/log.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "oops/access.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/nonJavaThread.hpp"
#include "utilities/globalDefinitions.hpp"

ReferencePolicy* ReferenceProcessor::_always_clear_soft_ref_policy = nullptr;
ReferencePolicy* ReferenceProcessor::_default_soft_ref_policy      = nullptr;
jlong            ReferenceProcessor::_soft_ref_timestamp_clock = 0;

void referenceProcessor_init() {
  ReferenceProcessor::init_statics();
}

void ReferenceProcessor::init_statics() {
  // We need a monotonically non-decreasing time in ms but
  // os::javaTimeMillis() does not guarantee monotonicity.
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;

  // Initialize the soft ref timestamp clock.
  _soft_ref_timestamp_clock = now;
  // Also update the soft ref clock in j.l.r.SoftReference
  java_lang_ref_SoftReference::set_clock(_soft_ref_timestamp_clock);

  _always_clear_soft_ref_policy = new AlwaysClearPolicy();
  if (CompilerConfig::is_c2_or_jvmci_compiler_enabled()) {
    _default_soft_ref_policy = new LRUMaxHeapPolicy();
  } else {
    _default_soft_ref_policy = new LRUCurrentHeapPolicy();
  }
}

void ReferenceProcessor::enable_discovery() {
#ifdef ASSERT
  // Verify that we're not currently discovering refs
  assert(!_discovering_refs, "nested call?");

  // Verify that the discovered lists are empty
  verify_no_references_recorded();
#endif // ASSERT

  _discovering_refs = true;
}

ReferenceProcessor::ReferenceProcessor(BoolObjectClosure* is_subject_to_discovery,
                                       uint      mt_processing_degree,
                                       uint      mt_discovery_degree,
                                       bool      concurrent_discovery,
                                       BoolObjectClosure* is_alive_non_header)  :
  _is_subject_to_discovery(is_subject_to_discovery),
  _discovering_refs(false),
  _next_id(0),
  _is_alive_non_header(is_alive_non_header)
{
  assert(is_subject_to_discovery != nullptr, "must be set");

  _discovery_is_concurrent = concurrent_discovery;
  _discovery_is_mt         = (mt_discovery_degree > 1);
  _num_queues              = MAX2(1U, mt_processing_degree);
  _max_num_queues          = MAX2(_num_queues, mt_discovery_degree);
  _discovered_refs         = NEW_C_HEAP_ARRAY(DiscoveredList,
            _max_num_queues * number_of_subclasses_of_ref(), mtGC);

  _discoveredSoftRefs    = &_discovered_refs[0];
  _discoveredWeakRefs    = &_discoveredSoftRefs[_max_num_queues];
  _discoveredFinalRefs   = &_discoveredWeakRefs[_max_num_queues];
  _discoveredPhantomRefs = &_discoveredFinalRefs[_max_num_queues];

  // Initialize all entries to null
  for (uint i = 0; i < _max_num_queues * number_of_subclasses_of_ref(); i++) {
    _discovered_refs[i].clear();
  }

  setup_policy(false /* default soft ref policy */);
}

#ifndef PRODUCT
void ReferenceProcessor::verify_no_references_recorded() {
  guarantee(!_discovering_refs, "Discovering refs?");
  for (uint i = 0; i < _max_num_queues * number_of_subclasses_of_ref(); i++) {
    guarantee(_discovered_refs[i].is_empty(),
              "Found non-empty discovered list at %u", i);
  }
}
#endif

bool ReferenceProcessor::processing_is_mt() const {
  return ParallelRefProcEnabled && _num_queues > 1;
}

void ReferenceProcessor::weak_oops_do(OopClosure* f) {
  for (uint i = 0; i < _max_num_queues * number_of_subclasses_of_ref(); i++) {
    if (UseCompressedOops) {
      f->do_oop((narrowOop*)_discovered_refs[i].adr_head());
    } else {
      f->do_oop((oop*)_discovered_refs[i].adr_head());
    }
  }
}

void ReferenceProcessor::update_soft_ref_master_clock() {
  // Update (advance) the soft ref master clock field. This must be done
  // after processing the soft ref list.

  // We need a monotonically non-decreasing time in ms but
  // os::javaTimeMillis() does not guarantee monotonicity.
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;

  NOT_PRODUCT(
  if (now < _soft_ref_timestamp_clock) {
    log_warning(gc)("time warp: " JLONG_FORMAT " to " JLONG_FORMAT,
                    _soft_ref_timestamp_clock, now);
  }
  )
  // The values of now and _soft_ref_timestamp_clock are set using
  // javaTimeNanos(), which is guaranteed to be monotonically
  // non-decreasing provided the underlying platform provides such
  // a time source (and it is bug free).
  // In product mode, however, protect ourselves from non-monotonicity.
  if (now > _soft_ref_timestamp_clock) {
    _soft_ref_timestamp_clock = now;
    java_lang_ref_SoftReference::set_clock(now);
  }
  // Else leave clock stalled at its old value until time progresses
  // past clock value.
}

size_t ReferenceProcessor::total_count(DiscoveredList lists[]) const {
  size_t total = 0;
  for (uint i = 0; i < _max_num_queues; ++i) {
    total += lists[i].length();
  }
  return total;
}

#ifdef ASSERT
void ReferenceProcessor::verify_total_count_zero(DiscoveredList lists[], const char* type) {
  size_t count = total_count(lists);
  assert(count == 0, "%ss must be empty but has %zu elements", type, count);
}
#endif

ReferenceProcessorStats ReferenceProcessor::process_discovered_references(RefProcProxyTask& proxy_task,
                                                                          WorkerThreads* workers,
                                                                          ReferenceProcessorPhaseTimes& phase_times) {

  double start_time = os::elapsedTime();

  // Stop treating discovered references specially.
  disable_discovery();

  phase_times.set_ref_discovered(REF_SOFT, total_count(_discoveredSoftRefs));
  phase_times.set_ref_discovered(REF_WEAK, total_count(_discoveredWeakRefs));
  phase_times.set_ref_discovered(REF_FINAL, total_count(_discoveredFinalRefs));
  phase_times.set_ref_discovered(REF_PHANTOM, total_count(_discoveredPhantomRefs));

  update_soft_ref_master_clock();

  phase_times.set_processing_is_mt(processing_is_mt());

  {
    RefProcTotalPhaseTimesTracker tt(SoftWeakFinalRefsPhase, &phase_times);
    process_soft_weak_final_refs(proxy_task, workers, phase_times);
  }

  {
    RefProcTotalPhaseTimesTracker tt(KeepAliveFinalRefsPhase, &phase_times);
    process_final_keep_alive(proxy_task, workers, phase_times);
  }

  {
    RefProcTotalPhaseTimesTracker tt(PhantomRefsPhase, &phase_times);
    process_phantom_refs(proxy_task, workers, phase_times);
  }

  phase_times.set_total_time_ms((os::elapsedTime() - start_time) * 1000);

  // Elements on discovered lists were pushed to the pending list.
  verify_no_references_recorded();

  ReferenceProcessorStats stats(phase_times.ref_discovered(REF_SOFT),
                                phase_times.ref_discovered(REF_WEAK),
                                phase_times.ref_discovered(REF_FINAL),
                                phase_times.ref_discovered(REF_PHANTOM));
  return stats;
}

void BarrierEnqueueDiscoveredFieldClosure::enqueue(HeapWord* discovered_field_addr, oop value) {
  assert(Universe::heap()->is_in(discovered_field_addr), PTR_FORMAT " not in heap", p2i(discovered_field_addr));
  HeapAccess<AS_NO_KEEPALIVE>::oop_store(discovered_field_addr,
                                         value);
}

void DiscoveredListIterator::load_ptrs(DEBUG_ONLY(bool allow_null_referent)) {
  _current_discovered_addr = java_lang_ref_Reference::discovered_addr_raw(_current_discovered);
  oop discovered = java_lang_ref_Reference::discovered(_current_discovered);
  assert(_current_discovered_addr && oopDesc::is_oop_or_null(discovered),
         "Expected an oop or null for discovered field at " PTR_FORMAT, p2i(discovered));
  _next_discovered = discovered;
  _referent = java_lang_ref_Reference::unknown_referent_no_keepalive(_current_discovered);
  assert(Universe::heap()->is_in_or_null(_referent),
         "Wrong oop found in java.lang.Reference object");
  assert(allow_null_referent ?
             oopDesc::is_oop_or_null(_referent)
           : oopDesc::is_oop(_referent),
         "Expected an oop%s for referent field at " PTR_FORMAT,
         (allow_null_referent ? " or null" : ""),
         p2i(_referent));
}

void DiscoveredListIterator::remove() {
  assert(oopDesc::is_oop(_current_discovered), "Dropping a bad reference");
  RawAccess<>::oop_store(_current_discovered_addr, oop(nullptr));

  // First _prev_next ref actually points into DiscoveredList (gross).
  oop new_next;
  if (_next_discovered == _current_discovered) {
    // At the end of the list, we should make _prev point to itself.
    // If _ref is the first ref, then _prev_next will be in the DiscoveredList,
    // and _prev will be null.
    new_next = _prev_discovered;
  } else {
    new_next = _next_discovered;
  }
  // Remove Reference object from discovered list. We do not need barriers here,
  // as we only remove. We will do the barrier when we actually advance the cursor.
  RawAccess<>::oop_store(_prev_discovered_addr, new_next);
  _removed++;
  _refs_list.dec_length(1);
}

void DiscoveredListIterator::make_referent_alive() {
  HeapWord* addr = java_lang_ref_Reference::referent_addr_raw(_current_discovered);
  if (UseCompressedOops) {
    _keep_alive->do_oop((narrowOop*)addr);
  } else {
    _keep_alive->do_oop((oop*)addr);
  }
}

void DiscoveredListIterator::clear_referent() {
  java_lang_ref_Reference::clear_referent_raw(_current_discovered);
}

void DiscoveredListIterator::enqueue() {
  if (_prev_discovered_addr != _refs_list.adr_head()) {
    _enqueue->enqueue(_prev_discovered_addr, _current_discovered);
  } else {
    RawAccess<>::oop_store(_prev_discovered_addr, _current_discovered);
  }
}

void DiscoveredListIterator::complete_enqueue() {
  if (_prev_discovered != nullptr) {
    // This is the last object.
    // Swap refs_list into pending list and set obj's
    // discovered to what we read from the pending list.
    oop old = Universe::swap_reference_pending_list(_refs_list.head());
    _enqueue->enqueue(java_lang_ref_Reference::discovered_addr_raw(_prev_discovered), old);
  }
}

inline void log_preclean_ref(const DiscoveredListIterator& iter, const char* reason) {
  if (log_develop_is_enabled(Trace, gc, ref)) {
    ResourceMark rm;
    log_develop_trace(gc, ref)("Precleaning %s reference " PTR_FORMAT ": %s",
                               reason, p2i(iter.obj()),
                               iter.obj()->klass()->internal_name());
  }
}

inline void log_dropped_ref(const DiscoveredListIterator& iter, const char* reason) {
  if (log_develop_is_enabled(Trace, gc, ref)) {
    ResourceMark rm;
    log_develop_trace(gc, ref)("Dropping %s reference " PTR_FORMAT ": %s",
                               reason, p2i(iter.obj()),
                               iter.obj()->klass()->internal_name());
  }
}

inline void log_enqueued_ref(const DiscoveredListIterator& iter, const char* reason) {
  if (log_develop_is_enabled(Trace, gc, ref)) {
    ResourceMark rm;
    log_develop_trace(gc, ref)("Enqueue %s reference (" PTR_FORMAT ": %s)",
                               reason, p2i(iter.obj()), iter.obj()->klass()->internal_name());
  }
  assert(oopDesc::is_oop(iter.obj()), "Adding a bad reference");
}

size_t ReferenceProcessor::process_discovered_list_work(DiscoveredList&    refs_list,
                                                        BoolObjectClosure* is_alive,
                                                        OopClosure*        keep_alive,
                                                        EnqueueDiscoveredFieldClosure* enqueue,
                                                        bool               do_enqueue_and_clear) {
  DiscoveredListIterator iter(refs_list, keep_alive, is_alive, enqueue);
  while (iter.has_next()) {
    iter.load_ptrs(DEBUG_ONLY(discovery_is_concurrent() /* allow_null_referent */));
    if (iter.referent() == nullptr) {
      // Reference has been cleared since discovery; only possible if
      // discovery is concurrent (checked by load_ptrs).  Remove
      // reference from list.
      log_dropped_ref(iter, "cleared");
      iter.remove();
      iter.move_to_next();
    } else if (iter.is_referent_alive()) {
      // The referent is reachable after all.
      // Remove reference from list.
      log_dropped_ref(iter, "reachable");
      iter.remove();
      // Update the referent pointer as necessary.  Note that this
      // should not entail any recursive marking because the
      // referent must already have been traversed.
      iter.make_referent_alive();
      iter.move_to_next();
    } else {
      if (do_enqueue_and_clear) {
        iter.clear_referent();
        iter.enqueue();
        log_enqueued_ref(iter, "cleared");
      }
      // Keep in discovered list
      iter.next();
    }
  }
  if (do_enqueue_and_clear) {
    iter.complete_enqueue();
    refs_list.clear();
  }

  log_develop_trace(gc, ref)(" Dropped %zu active Refs out of %zu"
                             " Refs in discovered list " PTR_FORMAT,
                             iter.removed(), iter.processed(), p2i(&refs_list));
  return iter.removed();
}

size_t ReferenceProcessor::process_final_keep_alive_work(DiscoveredList& refs_list,
                                                         OopClosure*     keep_alive,
                                                         EnqueueDiscoveredFieldClosure* enqueue) {
  DiscoveredListIterator iter(refs_list, keep_alive, nullptr, enqueue);
  while (iter.has_next()) {
    iter.load_ptrs(DEBUG_ONLY(false /* allow_null_referent */));
    // keep the referent and followers around
    iter.make_referent_alive();

    // Self-loop next, to mark the FinalReference not active.
    assert(java_lang_ref_Reference::next(iter.obj()) == nullptr, "enqueued FinalReference");
    java_lang_ref_Reference::set_next_raw(iter.obj(), iter.obj());

    iter.enqueue();
    log_enqueued_ref(iter, "Final");
    iter.next();
  }
  iter.complete_enqueue();
  refs_list.clear();

  assert(iter.removed() == 0, "This phase does not remove anything.");
  return iter.removed();
}

void
ReferenceProcessor::clear_discovered_references(DiscoveredList& refs_list) {
  oop obj = nullptr;
  oop next = refs_list.head();
  while (next != obj) {
    obj = next;
    next = java_lang_ref_Reference::discovered(obj);
    java_lang_ref_Reference::set_discovered_raw(obj, nullptr);
  }
  refs_list.clear();
}

void ReferenceProcessor::abandon_partial_discovery() {
  // loop over the lists
  for (uint i = 0; i < _max_num_queues * number_of_subclasses_of_ref(); i++) {
    if ((i % _max_num_queues) == 0) {
      log_develop_trace(gc, ref)("Abandoning %s discovered list", list_name(i));
    }
    clear_discovered_references(_discovered_refs[i]);
  }
}

size_t ReferenceProcessor::total_reference_count(ReferenceType type) const {
  DiscoveredList* list = nullptr;

  switch (type) {
    case REF_SOFT:
      list = _discoveredSoftRefs;
      break;
    case REF_WEAK:
      list = _discoveredWeakRefs;
      break;
    case REF_FINAL:
      list = _discoveredFinalRefs;
      break;
    case REF_PHANTOM:
      list = _discoveredPhantomRefs;
      break;
    case REF_NONE:
    default:
      ShouldNotReachHere();
  }
  return total_count(list);
}

void RefProcTask::process_discovered_list(uint worker_id,
                                          ReferenceType ref_type,
                                          BoolObjectClosure* is_alive,
                                          OopClosure* keep_alive,
                                          EnqueueDiscoveredFieldClosure* enqueue) {
  ReferenceProcessor::RefProcSubPhases subphase;
  DiscoveredList* dl;
  switch (ref_type) {
    case ReferenceType::REF_SOFT:
      subphase = ReferenceProcessor::ProcessSoftRefSubPhase;
      dl = _ref_processor._discoveredSoftRefs;
      break;
    case ReferenceType::REF_WEAK:
      subphase = ReferenceProcessor::ProcessWeakRefSubPhase;
      dl = _ref_processor._discoveredWeakRefs;
      break;
    case ReferenceType::REF_FINAL:
      subphase = ReferenceProcessor::ProcessFinalRefSubPhase;
      dl = _ref_processor._discoveredFinalRefs;
      break;
    case ReferenceType::REF_PHANTOM:
      subphase = ReferenceProcessor::ProcessPhantomRefsSubPhase;
      dl = _ref_processor._discoveredPhantomRefs;
      break;
    default:
      ShouldNotReachHere();
  }

  // Only Final refs are not enqueued and cleared here.
  bool do_enqueue_and_clear = (ref_type != REF_FINAL);

  {
    RefProcSubPhasesWorkerTimeTracker tt(subphase, _phase_times, tracker_id(worker_id));
    size_t const removed = _ref_processor.process_discovered_list_work(dl[worker_id],
                                                                       is_alive,
                                                                       keep_alive,
                                                                       enqueue,
                                                                       do_enqueue_and_clear);
    _phase_times->add_ref_dropped(ref_type, removed);
  }
}

class RefProcSoftWeakFinalPhaseTask: public RefProcTask {
public:
  RefProcSoftWeakFinalPhaseTask(ReferenceProcessor& ref_processor,
                                ReferenceProcessorPhaseTimes* phase_times)
    : RefProcTask(ref_processor,
                  phase_times) {}

  void rp_work(uint worker_id,
               BoolObjectClosure* is_alive,
               OopClosure* keep_alive,
               EnqueueDiscoveredFieldClosure* enqueue,
               VoidClosure* complete_gc) override {
    RefProcWorkerTimeTracker t(_phase_times->soft_weak_final_refs_phase_worker_time_sec(), tracker_id(worker_id));

    process_discovered_list(worker_id, REF_SOFT, is_alive, keep_alive, enqueue);

    process_discovered_list(worker_id, REF_WEAK, is_alive, keep_alive, enqueue);

    process_discovered_list(worker_id, REF_FINAL, is_alive, keep_alive, enqueue);

    // Close the reachable set; needed for collectors which keep_alive_closure do
    // not immediately complete their work.
    complete_gc->do_void();
  }
};

class RefProcKeepAliveFinalPhaseTask: public RefProcTask {
public:
  RefProcKeepAliveFinalPhaseTask(ReferenceProcessor& ref_processor,
                                 ReferenceProcessorPhaseTimes* phase_times)
    : RefProcTask(ref_processor,
                  phase_times) {}

  void rp_work(uint worker_id,
               BoolObjectClosure* is_alive,
               OopClosure* keep_alive,
               EnqueueDiscoveredFieldClosure* enqueue,
               VoidClosure* complete_gc) override {
    RefProcSubPhasesWorkerTimeTracker tt(ReferenceProcessor::KeepAliveFinalRefsSubPhase, _phase_times, tracker_id(worker_id));
    _ref_processor.process_final_keep_alive_work(_ref_processor._discoveredFinalRefs[worker_id], keep_alive, enqueue);
    // Close the reachable set
    complete_gc->do_void();
  }
};

class RefProcPhantomPhaseTask: public RefProcTask {
public:
  RefProcPhantomPhaseTask(ReferenceProcessor& ref_processor,
                          ReferenceProcessorPhaseTimes* phase_times)
    : RefProcTask(ref_processor,
                  phase_times) {}

  void rp_work(uint worker_id,
               BoolObjectClosure* is_alive,
               OopClosure* keep_alive,
               EnqueueDiscoveredFieldClosure* enqueue,
               VoidClosure* complete_gc) override {
    process_discovered_list(worker_id, REF_PHANTOM, is_alive, keep_alive, enqueue);

    // Close the reachable set; needed for collectors which keep_alive_closure do
    // not immediately complete their work.
    complete_gc->do_void();
  }
};

void ReferenceProcessor::log_reflist(const char* prefix, DiscoveredList list[], uint num_active_queues) {
  LogTarget(Trace, gc, ref) lt;

  if (!lt.is_enabled()) {
    return;
  }

  size_t total = 0;

  LogStream ls(lt);
  ls.print("%s", prefix);
  for (uint i = 0; i < num_active_queues; i++) {
    ls.print("%zu ", list[i].length());
    total += list[i].length();
  }
  ls.print_cr("(%zu)", total);
}

#ifndef PRODUCT
void ReferenceProcessor::log_reflist_counts(DiscoveredList ref_lists[], uint num_active_queues) {
  if (!log_is_enabled(Trace, gc, ref)) {
    return;
  }

  log_reflist("", ref_lists, num_active_queues);
#ifdef ASSERT
  for (uint i = num_active_queues; i < _max_num_queues; i++) {
    assert(ref_lists[i].length() == 0, "%zu unexpected References in %u",
           ref_lists[i].length(), i);
  }
#endif
}
#endif

void ReferenceProcessor::set_active_mt_degree(uint v) {
  assert(v <= max_num_queues(), "Mt degree %u too high, maximum %u", v,  max_num_queues());
  _num_queues = v;
  _next_id = 0;
}

bool ReferenceProcessor::need_balance_queues(DiscoveredList refs_lists[]) {
  assert(processing_is_mt(), "why balance non-mt processing?");
  // _num_queues is the processing degree.  Only list entries up to
  // _num_queues will be processed, so any non-empty lists beyond
  // that must be redistributed to lists in that range.  Even if not
  // needed for that, balancing may be desirable to eliminate poor
  // distribution of references among the lists.
  if (ParallelRefProcBalancingEnabled) {
    return true;                // Configuration says do it.
  } else {
    // Configuration says don't balance, but if there are non-empty
    // lists beyond the processing degree, then must ignore the
    // configuration and balance anyway.
    for (uint i = _num_queues; i < _max_num_queues; ++i) {
      if (!refs_lists[i].is_empty()) {
        return true;            // Must balance despite configuration.
      }
    }
    return false;               // Safe to obey configuration and not balance.
  }
}

void ReferenceProcessor::maybe_balance_queues(DiscoveredList refs_lists[]) {
  assert(processing_is_mt(), "Should not call this otherwise");
  if (need_balance_queues(refs_lists)) {
    balance_queues(refs_lists);
  }
}

// Balances reference queues.
// Move entries from all queues[0, 1, ..., _max_num_q-1] to
// queues[0, 1, ..., _num_q-1] because only the first _num_q
// corresponding to the active workers will be processed.
void ReferenceProcessor::balance_queues(DiscoveredList ref_lists[]) {
  // calculate total length
  size_t total_refs = 0;
  log_develop_trace(gc, ref)("Balance ref_lists ");

  log_reflist_counts(ref_lists, _max_num_queues);

  for (uint i = 0; i < _max_num_queues; ++i) {
    total_refs += ref_lists[i].length();
  }
  size_t avg_refs = total_refs / _num_queues + 1;
  uint to_idx = 0;
  for (uint from_idx = 0; from_idx < _max_num_queues; from_idx++) {
    size_t from_len = ref_lists[from_idx].length();

    size_t remaining_to_move;
    if (from_idx >= _num_queues) {
      // Move all
      remaining_to_move = from_len;
    } else {
      // Move those above avg_refs
      remaining_to_move = from_len > avg_refs
                        ? from_len - avg_refs
                        : 0;
    }

    while (remaining_to_move > 0) {
      assert(to_idx < _num_queues, "Sanity Check!");

      size_t to_len = ref_lists[to_idx].length();
      if (to_len >= avg_refs) {
        // this list is full enough; move on to next
        to_idx++;
        continue;
      }
      size_t refs_to_move = MIN2(remaining_to_move, avg_refs - to_len);
      assert(refs_to_move > 0, "otherwise the code below will fail");

      oop move_head = ref_lists[from_idx].head();
      oop move_tail = move_head;
      oop new_head  = move_head;
      // find an element to split the list on
      for (size_t j = 0; j < refs_to_move; ++j) {
        move_tail = new_head;
        new_head = java_lang_ref_Reference::discovered(new_head);
      }

      // Add the chain to the to list.
      if (ref_lists[to_idx].head() == nullptr) {
        // to list is empty. Make a loop at the end.
        java_lang_ref_Reference::set_discovered_raw(move_tail, move_tail);
      } else {
        java_lang_ref_Reference::set_discovered_raw(move_tail, ref_lists[to_idx].head());
      }
      ref_lists[to_idx].set_head(move_head);
      ref_lists[to_idx].inc_length(refs_to_move);

      // Remove the chain from the from list.
      if (move_tail == new_head) {
        // We found the end of the from list.
        ref_lists[from_idx].set_head(nullptr);
      } else {
        ref_lists[from_idx].set_head(new_head);
      }
      ref_lists[from_idx].dec_length(refs_to_move);

      remaining_to_move -= refs_to_move;
    }
  }
#ifdef ASSERT
  log_reflist_counts(ref_lists, _num_queues);
  size_t balanced_total_refs = 0;
  for (uint i = 0; i < _num_queues; ++i) {
    balanced_total_refs += ref_lists[i].length();
  }
  assert(total_refs == balanced_total_refs, "Balancing was incomplete");
#endif
}

void ReferenceProcessor::run_task(RefProcTask& task, RefProcProxyTask& proxy_task, WorkerThreads* workers, bool marks_oops_alive) {
  log_debug(gc, ref)("ReferenceProcessor::execute queues: %d, %s, marks_oops_alive: %s",
                     num_queues(),
                     processing_is_mt() ? "RefProcThreadModel::Multi" : "RefProcThreadModel::Single",
                     marks_oops_alive ? "true" : "false");

  proxy_task.prepare_run_task(task, num_queues(), processing_is_mt() ? RefProcThreadModel::Multi : RefProcThreadModel::Single, marks_oops_alive);
  if (processing_is_mt()) {
    assert(workers != nullptr, "can not dispatch multi threaded without workers");
    assert(workers->active_workers() >= num_queues(),
           "Ergonomically chosen workers(%u) should be less than or equal to active workers(%u)",
           num_queues(), workers->active_workers());
    workers->run_task(&proxy_task, num_queues());
  } else {
    for (unsigned i = 0; i < _max_num_queues; ++i) {
      proxy_task.work(i);
    }
  }
}

static uint num_active_workers(WorkerThreads* workers) {
  return workers != nullptr ? workers->active_workers() : 1;
}

void ReferenceProcessor::process_soft_weak_final_refs(RefProcProxyTask& proxy_task,
                                                      WorkerThreads* workers,
                                                      ReferenceProcessorPhaseTimes& phase_times) {

  size_t const num_soft_refs = phase_times.ref_discovered(REF_SOFT);
  size_t const num_weak_refs = phase_times.ref_discovered(REF_WEAK);
  size_t const num_final_refs = phase_times.ref_discovered(REF_FINAL);
  size_t const num_total_refs = num_soft_refs + num_weak_refs + num_final_refs;

  if (num_total_refs == 0) {
    log_debug(gc, ref)("Skipped SoftWeakFinalRefsPhase of Reference Processing: no references");
    return;
  }

  RefProcMTDegreeAdjuster a(this, SoftWeakFinalRefsPhase, num_active_workers(workers), num_total_refs);

  if (processing_is_mt()) {
    RefProcBalanceQueuesTimeTracker tt(SoftWeakFinalRefsPhase, &phase_times);
    maybe_balance_queues(_discoveredSoftRefs);
    maybe_balance_queues(_discoveredWeakRefs);
    maybe_balance_queues(_discoveredFinalRefs);
  }

  log_reflist("SoftWeakFinalRefsPhase Soft before", _discoveredSoftRefs, _max_num_queues);
  log_reflist("SoftWeakFinalRefsPhase Weak before", _discoveredWeakRefs, _max_num_queues);
  log_reflist("SoftWeakFinalRefsPhase Final before", _discoveredFinalRefs, _max_num_queues);

  RefProcSoftWeakFinalPhaseTask phase_task(*this, &phase_times);
  run_task(phase_task, proxy_task, workers, false);

  verify_total_count_zero(_discoveredSoftRefs, "SoftReference");
  verify_total_count_zero(_discoveredWeakRefs, "WeakReference");
  log_reflist("SoftWeakFinalRefsPhase Final after", _discoveredFinalRefs, _max_num_queues);
}

void ReferenceProcessor::process_final_keep_alive(RefProcProxyTask& proxy_task,
                                                  WorkerThreads* workers,
                                                  ReferenceProcessorPhaseTimes& phase_times) {

  size_t const num_final_refs = phase_times.ref_discovered(REF_FINAL);

  if (num_final_refs == 0) {
    log_debug(gc, ref)("Skipped KeepAliveFinalRefsPhase of Reference Processing: no references");
    return;
  }

  RefProcMTDegreeAdjuster a(this, KeepAliveFinalRefsPhase, num_active_workers(workers), num_final_refs);

  if (processing_is_mt()) {
    RefProcBalanceQueuesTimeTracker tt(KeepAliveFinalRefsPhase, &phase_times);
    maybe_balance_queues(_discoveredFinalRefs);
  }

  // Traverse referents of final references and keep them and followers alive.
  RefProcKeepAliveFinalPhaseTask phase_task(*this, &phase_times);
  run_task(phase_task, proxy_task, workers, true);

  verify_total_count_zero(_discoveredFinalRefs, "FinalReference");
}

void ReferenceProcessor::process_phantom_refs(RefProcProxyTask& proxy_task,
                                              WorkerThreads* workers,
                                              ReferenceProcessorPhaseTimes& phase_times) {

  size_t const num_phantom_refs = phase_times.ref_discovered(REF_PHANTOM);

  if (num_phantom_refs == 0) {
    log_debug(gc, ref)("Skipped PhantomRefsPhase of Reference Processing: no references");
    return;
  }

  RefProcMTDegreeAdjuster a(this, PhantomRefsPhase, num_active_workers(workers), num_phantom_refs);

  if (processing_is_mt()) {
    RefProcBalanceQueuesTimeTracker tt(PhantomRefsPhase, &phase_times);
    maybe_balance_queues(_discoveredPhantomRefs);
  }

  log_reflist("PhantomRefsPhase Phantom before", _discoveredPhantomRefs, _max_num_queues);

  RefProcPhantomPhaseTask phase_task(*this, &phase_times);
  run_task(phase_task, proxy_task, workers, false);

  verify_total_count_zero(_discoveredPhantomRefs, "PhantomReference");
}

inline DiscoveredList* ReferenceProcessor::get_discovered_list(ReferenceType rt) {
  uint id = 0;
  // Determine the queue index to use for this object.
  if (_discovery_is_mt) {
    // During a multi-threaded discovery phase,
    // each thread saves to its "own" list.
    id = WorkerThread::worker_id();
  } else {
    // single-threaded discovery, we save in round-robin
    // fashion to each of the lists.
    if (processing_is_mt()) {
      id = next_id();
    }
  }
  assert(id < _max_num_queues, "Id is out of bounds id %u and max id %u)", id, _max_num_queues);

  // Get the discovered queue to which we will add
  DiscoveredList* list = nullptr;
  switch (rt) {
    case REF_SOFT:
      list = &_discoveredSoftRefs[id];
      break;
    case REF_WEAK:
      list = &_discoveredWeakRefs[id];
      break;
    case REF_FINAL:
      list = &_discoveredFinalRefs[id];
      break;
    case REF_PHANTOM:
      list = &_discoveredPhantomRefs[id];
      break;
    case REF_NONE:
      // we should not reach here if we are an InstanceRefKlass
    default:
      ShouldNotReachHere();
  }
  log_develop_trace(gc, ref)("Thread %d gets list " PTR_FORMAT, id, p2i(list));
  return list;
}

inline bool ReferenceProcessor::set_discovered_link(HeapWord* discovered_addr, oop next_discovered) {
  return discovery_is_mt() ? set_discovered_link_mt(discovered_addr, next_discovered)
                           : set_discovered_link_st(discovered_addr, next_discovered);
}

inline void ReferenceProcessor::add_to_discovered_list(DiscoveredList& refs_list,
                                                       oop obj,
                                                       HeapWord* discovered_addr) {
  oop current_head = refs_list.head();
  // Prepare value to put into the discovered field. The last ref must have its
  // discovered field pointing to itself.
  oop next_discovered = (current_head != nullptr) ? current_head : obj;

  bool added = set_discovered_link(discovered_addr, next_discovered);
  if (added) {
    // We can always add the object without synchronization: every thread has its
    // own list head.
    refs_list.add_as_head(obj);
    log_develop_trace(gc, ref)("Discovered reference (%s) (" PTR_FORMAT ": %s)",
                               discovery_is_mt() ? "mt" : "st", p2i(obj), obj->klass()->internal_name());
  } else {
    log_develop_trace(gc, ref)("Already discovered reference (mt) (" PTR_FORMAT ": %s)",
                               p2i(obj), obj->klass()->internal_name());
  }
}

inline bool ReferenceProcessor::set_discovered_link_st(HeapWord* discovered_addr,
                                                       oop next_discovered) {
  assert(!discovery_is_mt(), "must be");

  if (discovery_is_stw()) {
    // Do a raw store here: the field will be visited later when processing
    // the discovered references.
    RawAccess<>::oop_store(discovered_addr, next_discovered);
  } else {
    HeapAccess<AS_NO_KEEPALIVE>::oop_store(discovered_addr, next_discovered);
  }
  // Always successful.
  return true;
}

inline bool ReferenceProcessor::set_discovered_link_mt(HeapWord* discovered_addr,
                                                       oop next_discovered) {
  assert(discovery_is_mt(), "must be");

  // We must make sure this object is only enqueued once. Try to CAS into the discovered_addr.
  oop retest;
  if (discovery_is_stw()) {
    // Try a raw store here, still making sure that we enqueue only once: the field
    // will be visited later when processing the discovered references.
    retest = RawAccess<>::oop_atomic_cmpxchg(discovered_addr, oop(nullptr), next_discovered);
  } else {
    retest = HeapAccess<AS_NO_KEEPALIVE>::oop_atomic_cmpxchg(discovered_addr, oop(nullptr), next_discovered);
  }
  return retest == nullptr;
}

#ifndef PRODUCT
// Concurrent discovery might allow us to observe j.l.References with null
// referents, being those cleared concurrently by mutators during (or after) discovery.
void ReferenceProcessor::verify_referent(oop obj) {
  bool concurrent = discovery_is_concurrent();
  oop referent = java_lang_ref_Reference::unknown_referent_no_keepalive(obj);
  assert(concurrent ? oopDesc::is_oop_or_null(referent) : oopDesc::is_oop(referent),
         "Bad referent " PTR_FORMAT " found in Reference "
         PTR_FORMAT " during %sconcurrent discovery ",
         p2i(referent), p2i(obj), concurrent ? "" : "non-");
}
#endif

bool ReferenceProcessor::is_subject_to_discovery(oop const obj) const {
  return _is_subject_to_discovery->do_object_b(obj);
}

// Reference discovery policy:
//   if the reference object is not in the "originating generation"
//   (or part of the heap being collected, indicated by our "span")
//   we don't treat it specially (i.e. we scan it as we would
//   a normal oop, treating its references as strong references).
//   This means that references can't be discovered unless their
//   referent is also in the same span. This is the simplest,
//   most "local" and most conservative approach, albeit one
//   that may cause weak references to be enqueued least promptly.
//   We call this choice the "ReferenceBasedDiscovery" policy.
bool ReferenceProcessor::discover_reference(oop obj, ReferenceType rt) {
  // Make sure we are discovering refs (rather than processing discovered refs).
  if (!_discovering_refs || !RegisterReferences) {
    return false;
  }

  if ((rt == REF_FINAL) && (java_lang_ref_Reference::next(obj) != nullptr)) {
    // Don't rediscover non-active FinalReferences.
    return false;
  }

  if (!is_subject_to_discovery(obj)) {
    // Reference is not in the originating generation;
    // don't treat it specially (i.e. we want to scan it as a normal
    // object with strong references).
    return false;
  }

  // We only discover references whose referents are not (yet)
  // known to be strongly reachable.
  if (is_alive_non_header() != nullptr) {
    verify_referent(obj);
    oop referent = java_lang_ref_Reference::unknown_referent_no_keepalive(obj);
    if (is_alive_non_header()->do_object_b(referent)) {
      return false;  // referent is reachable
    }
  }
  if (rt == REF_SOFT) {
    // For soft refs we can decide now if these are not
    // current candidates for clearing, in which case we
    // can mark through them now, rather than delaying that
    // to the reference-processing phase. Since all current
    // time-stamp policies advance the soft-ref clock only
    // at a full collection cycle, this is always currently
    // accurate.
    if (!_current_soft_ref_policy->should_clear_reference(obj, _soft_ref_timestamp_clock)) {
      return false;
    }
  }

  ResourceMark rm;      // Needed for tracing.

  HeapWord* const discovered_addr = java_lang_ref_Reference::discovered_addr_raw(obj);
  const oop  discovered = java_lang_ref_Reference::discovered(obj);
  assert(oopDesc::is_oop_or_null(discovered), "Expected an oop or null for discovered field at " PTR_FORMAT, p2i(discovered));
  if (discovered != nullptr) {
    // The reference has already been discovered...
    log_develop_trace(gc, ref)("Already discovered reference (" PTR_FORMAT ": %s)",
                               p2i(obj), obj->klass()->internal_name());

    // Encountering an already-discovered non-strong ref because G1 can restart
    // concurrent marking on marking-stack overflow. Must continue to treat
    // this non-strong ref as discovered to avoid keeping the referent
    // unnecessarily alive.
    assert(UseG1GC, "inv");
    assert(_discovery_is_concurrent, "inv");
    return true;
  }

  // Get the right type of discovered queue head.
  DiscoveredList* list = get_discovered_list(rt);
  add_to_discovered_list(*list, obj, discovered_addr);

  assert(oopDesc::is_oop(obj), "Discovered a bad reference");
  verify_referent(obj);
  return true;
}

void ReferenceProcessor::preclean_discovered_references(BoolObjectClosure* is_alive,
                                                        EnqueueDiscoveredFieldClosure* enqueue,
                                                        YieldClosure* yield,
                                                        GCTimer* gc_timer) {
  // These lists can be handled here in any order and, indeed, concurrently.

  // Soft references
  {
    GCTraceTime(Debug, gc, ref) tm("Preclean SoftReferences", gc_timer);
    log_reflist("SoftRef before: ", _discoveredSoftRefs, _max_num_queues);
    for (uint i = 0; i < _max_num_queues; i++) {
      if (yield->should_return()) {
        return;
      }
      if (preclean_discovered_reflist(_discoveredSoftRefs[i], is_alive,
                                      enqueue, yield)) {
        log_reflist("SoftRef abort: ", _discoveredSoftRefs, _max_num_queues);
        return;
      }
    }
    log_reflist("SoftRef after: ", _discoveredSoftRefs, _max_num_queues);
  }

  // Weak references
  {
    GCTraceTime(Debug, gc, ref) tm("Preclean WeakReferences", gc_timer);
    log_reflist("WeakRef before: ", _discoveredWeakRefs, _max_num_queues);
    for (uint i = 0; i < _max_num_queues; i++) {
      if (yield->should_return()) {
        return;
      }
      if (preclean_discovered_reflist(_discoveredWeakRefs[i], is_alive,
                                      enqueue, yield)) {
        log_reflist("WeakRef abort: ", _discoveredWeakRefs, _max_num_queues);
        return;
      }
    }
    log_reflist("WeakRef after: ", _discoveredWeakRefs, _max_num_queues);
  }

  // Final references
  {
    GCTraceTime(Debug, gc, ref) tm("Preclean FinalReferences", gc_timer);
    log_reflist("FinalRef before: ", _discoveredFinalRefs, _max_num_queues);
    for (uint i = 0; i < _max_num_queues; i++) {
      if (yield->should_return()) {
        return;
      }
      if (preclean_discovered_reflist(_discoveredFinalRefs[i], is_alive,
                                      enqueue, yield)) {
        log_reflist("FinalRef abort: ", _discoveredFinalRefs, _max_num_queues);
        return;
      }
    }
    log_reflist("FinalRef after: ", _discoveredFinalRefs, _max_num_queues);
  }

  // Phantom references
  {
    GCTraceTime(Debug, gc, ref) tm("Preclean PhantomReferences", gc_timer);
    log_reflist("PhantomRef before: ", _discoveredPhantomRefs, _max_num_queues);
    for (uint i = 0; i < _max_num_queues; i++) {
      if (yield->should_return()) {
        return;
      }
      if (preclean_discovered_reflist(_discoveredPhantomRefs[i], is_alive,
                                      enqueue, yield)) {
        log_reflist("PhantomRef abort: ", _discoveredPhantomRefs, _max_num_queues);
        return;
      }
    }
    log_reflist("PhantomRef after: ", _discoveredPhantomRefs, _max_num_queues);
  }
}

bool ReferenceProcessor::preclean_discovered_reflist(DiscoveredList&    refs_list,
                                                     BoolObjectClosure* is_alive,
                                                     EnqueueDiscoveredFieldClosure* enqueue,
                                                     YieldClosure*      yield) {
  DiscoveredListIterator iter(refs_list, nullptr /* keep_alive */, is_alive, enqueue);
  while (iter.has_next()) {
    if (yield->should_return_fine_grain()) {
      return true;
    }
    iter.load_ptrs(DEBUG_ONLY(true /* allow_null_referent */));
    if (iter.referent() == nullptr) {
      log_preclean_ref(iter, "cleared");
      iter.remove();
      iter.move_to_next();
    } else if (iter.is_referent_alive()) {
      log_preclean_ref(iter, "reachable");
      iter.remove();
      iter.move_to_next();
    } else {
      iter.next();
    }
  }

  if (iter.processed() > 0) {
    log_develop_trace(gc, ref)(" Dropped %zu Refs out of %zu Refs in discovered list " PTR_FORMAT,
                               iter.removed(), iter.processed(), p2i(&refs_list));
  }
  return false;
}

const char* ReferenceProcessor::list_name(uint i) {
   assert(i <= _max_num_queues * number_of_subclasses_of_ref(),
          "Out of bounds index");

   int j = i / _max_num_queues;
   switch (j) {
     case 0: return "SoftRef";
     case 1: return "WeakRef";
     case 2: return "FinalRef";
     case 3: return "PhantomRef";
   }
   ShouldNotReachHere();
   return nullptr;
}

uint RefProcMTDegreeAdjuster::ergo_proc_thread_count(size_t ref_count,
                                                     uint max_threads,
                                                     RefProcPhases phase) const {
  assert(0 < max_threads, "must allow at least one thread");

  if (use_max_threads(phase) || (ReferencesPerThread == 0)) {
    return max_threads;
  }

  size_t thread_count = 1 + (ref_count / ReferencesPerThread);
  return (uint)MIN3(thread_count,
                    static_cast<size_t>(max_threads),
                    (size_t)os::active_processor_count());
}

bool RefProcMTDegreeAdjuster::use_max_threads(RefProcPhases phase) const {
  // Even a small number of references in this phase could produce large amounts of work.
  return phase == ReferenceProcessor::KeepAliveFinalRefsPhase;
}

RefProcMTDegreeAdjuster::RefProcMTDegreeAdjuster(ReferenceProcessor* rp,
                                                 RefProcPhases phase,
                                                 uint num_active_workers,
                                                 size_t ref_count):
    _rp(rp),
    _saved_num_queues(_rp->num_queues()) {
  uint workers = ergo_proc_thread_count(ref_count, num_active_workers, phase);
  _rp->set_active_mt_degree(workers);
}

RefProcMTDegreeAdjuster::~RefProcMTDegreeAdjuster() {
  // Revert to previous status.
  _rp->set_active_mt_degree(_saved_num_queues);
}
