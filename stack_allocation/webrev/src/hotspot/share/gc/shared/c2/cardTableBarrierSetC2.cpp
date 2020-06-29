/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "ci/ciUtilities.hpp"
#include "gc/shared/cardTable.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "gc/shared/c2/cardTableBarrierSetC2.hpp"
#include "opto/arraycopynode.hpp"
#include "opto/graphKit.hpp"
#include "opto/idealKit.hpp"
#include "opto/macro.hpp"
#include "opto/rootnode.hpp"
#include "utilities/macros.hpp"

#define __ ideal.

Node* CardTableBarrierSetC2::byte_map_base_node(GraphKit* kit) const {
  // Get base of card map
  CardTable::CardValue* card_table_base = ci_card_table_address();
   if (card_table_base != NULL) {
     return kit->makecon(TypeRawPtr::make((address)card_table_base));
   } else {
     return kit->null();
   }
}

// vanilla post barrier
// Insert a write-barrier store.  This is to let generational GC work; we have
// to flag all oop-stores before the next GC point.
void CardTableBarrierSetC2::post_barrier(GraphKit* kit,
                                         Node* ctl,
                                         Node* oop_store,
                                         Node* obj,
                                         Node* adr,
                                         uint  adr_idx,
                                         Node* val,
                                         BasicType bt,
                                         bool use_precise) const {
  // No store check needed if we're storing a NULL or an old object
  // (latter case is probably a string constant). The concurrent
  // mark sweep garbage collector, however, needs to have all nonNull
  // oop updates flagged via card-marks.
  if (val != NULL && val->is_Con()) {
    // must be either an oop or NULL
    const Type* t = val->bottom_type();
    if (t == TypePtr::NULL_PTR || t == Type::TOP)
      // stores of null never (?) need barriers
      return;
  }

  if (use_ReduceInitialCardMarks()
      && obj == kit->just_allocated_object(kit->control())) {
    // We can skip marks on a freshly-allocated object in Eden.
    // Keep this code in sync with new_deferred_store_barrier() in runtime.cpp.
    // That routine informs GC to take appropriate compensating steps,
    // upon a slow-path allocation, so as to make this card-mark
    // elision safe.
    return;
  }

  if (!use_precise) {
    // All card marks for a (non-array) instance are in one place:
    adr = obj;
  }
  // (Else it's an array (or unknown), and we want more precise card marks.)
  assert(adr != NULL, "");

  IdealKit ideal(kit, true);

  BarrierSet* bs = BarrierSet::barrier_set();
  CardTableBarrierSet* ctbs = barrier_set_cast<CardTableBarrierSet>(bs);
  CardTable* ct = ctbs->card_table();

  float likely = PROB_LIKELY_MAG(3);

  // Convert the pointer to an int prior to doing math on it
  Node* cast = __ CastPX(__ ctrl(), adr);

  // Divide by card size
  Node* card_offset = __ URShiftX( cast, __ ConI(CardTable::card_shift) );

  // Combine card table base and card offset
  Node* card_adr = __ AddP(__ top(), byte_map_base_node(kit), card_offset );

  // Get the alias_index for raw card-mark memory
  int adr_type = Compile::AliasIdxRaw;
  Node*   zero = __ ConI(0); // Dirty card value

  if (kit->C->do_stack_allocation()) {
    // Stack allocation: cache CastP2XNode for later processing
    state()->add_enqueue_barrier(static_cast<CastP2XNode*>(cast));

    Node* low_off = kit->longcon(ct->byte_map_bottom_offset());
    Node* delta_off = kit->longcon(ct->byte_map_top_offset() - ct->byte_map_bottom_offset());
    Node* sub_off = __ SubL(cast, low_off);

    __ uif_then(sub_off, BoolTest::le, delta_off, likely); } {

      if (UseCondCardMark) {
        if (ct->scanned_concurrently()) {
          kit->insert_mem_bar(Op_MemBarVolatile, oop_store);
          __ sync_kit(kit);
        }
        // The classic GC reference write barrier is typically implemented
        // as a store into the global card mark table.  Unfortunately
        // unconditional stores can result in false sharing and excessive
        // coherence traffic as well as false transactional aborts.
        // UseCondCardMark enables MP "polite" conditional card mark
        // stores.  In theory we could relax the load from ctrl() to
        // no_ctrl, but that doesn't buy much latitude.
        Node* card_val = __ load( __ ctrl(), card_adr, TypeInt::BYTE, T_BYTE, adr_type);
        __ if_then(card_val, BoolTest::ne, zero);
      }

      // Smash zero into card
      if(!ct->scanned_concurrently()) {
        __ store(__ ctrl(), card_adr, zero, T_BYTE, adr_type, MemNode::unordered);
      } else {
        // Specialized path for CM store barrier
        __ storeCM(__ ctrl(), card_adr, zero, oop_store, adr_idx, T_BYTE, adr_type);
      }

      if (UseCondCardMark) {
        __ end_if();
      }
  } if (kit->C->do_stack_allocation()) {
    __ end_if();
  }

  // Final sync IdealKit and GraphKit.
  kit->final_sync(ideal);
}

void CardTableBarrierSetC2::clone(GraphKit* kit, Node* src, Node* dst, Node* size, bool is_array) const {
  BarrierSetC2::clone(kit, src, dst, size, is_array);
  const TypePtr* raw_adr_type = TypeRawPtr::BOTTOM;

  // If necessary, emit some card marks afterwards.  (Non-arrays only.)
  bool card_mark = !is_array && !use_ReduceInitialCardMarks();
  if (card_mark) {
    assert(!is_array, "");
    // Put in store barrier for any and all oops we are sticking
    // into this object.  (We could avoid this if we could prove
    // that the object type contains no oop fields at all.)
    Node* no_particular_value = NULL;
    Node* no_particular_field = NULL;
    int raw_adr_idx = Compile::AliasIdxRaw;
    post_barrier(kit, kit->control(),
                 kit->memory(raw_adr_type),
                 dst,
                 no_particular_field,
                 raw_adr_idx,
                 no_particular_value,
                 T_OBJECT,
                 false);
  }
}

bool CardTableBarrierSetC2::use_ReduceInitialCardMarks() const {
  return ReduceInitialCardMarks;
}

bool CardTableBarrierSetC2::is_gc_barrier_node(Node* node) const {
  return ModRefBarrierSetC2::is_gc_barrier_node(node) || node->Opcode() == Op_StoreCM;
}

bool CardTableBarrierSetC2::process_barrier_node(Node* node, PhaseIterGVN& igvn) const {
  assert(node->Opcode() == Op_CastP2X, "ConvP2XNode required");

  // Must have a control node
  if (node->in(0) == NULL) {
    return false;
  }

  Node *addx_node = node->find_out_with(Op_AddX);
  if (addx_node == NULL) {
    return false;
  }

  Node *addx_out = addx_node->unique_out();
  if (addx_out == NULL) {
    return false;
  }

  CmpNode* cmp_node = addx_out->as_Cmp();
  // the input to the CMPX is the card_table_top_offset constant
  Node* cmp_node_in_2_node = cmp_node->in(2);
  if (!cmp_node_in_2_node->is_Con()) {
    return false;
  }

  BarrierSet* bs = BarrierSet::barrier_set();
  CardTableBarrierSet* ctbs = barrier_set_cast<CardTableBarrierSet>(bs);
  CardTable* ct = ctbs->card_table();
  size_t constant = ct->byte_map_top_offset() - ct->byte_map_bottom_offset();

  // Check that the input to this CMP node is the expected constant
  const TypeX* otype = cmp_node_in_2_node->find_intptr_t_type();
  if (otype != NULL && otype->is_con() &&
      size_t(otype->get_con()) != constant) {
    // Constant offset but not the card table size constant so just return
    return false;
  }

  // we can't change the compare or the constant so create a new constant(0) and replace the variable
  Node* cmp_node_in_1_node = cmp_node->in(1);
  ConNode* zeroConstant_node = igvn.makecon(TypeX_ZERO);
  if (cmp_node_in_1_node->_idx == zeroConstant_node->_idx) {
    // we can get here via different nodes - but we only want to change the input once
    return false;
  }

  igvn.rehash_node_delayed(cmp_node);
  int numReplaced = cmp_node->replace_edge(cmp_node_in_1_node, zeroConstant_node);
  assert(numReplaced == 1, "Failed to replace the card_offset with Conx(0)");
  igvn.replace_node(addx_node, igvn.C->top());

  return true;
}

void CardTableBarrierSetC2::eliminate_gc_barrier(PhaseMacroExpand* macro, Node* node) const {
  assert(node->Opcode() == Op_CastP2X, "ConvP2XNode required");
  assert(node->outcnt() <= 2, "node->outcnt() <= 2");

  // Certain loop optimisations may introduce a CastP2X node with
  // ConvL2I in case of an AllocateArray op. Check for that case
  // here and do not attempt to eliminate it as write barrier.
  if (macro->C->do_stack_allocation() && !state()->is_a_barrier(static_cast<CastP2XNode*>(node))) {
    return;
  }

  Node *shift = node->find_out_with(Op_URShiftX);
  Node *addp = shift->unique_out();
  for (DUIterator_Last jmin, j = addp->last_outs(jmin); j >= jmin; --j) {
    Node *mem = addp->last_out(j);
    if (UseCondCardMark && mem->is_Load()) {
      assert(mem->Opcode() == Op_LoadB, "unexpected code shape");
      // The load is checking if the card has been written so
      // replace it with zero to fold the test.
      macro->replace_node(mem, macro->intcon(0));
      continue;
    }
    assert(mem->is_Store(), "store required");
    macro->replace_node(mem, mem->in(MemNode::Memory));
  }

  if (macro->C->do_stack_allocation()) {
    Node *addl_node = node->find_out_with(Op_AddL);
    assert(addl_node != NULL, "stackallocation expects addl");

    Node* cmp_node = addl_node->unique_out();
    assert(cmp_node != NULL && cmp_node->is_Cmp(), "expected unique cmp node");

    macro->replace_node(cmp_node, macro->makecon(TypeInt::CC_EQ));
  }

  // Stack allocation: remove this node from our cache so we don't process it later
  state()->remove_enqueue_barrier(static_cast<CastP2XNode*>(node));
}

bool CardTableBarrierSetC2::array_copy_requires_gc_barriers(bool tightly_coupled_alloc, BasicType type, bool is_clone, ArrayCopyPhase phase) const {
  bool is_oop = is_reference_type(type);
  return is_oop && (!tightly_coupled_alloc || !use_ReduceInitialCardMarks());
}

bool CardTableBarrierSetC2::expand_barriers(Compile* C, PhaseIterGVN& igvn) const {
  // We need to process write barriers for extra checks in case we have stack allocation on
  if (C->do_stack_allocation()) {
    BarrierSetC2State* set_state = state();

    for (int i = 0; i < set_state->enqueue_barriers_count(); i++) {
      Node* n = set_state->enqueue_barrier(i);
      process_barrier_node(n, igvn);
    }

    if (set_state->enqueue_barriers_count()) {
      // this kicks in the dead code elimination we need to remove the redundant check
      igvn.optimize();
    }
  }

  return false;
}

Node* CardTableBarrierSetC2::step_over_gc_barrier(Node* c) const {
  if (Compile::current()->do_stack_allocation() &&
      !use_ReduceInitialCardMarks() &&
      c != NULL && c->is_Region() && c->req() == 3) {

    //                  [Proj] <----------- step over to here and return
    //                    |
    //               -----------
    //              /           \
    //             /             \
    //            /             [CastP2X]
    //            |            /
    //            |           [AddL]
    //            |          /
    //            |         [CmpUL]
    //            |        /
    //            \      [Bool]
    //             \    /
    //              [If]
    //            /     \
    //  [IfFalse]        [IfTrue]
    //         \        /
    //          [Region] <---------------- c node

    Node* if_bool = c->in(1);
    assert(if_bool->is_IfTrue() || if_bool->is_IfFalse(), "Invalid gc graph pattern");
    Node* if_node = if_bool->in(0);
    Node* proj_node = if_node->in(0);
    assert(proj_node->is_Proj(), "Invalid gc graph pattern");
    return proj_node;
  }
  return c;
}

void CardTableBarrierSetC2::register_potential_barrier_node(Node* node) const {
  if (node->Opcode() == Op_CastP2X) {
    state()->add_enqueue_barrier(static_cast<CastP2XNode*>(node));
  }
}

void CardTableBarrierSetC2::unregister_potential_barrier_node(Node* node) const {
  if (node->Opcode() == Op_CastP2X) {
    state()->remove_enqueue_barrier(static_cast<CastP2XNode*>(node));
  }
}

BarrierSetC2State* CardTableBarrierSetC2::state() const {
  BarrierSetC2State* ret = reinterpret_cast<BarrierSetC2State*>(Compile::current()->barrier_set_state());
  assert(ret != NULL, "Sanity");
  return ret;
}

void* CardTableBarrierSetC2::create_barrier_state(Arena* comp_arena) const {
  return new(comp_arena) BarrierSetC2State(comp_arena);
}

BarrierSetC2State::BarrierSetC2State(Arena* comp_arena)
  : _enqueue_barriers(new (comp_arena) GrowableArray<CastP2XNode*>(comp_arena, 8,  0, NULL)) {
}

int BarrierSetC2State::enqueue_barriers_count() const {
  return _enqueue_barriers->length();
}

CastP2XNode* BarrierSetC2State::enqueue_barrier(int idx) const {
  return _enqueue_barriers->at(idx);
}

void BarrierSetC2State::add_enqueue_barrier(CastP2XNode* n) {
  assert(!_enqueue_barriers->contains(n), "duplicate entry in barrier list");
  _enqueue_barriers->append(n);
}

void BarrierSetC2State::remove_enqueue_barrier(CastP2XNode* n) {
  if (_enqueue_barriers->contains(n)) {
    _enqueue_barriers->remove(n);
  }
}

bool BarrierSetC2State::is_a_barrier(CastP2XNode* n) {
  return _enqueue_barriers->contains(n);
}
