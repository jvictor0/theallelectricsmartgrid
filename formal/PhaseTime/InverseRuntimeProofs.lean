import PhaseTime.InverseRuntime
import PhaseTime.WriteOptimization
import Mathlib.Algebra.Order.Archimedean.Real.Basic

namespace PhaseTime.InverseRuntime

noncomputable section
open Classical

local instance exactArithmetic : Arithmetic ℝ where
  toAdd := inferInstance
  toSub := inferInstance
  toMul := inferInstance
  toDiv := inferInstance
  toBEq := ⟨fun a b => decide (a = b)⟩
  lt a b := decide (a < b)
  le a b := decide (a ≤ b)
  floor a := ⌊a⌋
  ceil a := ⌈a⌉
  ofIndex i := i
  Delta := ℝ
  encode base time := time - base
  decode base delta := some (base + delta)

@[local simp] theorem exact_lt (a b : ℝ) : Arithmetic.lt a b = decide (a < b) := rfl
@[local simp] theorem exact_le (a b : ℝ) : Arithmetic.le a b = decide (a ≤ b) := rfl
@[local simp] theorem exact_floor (a : ℝ) : Arithmetic.floor a = ⌊a⌋ := rfl
@[local simp] theorem exact_encode (base time : ℝ) : Arithmetic.encode base time = time - base := rfl

def Point.exact (p : Point ℝ) : InverseGrid.Point := ⟨p.phase, p.time⟩

/-- Before a right endpoint is available, only the exact entry is readable. -/
def Interval.piece (s : Interval ℝ) : InverseGrid.Piece :=
  ⟨s.left.exact, s.rightTime.elim s.left.exact (fun time => ⟨s.rightPhase, time⟩)⟩

theorem read_interval_refines (s : Interval ℝ) (q cutoff t : ℝ)
    (ordered : s.piece.left.phase ≤ s.piece.right.phase)
    (read : readInterval s.left s.rightPhase s.rightTime q cutoff = some t) :
    FixedCell.Covers s.piece q ∧ t ≤ cutoff ∧
      ((q = s.left.phase ∧ t = s.left.time) ∨ t = FixedCell.interpolate s.piece q) := by
  rcases s with ⟨⟨leftPhase, leftTime⟩, rightPhase, rightTime⟩
  by_cases early : q < leftPhase
  · simp [readInterval, readIntervalWith, entryInRange, early] at read
  by_cases late : leftTime ≤ cutoff
  · by_cases atLeft : q = leftPhase
    · subst q
      simp [readInterval, readIntervalWith, entryInRange, late] at read
      subst t
      cases rightTime with
      | none => simp [Interval.piece, Point.exact, FixedCell.Covers, late]
      | some right =>
        simp only [Interval.piece, Point.exact, Option.elim] at ordered ⊢
        exact ⟨⟨le_rfl, ordered⟩, late, Or.inl ⟨True.intro, True.intro⟩⟩
    · cases rightTime with
      | none => simp [readInterval, readIntervalWith, entryInRange, early, atLeft] at read
      | some right =>
        by_cases outside : rightPhase < q
        · simp [readInterval, readIntervalWith, entryInRange, interiorInRange, early, atLeft, outside] at read
        by_cases width : leftPhase < rightPhase
        · simp [readInterval, readIntervalWith, entryInRange, interiorInRange, interpolate, interpolateUsing, early, late,
            atLeft, outside, width] at read
          obtain ⟨within, rfl⟩ := read
          refine ⟨⟨le_of_not_gt early, le_of_not_gt outside⟩, within, Or.inr ?_⟩
          by_cases atRight : q = rightPhase
          · subst q
            simp [Interval.piece, Point.exact, FixedCell.interpolate, ne_of_gt (sub_pos.mpr width)]
          · simp [Interval.piece, Point.exact, FixedCell.interpolate, InverseGrid.Piece.read, atRight]
        · simp [readInterval, readIntervalWith, entryInRange, interiorInRange, early, atLeft, outside, width] at read
  · simp [readInterval, readIntervalWith, entryInRange, late] at read

theorem read_interval_sound (F : ℝ → ℝ) (lo hi now q cutoff t : ℝ) (s : Interval ℝ)
    (certified : FixedCell.Certified F lo hi now s.piece)
    (read : readInterval s.left s.rightPhase s.rightTime q cutoff = some t) :
    t ≤ now ∧ t ≤ cutoff ∧ |F t - q| ≤ hi - lo ∧ ∃ exactTime ≤ now, F exactTime = q := by
  obtain ⟨covers, cutoffBound, atEntry | interpolated⟩ :=
    read_interval_refines s q cutoff t certified.phaseOrder read
  · obtain ⟨rfl, rfl⟩ := atEntry
    have nowBound := certified.timeOrder.trans certified.causal
    have entry : F s.left.time = s.left.phase := certified.leftExact
    have width : 0 ≤ hi - lo := by
      linarith [certified.leftBound, certified.phaseOrder, certified.rightBound]
    exact ⟨nowBound, cutoffBound, by simpa [entry] using width, s.left.time, nowBound, entry⟩
  · subst t
    have bounds := FixedCell.interpolation_time_bounds s.piece q
      certified.phaseOrder certified.timeOrder covers
    obtain ⟨exactTime, _, exactBound, exactPhase⟩ :=
      FixedCell.exact_preimage_exists F lo hi now q s.piece certified covers
    exact ⟨bounds.2.trans certified.causal, cutoffBound,
      FixedCell.interpolation_phase_error F lo hi now q s.piece certified covers,
      exactTime, exactBound.trans certified.causal, exactPhase⟩

theorem read_cell_sound (F : ℝ → ℝ) (lo hi now q cutoff t : ℝ)
    (cell : Cell ℝ ℝ) (seed : Option ℝ) (frontier : Option (Point ℝ)) (index : Int)
    (width : lo ≤ hi)
    (certified : ∀ interval, selectInterval cell seed frontier index q = some interval →
      FixedCell.Certified F lo hi now interval.piece)
    (live : ∀ edge ∈ frontier, F edge.time = edge.phase ∧ edge.time ≤ now)
    (read : readCell cell seed frontier index q cutoff = some t) :
    t ≤ now ∧ t ≤ cutoff ∧ |F t - q| ≤ hi - lo ∧ ∃ exactTime ≤ now, F exactTime = q := by
  have segment (lookup : (selectInterval cell seed frontier index q).bind (fun interval =>
      readInterval interval.left interval.rightPhase interval.rightTime q cutoff) = some t) :
      t ≤ now ∧ t ≤ cutoff ∧ |F t - q| ≤ hi - lo ∧ ∃ exactTime ≤ now, F exactTime = q := by
    cases selected : selectInterval cell seed frontier index q with
    | none => simp [selected] at lookup
    | some interval =>
      exact read_interval_sound F lo hi now q cutoff t interval (certified interval selected)
        (by simpa [selected] using lookup)
  cases frontier with
  | none => exact segment read
  | some edge =>
    by_cases here : q = edge.phase
    · simp [readCell, here, timeWithinCutoff] at read
      obtain ⟨cutoffBound, rfl⟩ := read
      obtain ⟨phase, time⟩ := live edge (by simp)
      exact ⟨time, cutoffBound, by simpa [phase, here] using sub_nonneg.mpr width,
        edge.time, time, phase.trans here.symm⟩
    · apply segment
      simpa [readCell, here] using read

private def abstractCell (c : Cell ℝ ℝ) : WriteOptimization.RawCell :=
  ⟨c.base, c.fallbackEnd, c.replacement.map (·.cut),
    (c.replacement.map (·.start)).getD 0, c.replacement.bind (·.stop)⟩

def abstractStorage {n : Nat} (s : Published ℝ ℝ n) : WriteOptimization.Storage (Fin (n + 1)) :=
  ⟨fun slot => abstractCell (s.cells.get slot), s.seed.map (fun phase => wrap n ⌊phase⌋)⟩

@[local simp] theorem get_set_same {A : Type} {n : Nat}
    (cells : Vector A n) (slot : Fin n) (cell : A) : (cells.set slot.val cell).get slot = cell :=
  Vector.getElem_set_self slot.isLt

@[local simp] theorem get_set_other {A : Type} {n : Nat}
    (cells : Vector A n) (written queried : Fin n) (cell : A) (different : written ≠ queried) :
    (cells.set written.val cell).get queried = cells.get queried :=
  Vector.getElem_set_ne written.isLt queried.isLt (fun h => different (Fin.ext h))

private theorem abstract_retire {n : Nat} (seed : Option ℝ) (slot : Fin (n + 1)) :
    (retireSeed seed slot).map (fun phase => wrap n ⌊phase⌋) =
      WriteOptimization.retireSeed (seed.map (fun phase => wrap n ⌊phase⌋)) slot := by
  cases seed with
  | none => simp [retireSeed, WriteOptimization.retireSeed]
  | some phase =>
    by_cases sameSlot : wrap n ⌊phase⌋ = slot <;>
      simp [retireSeed, WriteOptimization.retireSeed, sameSlot]

theorem assign_cell_refines {n : Nat} (s : Published ℝ ℝ n)
    (reference : WriteOptimization.Storage (Fin (n + 1)))
    (same : WriteOptimization.Equivalent (abstractStorage s) reference) (index : Int) (time : ℝ) (stop : Option ℝ) :
    WriteOptimization.Equivalent (abstractStorage (assignCell s index time stop))
      (match stop with
        | none => WriteOptimization.enter reference (wrap n index) time
        | some right => WriteOptimization.complete reference (wrap n index) time right) := by
  cases stop <;> constructor
  · simpa [assignCell, abstractStorage, WriteOptimization.enter, abstract_retire] using
      congrArg (fun seed => WriteOptimization.retireSeed seed (wrap n index)) same.1
  · intro slot
    by_cases sameSlot : slot = wrap n index
    · subst slot
      simp [assignCell, abstractStorage, abstractCell, WriteOptimization.enter,
        WriteOptimization.RawCell.enter, WriteOptimization.RawCell.visible]
    · have different : wrap n index ≠ slot := Ne.symm sameSlot
      simpa [assignCell, abstractStorage, WriteOptimization.enter, Function.update,
        sameSlot, different] using same.2 slot
  · simpa [assignCell, abstractStorage, WriteOptimization.complete, abstract_retire] using
      congrArg (fun seed => WriteOptimization.retireSeed seed (wrap n index)) same.1
  · intro slot
    by_cases sameSlot : slot = wrap n index
    · subst slot
      simp [assignCell, abstractStorage, abstractCell, WriteOptimization.complete,
        WriteOptimization.RawCell.complete, WriteOptimization.RawCell.visible]
    · have different : wrap n index ≠ slot := Ne.symm sameSlot
      simpa [assignCell, abstractStorage, WriteOptimization.complete, Function.update,
        sameSlot, different] using same.2 slot

theorem finish_cell_refines {n : Nat} (s : Published ℝ ℝ n) (index : Int) (time : ℝ) :
    WriteOptimization.Equivalent (abstractStorage (finishCell s index time))
      (WriteOptimization.finish (abstractStorage s) (wrap n index) time) := by
  refine ⟨rfl, ?_⟩
  intro slot
  by_cases sameSlot : slot = wrap n index
  · subst slot
    simp only [finishCell, abstractStorage, WriteOptimization.finish, Function.update_self, get_set_same]
    rcases s.cells.get (wrap n index) with ⟨base, fallback, branch⟩
    cases base <;> cases fallback <;> cases branch <;>
      simp [abstractCell, WriteOptimization.RawCell.finish, WriteOptimization.RawCell.visible]
  · have different : wrap n index ≠ slot := Ne.symm sameSlot
    simp [finishCell, abstractStorage, WriteOptimization.finish, Function.update, sameSlot, different]

private def abstractEdge (n : Nat) (index : Int) (edge : Point ℝ) :
    WriteOptimization.Edge (Fin (n + 1)) :=
  ⟨wrap n index, edge.phase, edge.time⟩

private def remainingEdges (n : Nat) (left right : Point ℝ) (index : Int) :
    Nat → List (WriteOptimization.Edge (Fin (n + 1)))
  | 0 => []
  | count + 1 => abstractEdge n (index + 1) (edgeAt left right (index + 1)) ::
      remainingEdges n left right (index + 1) count

theorem commit_tail_refines {n : Nat} (s : Published ℝ ℝ n)
    (reference : WriteOptimization.Storage (Fin (n + 1)))
    (same : WriteOptimization.Equivalent (abstractStorage s) reference)
    (left right edge : Point ℝ) (index : Int) (count : Nat) :
    let committed := commitTail s left right edge index count
    let expected := WriteOptimization.directBatch reference (abstractEdge n index edge)
      (remainingEdges n left right index count)
    WriteOptimization.Equivalent (abstractStorage committed) expected.storage ∧
      committed.frontier.map (fun p => (p.phase, p.time)) = some expected.frontier := by
  induction count generalizing s reference edge index with
  | zero =>
    exact ⟨assign_cell_refines s reference same index edge.time none, rfl⟩
  | succ count ih =>
    have refined := assign_cell_refines s reference same index edge.time
      (some (edgeAt left right (index + 1)).time)
    exact ih _ _ refined _ _

theorem committed_prefix {n : Nat} (s : Published ℝ ℝ n)
    (reference : WriteOptimization.Storage (Fin (n + 1)))
    (same : WriteOptimization.Equivalent (abstractStorage s) reference)
    (left right edge : Point ℝ) (index : Int) (count : Nat) :
    let committed := commitTail s left right edge index count
    let first := abstractEdge n index edge
    let scalar := WriteOptimization.scalarTail (WriteOptimization.enter reference first.slot first.time)
      first (remainingEdges n left right index count)
    WriteOptimization.Equivalent (abstractStorage committed) scalar.storage ∧
      committed.frontier.map (fun p => (p.phase, p.time)) = some scalar.frontier := by
  have refined := commit_tail_refines s reference same left right edge index count
  rwa [WriteOptimization.direct_batch_eq_scalar] at refined

theorem commit_batch_is_prefix {n : Nat} (s : Published ℝ ℝ n) (left right : Point ℝ)
    (index : Int) (count : Nat) :
    let committed := commitBatch s left right index count
    let edge := edgeAt left right index
    let first := abstractEdge n index edge
    let reference := abstractStorage s
    let ready := if Arithmetic.lt left.phase edge.phase then
      WriteOptimization.finish reference (wrap n (index - 1)) edge.time else reference
    let scalar := WriteOptimization.scalarTail (WriteOptimization.enter ready first.slot first.time)
      first (remainingEdges n left right index count)
    WriteOptimization.Equivalent (abstractStorage committed) scalar.storage ∧
      committed.frontier.map (fun p => (p.phase, p.time)) = some scalar.frontier := by
  dsimp only [commitBatch]
  split
  · exact committed_prefix _ _ (finish_cell_refines _ _ _) _ _ _ _ _
  · exact committed_prefix _ _ ⟨rfl, fun _ => rfl⟩ _ _ _ _ _

theorem nonforward_keeps_payload {n : Nat} (s : State ℝ ℝ n) (resolution : ℝ)
    (previous current : Point ℝ) (observed : s.previous = some previous)
    (nonforward : current.phase ≤ previous.phase) :
    (write s resolution current).payload = s.payload := by
  simp only [write, observed, exact_lt, decide_eq_true_eq,
    if_neg (not_lt.mpr nonforward)]
  split <;> rfl

theorem keep_cut_refines_restart (frontier rightPhase : ℝ) (old fresh : InverseGrid.Piece) :
    (if keepCut frontier rightPhase fresh.left.phase old.left.phase then old.left else fresh.left) =
      (FixedCell.restart old fresh (decide (frontier ≤ rightPhase))).left := by
  by_cases inside : frontier ≤ rightPhase <;> by_cases deeper : fresh.left.phase < old.left.phase <;>
    simp [keepCut, FixedCell.restart, FixedCell.minimumStart, inside, deeper]

theorem coherent_runtime_read {Field Value : Type} {n : Nat}
    (before after : Concurrency.State Field Value) (observed : Field → Concurrency.State Field Value)
    (ordered : ∀ field, Concurrency.Steps before (observed field) ∧ Concurrency.Steps (observed field) after)
    (even : before.revision % 2 = 0) (same : before.revision = after.revision)
    (decode : (Field → Value) → Published ℝ ℝ n) (query cutoff : ℝ) :
    read (decode (fun field => (observed field).payload field)) query cutoff =
      read (decode before.payload) query cutoff :=
  Concurrency.accepted_computation_coherent before after observed ordered even same
    (fun payload => read (decode payload) query cutoff)

end

end PhaseTime.InverseRuntime
