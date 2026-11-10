import PhaseTime.AnalysisStability
import Mathlib.Topology.Order.IntermediateValue

namespace PhaseTime.FixedCell

open Classical
open InverseGrid (Point Piece)
open AnalysisStability (Confined)

/-! A cell keeps a fallback traversal and at most one replacement traversal.
Starting a separate replacement visit discards the older interior island.
Within a confined visit, restart keeps its minimum cut and earlier time on ties. The fallback's
full endpoints are preserved, rather than extrapolating a clipped old prefix.
An unfinished first traversal has no replacement: its entry and forward frontier
bound a time interval confined to the cell, including any tiny internal reversals.
This model is exact arithmetic over the continuous, linearly sampled trajectory.
The existing Fourier chord and codec lemmas account for the separate perturbations. -/

def Covers (s : Piece) (q : ℝ) : Prop := s.left.phase ≤ q ∧ q ≤ s.right.phase

structure Certified (F : ℝ → ℝ) (lo hi now : ℝ) (s : Piece) : Prop where
  phaseOrder : s.left.phase ≤ s.right.phase
  timeOrder : s.left.time ≤ s.right.time
  causal : s.right.time ≤ now
  leftBound : lo ≤ s.left.phase
  rightBound : s.right.phase ≤ hi
  leftExact : F s.left.time = s.left.phase
  rightExact : F s.right.time = s.right.phase
  continuous : ContinuousOn F (Set.Icc s.left.time s.right.time)
  confined : Confined F s.left.time s.right.time lo hi

-- Exact endpoints, including the degenerate entry point, remain exact reads.
noncomputable def interpolate (s : Piece) (q : ℝ) : ℝ :=
  if q = s.right.phase then s.right.time else s.read q

theorem interpolation_time_bounds (s : Piece) (q : ℝ)
    (hp : s.left.phase ≤ s.right.phase) (ht : s.left.time ≤ s.right.time)
    (hq : Covers s q) :
    s.left.time ≤ interpolate s q ∧ interpolate s q ≤ s.right.time := by
  unfold interpolate
  split_ifs with he
  · exact ⟨ht, le_refl _⟩
  · have hs : s.left.phase < s.right.phase := by
      rcases hq with ⟨hl, hr⟩
      by_contra hn
      have h := le_antisymm hp (le_of_not_gt hn)
      apply he
      linarith
    exact InverseGrid.interpolation_inside s q hs ht hq

theorem interpolation_phase_error (F : ℝ → ℝ) (lo hi now q : ℝ) (s : Piece)
    (hc : Certified F lo hi now s) (hq : Covers s q) :
    |F (interpolate s q) - q| ≤ hi - lo := by
  have ht := interpolation_time_bounds s q hc.phaseOrder hc.timeOrder hq
  have hf := hc.confined _ ht.1 ht.2
  apply abs_le.mpr
  constructor <;> linarith [hq.1, hq.2, hc.leftBound, hc.rightBound, hf.1, hf.2]

-- A selected traversal has an actual preimage for every covered phase. It need
-- not be the newest preimage and the trajectory inside the cell may reverse.
theorem exact_preimage_exists (F : ℝ → ℝ) (lo hi now q : ℝ) (s : Piece)
    (hc : Certified F lo hi now s) (hq : Covers s q) :
    ∃ t, s.left.time ≤ t ∧ t ≤ s.right.time ∧ F t = q := by
  have hq' : q ∈ Set.Icc (F s.left.time) (F s.right.time) := by
    simpa [hc.leftExact, hc.rightExact, Covers] using hq
  obtain ⟨t, ht, he⟩ := intermediate_value_Icc hc.timeOrder hc.continuous hq'
  exact ⟨t, ht.1, ht.2, he⟩

structure Cell where
  fallback : Piece
  replacement : Option Piece

def Valid (F : ℝ → ℝ) (lo hi now : ℝ) (c : Cell) : Prop :=
  Certified F lo hi now c.fallback ∧
  ∀ s ∈ c.replacement, Certified F lo hi now s ∧
    c.fallback.left.phase ≤ s.left.phase ∧ s.right.phase ≤ c.fallback.right.phase

-- @cpp cell_select: one exact phase split chooses fallback or replacement.
noncomputable def select (c : Cell) (q : ℝ) : Option Piece :=
  match c.replacement with
  | none => if Covers c.fallback q then some c.fallback else none
  | some fresh =>
    if q < fresh.left.phase then
      if Covers c.fallback q then some c.fallback else none
    else if Covers fresh q then some fresh else none

theorem selected_certified (F : ℝ → ℝ) (lo hi now q : ℝ) (c : Cell) (s : Piece)
    (hc : Valid F lo hi now c) (hr : select c q = some s) :
    Certified F lo hi now s ∧ Covers s q := by
  cases he : c.replacement with
  | none =>
    simp only [select, he] at hr
    split_ifs at hr with hq
    · cases hr
      exact ⟨hc.1, hq⟩
  | some fresh =>
    have hf := (hc.2 fresh (by simp [he])).1
    simp only [select, he] at hr
    split_ifs at hr with hp hq hq
    · cases hr
      exact ⟨hc.1, hq⟩
    · cases hr
      exact ⟨hf, hq⟩

noncomputable def read (c : Cell) (q : ℝ) : Option ℝ :=
  (select c q).map (fun s => interpolate s q)

theorem read_sound (F : ℝ → ℝ) (lo hi now q t : ℝ) (c : Cell)
    (hc : Valid F lo hi now c) (hr : read c q = some t) :
    t ≤ now ∧ |F t - q| ≤ hi - lo := by
  unfold read at hr
  cases hs : select c q with
  | none => simp [hs] at hr
  | some s =>
    simp only [hs, Option.map_some, Option.some.injEq] at hr
    subst t
    have hp := selected_certified F lo hi now q c s hc hs
    exact ⟨le_trans (interpolation_time_bounds s q hp.1.phaseOrder hp.1.timeOrder hp.2).2
      hp.1.causal, interpolation_phase_error F lo hi now q s hp.1 hp.2⟩

theorem selected_right_inverse_exists (F : ℝ → ℝ) (lo hi now q : ℝ) (c : Cell) (s : Piece)
    (hc : Valid F lo hi now c) (hr : select c q = some s) :
    ∃ t, t ≤ now ∧ F t = q := by
  have hp := selected_certified F lo hi now q c s hc hr
  obtain ⟨t, _, ht, he⟩ := exact_preimage_exists F lo hi now q s hp.1 hp.2
  exact ⟨t, le_trans ht hp.1.causal, he⟩

-- @cpp cell_replace: old islands are reassigned to the preserved fallback.
def replace (c : Cell) (fresh : Piece) : Cell := ⟨c.fallback, some fresh⟩

theorem replacement_valid (F : ℝ → ℝ) (lo hi now : ℝ) (c : Cell) (fresh : Piece)
    (hc : Valid F lo hi now c) (hf : Certified F lo hi now fresh)
    (hl : c.fallback.left.phase ≤ fresh.left.phase)
    (hr : fresh.right.phase ≤ c.fallback.right.phase) :
    Valid F lo hi now (replace c fresh) := by
  refine ⟨hc.1, ?_⟩
  intro s hs
  simp only [replace, Option.mem_some_iff] at hs
  subst s
  exact ⟨hf, hl, hr⟩

theorem replacement_preserves_fallback (c : Cell) (fresh : Piece) :
    (replace c fresh).fallback = c.fallback := rfl

theorem island_reattributed (c : Cell) (fresh : Piece) (q : ℝ)
    (hq : Covers c.fallback q) (hs : q < fresh.left.phase) :
    read (replace c fresh) q = some (interpolate c.fallback q) := by
  simp [read, select, replace, hs, hq]

theorem fresh_interval_wins (c : Cell) (fresh : Piece) (q : ℝ)
    (hq : Covers fresh q) :
    read (replace c fresh) q = some (interpolate fresh q) := by
  simp [read, select, replace, not_lt.mpr hq.1, hq]

-- @cpp cell_enter: entering through the left edge starts a new fallback.
-- @cpp cell_finish: first exit completes it; subsequent exits complete the replacement.
-- @cpp grid_forward: each crossed cell edge completes and starts traversals.
def firstTraversal (s : Piece) : Cell := ⟨s, none⟩

theorem first_traversal_valid (F : ℝ → ℝ) (lo hi now : ℝ) (s : Piece)
    (hs : Certified F lo hi now s) : Valid F lo hi now (firstTraversal s) := by
  exact ⟨hs, by simp [firstTraversal]⟩

inductive Built (F : ℝ → ℝ) (lo hi now : ℝ) : Cell → Prop
  | first {s : Piece} : Certified F lo hi now s → Built F lo hi now (firstTraversal s)
  | replacement {c : Cell} {fresh : Piece} : Built F lo hi now c →
      Certified F lo hi now fresh → c.fallback.left.phase ≤ fresh.left.phase →
      fresh.right.phase ≤ c.fallback.right.phase → Built F lo hi now (replace c fresh)

theorem built_valid (F : ℝ → ℝ) (lo hi now : ℝ) (c : Cell)
    (h : Built F lo hi now c) : Valid F lo hi now c := by
  induction h with
  | first hs => exact first_traversal_valid F lo hi now _ hs
  | replacement _ hf hl hr ih => exact replacement_valid F lo hi now _ _ ih hf hl hr

theorem built_read_sound (F : ℝ → ℝ) (lo hi now q t : ℝ) (c : Cell)
    (hc : Built F lo hi now c) (hr : read c q = some t) :
    t ≤ now ∧ |F t - q| ≤ hi - lo :=
  read_sound F lo hi now q t c (built_valid F lo hi now c hc) hr

-- Sample intervals and within-cell reversals can be joined without assuming
-- global monotonicity. Crossing the left boundary starts a fresh entry interval.
theorem confined_join (F : ℝ → ℝ) (a b c lo hi : ℝ)
    (hab : Confined F a b lo hi) (hbc : Confined F b c lo hi) :
    Confined F a c lo hi := by
  intro t ha hc
  by_cases ht : t ≤ b
  · exact hab t ha ht
  · exact hbc t (le_of_not_ge ht) hc

theorem affine_phase_confined (lo hi a b alpha : ℝ)
    (ha : lo ≤ a ∧ a ≤ hi) (hb : lo ≤ b ∧ b ≤ hi)
    (hzero : 0 ≤ alpha) (hone : alpha ≤ 1) :
    lo ≤ a + alpha * (b - a) ∧ a + alpha * (b - a) ≤ hi := by
  constructor <;> nlinarith [mul_nonneg hzero (sub_nonneg.mpr hb.1),
    mul_nonneg (sub_nonneg.mpr hone) (sub_nonneg.mpr ha.1),
    mul_nonneg hzero (sub_nonneg.mpr hb.2),
    mul_nonneg (sub_nonneg.mpr hone) (sub_nonneg.mpr ha.2)]

-- @cpp cell_coalesce: retain the earliest minimum during a confined visit.
noncomputable def minimumStart (old fresh : Point) : Point :=
  if fresh.phase < old.phase then fresh else old

noncomputable def restart (old fresh : Piece) (sameVisit : Bool) : Piece :=
  ⟨if sameVisit then minimumStart old.left fresh.left else fresh.left, fresh.right⟩

theorem minimum_start_phase (old fresh : Point) :
    (minimumStart old fresh).phase = min old.phase fresh.phase := by
  unfold minimumStart
  split_ifs with h
  · exact (min_eq_right (le_of_lt h)).symm
  · exact (min_eq_left (le_of_not_gt h)).symm

theorem restart_minimum (old fresh : Piece) :
    (restart old fresh true).left.phase = min old.left.phase fresh.left.phase := by
  simpa [restart] using minimum_start_phase old.left fresh.left

theorem restart_never_raises_cut (old fresh : Piece) :
    (restart old fresh true).left.phase ≤ old.left.phase := by
  rw [restart_minimum]
  exact min_le_left _ _

theorem restart_keeps_origin (old fresh : Piece) (h : old.left.phase ≤ fresh.left.phase) :
    (restart old fresh true).left = old.left := by
  simp [restart, minimumStart, not_lt.mpr h]

theorem restart_new_minimum (old fresh : Piece) (h : fresh.left.phase < old.left.phase) :
    (restart old fresh true).left = fresh.left := by
  simp [restart, minimumStart, h]

theorem restart_separate_visit (old fresh : Piece) : restart old fresh false = fresh := by
  cases fresh
  rfl

theorem restart_uses_frontier (old fresh : Piece) (sameVisit : Bool) :
    (restart old fresh sameVisit).right = fresh.right := rfl

-- Joining the old traversal, descending bridge, and new forward interval is
-- sound even if their phase ranges overlap or touch either cell boundary.
theorem restart_certified (F : ℝ → ℝ) (lo hi now : ℝ) (old fresh : Piece)
    (ho : Certified F lo hi now old) (hf : Certified F lo hi now fresh)
    (ht : old.right.time ≤ fresh.left.time)
    (hcont : ContinuousOn F (Set.Icc old.left.time fresh.right.time))
    (hbridge : Confined F old.right.time fresh.left.time lo hi) :
    Certified F lo hi now (restart old fresh true) := by
  by_cases h : fresh.left.phase < old.left.phase
  · simpa [restart, minimumStart, h] using hf
  · simp only [restart, ↓reduceIte, minimumStart, h]
    exact ⟨le_trans (le_of_not_gt h) hf.phaseOrder,
      le_trans ho.timeOrder (le_trans ht hf.timeOrder), hf.causal,
      ho.leftBound, hf.rightBound, ho.leftExact, hf.rightExact, hcont,
      confined_join F _ _ _ lo hi ho.confined
        (confined_join F _ _ _ lo hi hbridge hf.confined)⟩

theorem restart_cell_valid (F : ℝ → ℝ) (lo hi now : ℝ) (c : Cell) (old fresh : Piece)
    (hc : Valid F lo hi now c) (hm : old ∈ c.replacement)
    (hf : Certified F lo hi now fresh)
    (ht : old.right.time ≤ fresh.left.time)
    (hcont : ContinuousOn F (Set.Icc old.left.time fresh.right.time))
    (hbridge : Confined F old.right.time fresh.left.time lo hi)
    (hl : c.fallback.left.phase ≤ fresh.left.phase)
    (hr : fresh.right.phase ≤ c.fallback.right.phase) :
    Valid F lo hi now (replace c (restart old fresh true)) := by
  have ho := hc.2 old hm
  apply replacement_valid F lo hi now c _ hc
  · exact restart_certified F lo hi now old fresh ho.1 hf ht hcont hbridge
  · rw [restart_minimum]
    exact le_min ho.2.1 hl
  · exact hr

theorem restart_read_sound (F : ℝ → ℝ) (lo hi now q t : ℝ)
    (c : Cell) (old fresh : Piece)
    (hc : Valid F lo hi now c) (hm : old ∈ c.replacement)
    (hf : Certified F lo hi now fresh)
    (ht : old.right.time ≤ fresh.left.time)
    (hcont : ContinuousOn F (Set.Icc old.left.time fresh.right.time))
    (hbridge : Confined F old.right.time fresh.left.time lo hi)
    (hl : c.fallback.left.phase ≤ fresh.left.phase)
    (hr : fresh.right.phase ≤ c.fallback.right.phase)
    (hread : read (replace c (restart old fresh true)) q = some t) :
    t ≤ now ∧ |F t - q| ≤ hi - lo :=
  read_sound F lo hi now q t _
    (restart_cell_valid F lo hi now c old fresh hc hm hf ht hcont hbridge hl hr) hread

-- @cpp cell_capacity: representation size never depends on reversal count.
def pieceCount (c : Cell) : Nat := 1 + if c.replacement.isSome then 1 else 0

theorem at_most_two_pieces (c : Cell) : pieceCount c ≤ 2 := by
  unfold pieceCount
  split <;> omega

-- @cpp grid_memory: fixed per-cell storage uses 32 bytes.
-- Three double arrays (fallback endpoints and cut), two relative float arrays.
def payloadBytes (cells : Nat) : Nat := cells * (3 * 8 + 2 * 4)

theorem fixed_payload_budget : payloadBytes (2^20) = 32 * 2^20 := by
  norm_num [payloadBytes]

end PhaseTime.FixedCell
