import PhaseTime.FixedCell
import PhaseTime.Concurrency

namespace PhaseTime.WriteOptimization

/-! Refinement of the cell writer, before native optimization. Option models
the native missing-value sentinel. Replacement timestamps are relative to base.
The arbitrary slot map deliberately permits ring aliases. All arithmetic is
exact; native floating-point evaluation remains the existing boundary. -/

structure RawCell where
  base : Option ℝ
  fallbackEnd : Option ℝ
  cut : Option ℝ
  replacementStart : ℝ
  replacementEnd : Option ℝ

structure VisibleCell where
  base : Option ℝ
  fallbackEnd : Option ℝ
  replacement : Option (ℝ × ℝ × Option ℝ)

def RawCell.visible (c : RawCell) : VisibleCell :=
  ⟨c.base, c.fallbackEnd, c.cut.map (fun p => (p, c.replacementStart, c.replacementEnd))⟩

def RawCell.enterReference (_c : RawCell) (time : ℝ) : RawCell :=
  ⟨some time, none, none, 0, none⟩

-- @cpp cell_enter: inactive replacement bytes need not be cleared.
def RawCell.enter (c : RawCell) (time : ℝ) : RawCell :=
  { c with base := some time, fallbackEnd := none, cut := none }

def RawCell.finish (c : RawCell) (time : ℝ) : RawCell :=
  if c.fallbackEnd.isNone then { c with fallbackEnd := some time }
  else { c with replacementEnd := c.base.map (fun base => time - base) }

-- @cpp cell_complete: a whole traversal directly stores its two endpoints.
def RawCell.complete (c : RawCell) (left right : ℝ) : RawCell :=
  { c with base := some left, fallbackEnd := some right, cut := none }

def RawCell.activate (c : RawCell) (phase time : ℝ) : RawCell :=
  { c with cut := some phase, replacementStart := time, replacementEnd := none }

theorem skipped_clears_invisible (c : RawCell) (time : ℝ) :
    (c.enter time).visible = (c.enterReference time).visible := rfl

theorem inactive_payload_irrelevant (c : RawCell) (start : ℝ) (stop : Option ℝ)
    (absent : c.cut = none) :
    ({ c with replacementStart := start, replacementEnd := stop }).visible = c.visible := by
  simp [RawCell.visible, absent]

theorem activation_initializes (c : RawCell) (phase time : ℝ) :
    (c.activate phase time).visible.replacement = some (phase, time, none) := rfl

theorem direct_complete_eq_enter_finish (c : RawCell) (left right : ℝ) :
    (c.enter left).finish right = c.complete left right := rfl

theorem complete_reference_equivalent (c : RawCell) (left right : ℝ) :
    (c.complete left right).visible = ((c.enterReference left).finish right).visible := rfl

theorem same_visible_preserves_finish (a b : RawCell) (time : ℝ)
    (same : a.visible = b.visible) :
    (a.finish time).visible = (b.finish time).visible := by
  have base := congrArg VisibleCell.base same
  have stop := congrArg VisibleCell.fallbackEnd same
  have replacement := congrArg VisibleCell.replacement same
  rcases a with ⟨ab, ae, ac, ast, ar⟩
  rcases b with ⟨bb, be, bc, bst, br⟩
  simp only [RawCell.visible] at base stop replacement
  subst bb
  subst be
  cases ae <;> cases ac <;> cases bc <;> simp_all [RawCell.finish, RawCell.visible]

theorem same_visible_preserves_activation (a b : RawCell) (phase time : ℝ)
    (same : a.visible = b.visible) :
    (a.activate phase time).visible = (b.activate phase time).visible := by
  have base := congrArg VisibleCell.base same
  have stop := congrArg VisibleCell.fallbackEnd same
  simp only [RawCell.activate, RawCell.visible] at *
  simp_all

-- Every native read factors through these visible fields: cut absence gates
-- both relative-time loads. Environment contains seed, frontier and cutoff.
theorem same_visible_preserves_every_read {Environment Result : Type}
    (read : VisibleCell → Environment → Result) (environment : Environment)
    (a b : RawCell) (same : a.visible = b.visible) :
    read a.visible environment = read b.visible environment := congrArg (fun c => read c environment) same

variable {Slot : Type} [DecidableEq Slot]

structure Storage (Slot : Type) where
  cells : Slot → RawCell
  seed : Option Slot

def retireSeed (seed : Option Slot) (slot : Slot) : Option Slot :=
  if seed = some slot then none else seed

def enter (s : Storage Slot) (slot : Slot) (time : ℝ) : Storage Slot :=
  ⟨Function.update s.cells slot ((s.cells slot).enter time), retireSeed s.seed slot⟩

def finish (s : Storage Slot) (slot : Slot) (time : ℝ) : Storage Slot :=
  { s with cells := Function.update s.cells slot ((s.cells slot).finish time) }

def complete (s : Storage Slot) (slot : Slot) (left right : ℝ) : Storage Slot :=
  ⟨Function.update s.cells slot ((s.cells slot).complete left right), retireSeed s.seed slot⟩

def Equivalent (a b : Storage Slot) : Prop :=
  a.seed = b.seed ∧ ∀ slot, (a.cells slot).visible = (b.cells slot).visible

def enterReference (s : Storage Slot) (slot : Slot) (time : ℝ) : Storage Slot :=
  ⟨Function.update s.cells slot ((s.cells slot).enterReference time), retireSeed s.seed slot⟩

theorem enter_refines_reference (a b : Storage Slot) (slot : Slot) (time : ℝ)
    (same : Equivalent a b) : Equivalent (enter a slot time) (enterReference b slot time) := by
  constructor
  · simp only [enter, enterReference, same.1]
  · intro query
    by_cases h : query = slot
    · subst query
      simp [enter, enterReference, RawCell.enter, RawCell.enterReference, RawCell.visible]
    · simpa [enter, enterReference, Function.update, h] using same.2 query

theorem finish_preserves_equivalence (a b : Storage Slot) (slot : Slot) (time : ℝ)
    (same : Equivalent a b) : Equivalent (finish a slot time) (finish b slot time) := by
  constructor
  · exact same.1
  · intro query
    by_cases h : query = slot
    · subst query
      simpa [finish] using same_visible_preserves_finish (a.cells slot) (b.cells slot) time (same.2 slot)
    · simpa [finish, Function.update, h] using same.2 query

theorem finish_enter_eq_complete (s : Storage Slot) (slot : Slot) (left right : ℝ) :
    finish (enter s slot left) slot right = complete s slot left right := by
  simp [finish, enter, complete, direct_complete_eq_enter_finish]

-- A prepared edge has its own grid index/time. Its slot can alias any earlier
-- slot: no distinctness assumption appears in the fusion or traversal proof.
structure Edge (Slot : Type) where
  slot : Slot
  phase : ℝ
  time : ℝ

structure Published (Slot : Type) where
  storage : Storage Slot
  frontier : ℝ × ℝ

def setFrontier (s : Storage Slot) (edge : Edge Slot) : Published Slot :=
  ⟨s, (edge.phase, edge.time)⟩

-- Scalar reference after the first edge: finish the current open cell, enter
-- the next, publish its frontier, and repeat.
def scalarTail (s : Storage Slot) (current : Edge Slot) : List (Edge Slot) → Published Slot
  | [] => setFrontier s current
  | next :: rest => scalarTail (enter (finish s current.slot next.time) next.slot next.time) next rest

def referenceTail (s : Storage Slot) (current : Edge Slot) : List (Edge Slot) → Published Slot
  | [] => setFrontier s current
  | next :: rest => referenceTail (enterReference (finish s current.slot next.time) next.slot next.time) next rest

theorem traversal_refines_reference (a b : Storage Slot) (first : Edge Slot)
    (rest : List (Edge Slot)) (same : Equivalent a b) :
    Equivalent (scalarTail a first rest).storage (referenceTail b first rest).storage ∧
      (scalarTail a first rest).frontier = (referenceTail b first rest).frontier := by
  induction rest generalizing a b first with
  | nil => exact ⟨same, rfl⟩
  | cons next rest ih =>
    exact ih _ _ next (enter_refines_reference _ _ next.slot next.time
      (finish_preserves_equivalence a b first.slot next.time same))

-- Optimized batch: directly assign each complete interior cell; leave exactly
-- the final cell open and publish the final frontier once.
def directBatch (s : Storage Slot) (current : Edge Slot) : List (Edge Slot) → Published Slot
  | [] => setFrontier (enter s current.slot current.time) current
  | next :: rest => directBatch (complete s current.slot current.time next.time) next rest

theorem direct_batch_eq_scalar (s : Storage Slot) (first : Edge Slot) (rest : List (Edge Slot)) :
    directBatch s first rest = scalarTail (enter s first.slot first.time) first rest := by
  induction rest generalizing s first with
  | nil => rfl
  | cons next rest ih =>
    simp only [directBatch, scalarTail]
    rw [finish_enter_eq_complete, ih]

-- Chunk boundaries are arbitrary. Continuing the same scalar traversal after
-- a committed prefix is exactly the unsplit traversal.
def lastEdge (first : Edge Slot) : List (Edge Slot) → Edge Slot
  | [] => first
  | next :: rest => lastEdge next rest

theorem scalar_append (s : Storage Slot) (first : Edge Slot)
    (initialEdges suffix : List (Edge Slot)) :
    scalarTail s first (initialEdges ++ suffix) =
      scalarTail (scalarTail s first initialEdges).storage (lastEdge first initialEdges) suffix := by
  induction initialEdges generalizing s first with
  | nil => rfl
  | cons next initialEdges ih =>
    exact ih (enter (finish s first.slot next.time) next.slot next.time) next

theorem committed_batch_is_prefix (s : Storage Slot) (first : Edge Slot)
    (initialEdges : List (Edge Slot)) :
    directBatch s first initialEdges = scalarTail (enter s first.slot first.time) first initialEdges :=
  direct_batch_eq_scalar s first initialEdges

-- The initial edge may finish an existing replacement or leave it untouched
-- on an exact lower endpoint. Both algorithms share that boundary operation.
theorem first_edge_boundary_preserved (s : Storage Slot) (previous : Slot)
    (first : Edge Slot) (rest : List (Edge Slot)) (finishPrevious : Bool) :
    directBatch (if finishPrevious then finish s previous first.time else s) first rest =
      scalarTail (enter (if finishPrevious then finish s previous first.time else s)
        first.slot first.time) first rest := direct_batch_eq_scalar _ _ _

-- @cpp write_batch: the native section uses at most capacity prepared edges.
def batchCount (capacity remaining : Nat) : Nat := min capacity remaining

theorem batch_bounded (capacity remaining : Nat) : batchCount capacity remaining ≤ capacity :=
  Nat.min_le_left _ _

-- Each entered/completed cell has three payload stores and at most one seed
-- retirement. Add one predecessor endpoint, two frontier stores, two revisions.
-- This is a store-count bound, not a scheduler or wall-clock latency bound.
def batchStoreBound (count : Nat) : Nat := 4 * count + 5

theorem batch_store_bound (capacity remaining : Nat) :
    batchStoreBound (batchCount capacity remaining) ≤ 4 * capacity + 5 := by
  unfold batchStoreBound
  exact Nat.add_le_add_right (Nat.mul_le_mul_left 4 (batch_bounded capacity remaining)) 5

theorem batch_progress (capacity remaining : Nat) (hc : 0 < capacity) (hr : 0 < remaining) :
    0 < batchCount capacity remaining ∧ remaining - batchCount capacity remaining < remaining := by
  have hn : 0 < batchCount capacity remaining := lt_min hc hr
  exact ⟨hn, Nat.sub_lt hr hn⟩

-- Existing SC snapshot coherence applies to any finite bounded collection of
-- payload stores inside a revision, including direct-cell batches.
theorem batched_snapshot_coherent {Field Value : Type}
    (before after : Concurrency.State Field Value)
    (observed : Field → Concurrency.State Field Value)
    (ordered : ∀ field, Concurrency.Steps before (observed field) ∧
      Concurrency.Steps (observed field) after)
    (even : before.revision % 2 = 0) (same : before.revision = after.revision) :
    (fun field => (observed field).payload field) = before.payload :=
  Concurrency.accepted_fields_coherent before after observed ordered even same

end PhaseTime.WriteOptimization
