import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring
import Mathlib.Tactic.FieldSimp

namespace PhaseTime.InverseGrid

/-! The stored inverse is a partition into forward branches. Discontinuities
carry two one-sided timestamps. A live/frozen forward endpoint closes the last
piece. Nothing here assumes a band limit implies separated turning points.
Theorems are exact arithmetic; codec perturbations have separate hypotheses. -/

structure Point where
  phase : ℝ
  time : ℝ

structure Jump where
  phase : ℝ
  before : ℝ
  after : ℝ

structure Piece where
  left : Point
  right : Point

noncomputable def Piece.read (s : Piece) (q : ℝ) : ℝ :=
  s.left.time + (q - s.left.phase) / (s.right.phase - s.left.phase) *
    (s.right.time - s.left.time)

def Piece.Valid (s : Piece) (F : ℝ → ℝ) : Prop :=
  s.left.phase < s.right.phase ∧ s.left.time ≤ s.right.time ∧
  MonotoneOn F (Set.Icc s.left.time s.right.time) ∧
  F s.left.time = s.left.phase ∧ F s.right.time = s.right.phase

def ValidPartition (F : ℝ → ℝ) (left : Point) (jumps : List Jump) (right : Point) : Prop :=
  match jumps with
  | [] => (Piece.mk left right).Valid F
  | j :: rest => (Piece.mk left ⟨j.phase, j.before⟩).Valid F ∧
      ValidPartition F ⟨j.phase, j.after⟩ rest right

-- The two sides of a jump are never used as interpolation endpoints together.
-- Generic reference lemma: select adjacent knots on one side of every recorded jump.
noncomputable def select (left : Point) (jumps : List Jump) (right : Point) (q : ℝ) : Piece :=
  match jumps with
  | [] => ⟨left, right⟩
  | j :: rest => if q < j.phase then ⟨left, ⟨j.phase, j.before⟩⟩
      else select ⟨j.phase, j.after⟩ rest right q

theorem partition_order (F : ℝ → ℝ) (left right : Point) (jumps : List Jump)
    (h : ValidPartition F left jumps right) : left.phase < right.phase := by
  induction jumps generalizing left with
  | nil => exact h.1
  | cons j rest ih =>
    exact lt_trans h.1.1 (ih ⟨j.phase, j.after⟩ h.2)

theorem selected_branch (F : ℝ → ℝ) (left right : Point) (jumps : List Jump) (q : ℝ)
    (h : ValidPartition F left jumps right) (hq : left.phase ≤ q ∧ q ≤ right.phase) :
    (select left jumps right q).Valid F ∧
    (select left jumps right q).left.phase ≤ q ∧
    q ≤ (select left jumps right q).right.phase ∧
    left.phase ≤ (select left jumps right q).left.phase ∧
    (select left jumps right q).right.phase ≤ right.phase := by
  induction jumps generalizing left with
  | nil => exact ⟨h, hq.1, hq.2, le_refl _, le_refl _⟩
  | cons j rest ih =>
    by_cases hj : q < j.phase
    · simp only [select, if_pos hj]
      exact ⟨h.1, hq.1, le_of_lt hj, le_refl _,
        le_of_lt (partition_order F ⟨j.phase, j.after⟩ right rest h.2)⟩
    · simp only [select, if_neg hj]
      have hr := ih ⟨j.phase, j.after⟩ h.2 ⟨le_of_not_gt hj, hq.2⟩
      exact ⟨hr.1, hr.2.1, hr.2.2.1,
        le_trans (le_of_lt h.1.1) hr.2.2.2.1, hr.2.2.2.2⟩

theorem interpolation_inside (s : Piece) (q : ℝ)
    (hp : s.left.phase < s.right.phase) (ht : s.left.time ≤ s.right.time)
    (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase) :
    s.left.time ≤ s.read q ∧ s.read q ≤ s.right.time := by
  have hd : 0 < s.right.phase - s.left.phase := sub_pos.mpr hp
  have ha : 0 ≤ (q - s.left.phase) / (s.right.phase - s.left.phase) :=
    div_nonneg (sub_nonneg.mpr hq.1) (le_of_lt hd)
  have hb : (q - s.left.phase) / (s.right.phase - s.left.phase) ≤ 1 := by
    apply (div_le_one hd).mpr
    linarith [hq.2]
  unfold Piece.read
  constructor <;> nlinarith

theorem branch_phase_bounds (s : Piece) (F : ℝ → ℝ) (q : ℝ)
    (h : s.Valid F) (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase) :
    s.left.phase ≤ F (s.read q) ∧ F (s.read q) ≤ s.right.phase := by
  have ht := interpolation_inside s q h.1 h.2.1 hq
  have hl := h.2.2.1 ⟨le_refl _, h.2.1⟩ ht ht.1
  have hr := h.2.2.1 ht ⟨h.2.1, le_refl _⟩ ht.2
  rw [h.2.2.2.1] at hl
  rw [h.2.2.2.2] at hr
  exact ⟨hl, hr⟩

-- The error depends on cell width, not on the timestamp jump between branches.
theorem phase_error_bound (F : ℝ → ℝ) (left right : Point) (jumps : List Jump) (q width : ℝ)
    (h : ValidPartition F left jumps right) (hq : left.phase ≤ q ∧ q ≤ right.phase)
    (hw : right.phase - left.phase ≤ width) :
    |F ((select left jumps right q).read q) - q| ≤ width := by
  have hs := selected_branch F left right jumps q h hq
  have hb := branch_phase_bounds _ F q hs.1 ⟨hs.2.1, hs.2.2.1⟩
  apply abs_le.mpr
  constructor <;> linarith [hs.2.2.2.1, hs.2.2.2.2, hq.1, hq.2]

theorem branch_time_error (s : Piece) (q exact : ℝ)
    (hp : s.left.phase < s.right.phase) (ht : s.left.time ≤ s.right.time)
    (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase)
    (he : s.left.time ≤ exact ∧ exact ≤ s.right.time) :
    |s.read q - exact| ≤ s.right.time - s.left.time := by
  have hb := interpolation_inside s q hp ht hq
  apply abs_le.mpr
  constructor <;> linarith [he.1, he.2, hb.1, hb.2]

-- A useful uniform time bound needs a separate lower forward-speed hypothesis.
theorem conditioned_time_error (F : ℝ → ℝ) (approx exact q width speed : ℝ)
    (hv : 0 < speed) (he : F exact = q)
    (hc : speed * |approx - exact| ≤ |F approx - F exact|)
    (hr : |F approx - q| ≤ width) : |approx - exact| ≤ width / speed := by
  apply (le_div_iff₀ hv).mpr
  rw [he] at hc
  nlinarith

theorem scope_error (F L S : ℝ → ℝ) (approx q width gain : ℝ)
    (hg : 0 ≤ gain) (signal : ∀ t, S t = L (F t))
    (hl : ∀ x y, |L x - L y| ≤ gain * |x - y|)
    (hr : |F approx - q| ≤ width) : |S approx - L q| ≤ gain * width := by
  rw [signal]
  exact le_trans (hl _ _) (mul_le_mul_of_nonneg_left hr hg)

-- Relative storage is translation-invariant; finite-precision error is separate.
-- @cpp grid_codec: absolute double base plus a relative timestamp payload.
def decode (base delta : ℝ) : ℝ := base + delta

theorem delta_roundtrip (base value : ℝ) : decode base (value - base) = value := by
  unfold decode
  ring

theorem delta_error (base value delta epsilon : ℝ)
    (h : |delta - (value - base)| ≤ epsilon) : |decode base delta - value| ≤ epsilon := by
  have he : decode base delta - value = delta - (value - base) := by
    unfold decode
    ring
  rw [he]
  exact h

-- @cpp grid_frontier: the last forward observation closes the live partial cell.
theorem frontier_exact (s : Piece) (hp : s.left.phase < s.right.phase) :
    s.read s.right.phase = s.right.time := by
  simp [Piece.read, ne_of_gt (sub_pos.mpr hp)]

end PhaseTime.InverseGrid



namespace PhaseTime.InverseGrid

/-! A declared finite spectral operating range in unmodulated phase. Frequencies
are angular frequencies per master cycle, so a limit of B cycles means 2πB.
This is an assumption on the input, not a claim about today's warp generator.
It provides an upper slope bound, NOT a lower slope or local root separation. -/
structure Harmonic where
  amplitude : ℝ
  omega : ℝ
  offset : ℝ

noncomputable def Harmonic.value (h : Harmonic) (u : ℝ) : ℝ :=
  h.amplitude * Real.sin (h.omega * u + h.offset)

noncomputable def wave (hs : List Harmonic) (u : ℝ) : ℝ :=
  (hs.map (fun h => h.value u)).sum

noncomputable def amplitudeBudget (hs : List Harmonic) : ℝ :=
  (hs.map (fun h => |h.amplitude|)).sum

def BandLimited (hs : List Harmonic) (band : ℝ) : Prop :=
  0 ≤ band ∧ ∀ h ∈ hs, |h.omega| ≤ band

theorem harmonic_slope (h : Harmonic) (u v : ℝ) :
    |h.value u - h.value v| ≤ |h.amplitude| * |h.omega| * |u - v| := by
  have hb := Real.abs_sin_sub_sin_le (h.omega * u + h.offset) (h.omega * v + h.offset)
  rw [show h.omega * u + h.offset - (h.omega * v + h.offset) = h.omega * (u - v) by ring,
    abs_mul] at hb
  calc
    |h.value u - h.value v| = |h.amplitude| *
        |Real.sin (h.omega * u + h.offset) - Real.sin (h.omega * v + h.offset)| := by
      rw [← abs_mul]; congr 1; unfold Harmonic.value; ring
    _ ≤ |h.amplitude| * (|h.omega| * |u - v|) :=
      mul_le_mul_of_nonneg_left hb (abs_nonneg _)
    _ = _ := by ring

theorem bandlimited_slope (hs : List Harmonic) (band u v : ℝ) (hb : BandLimited hs band) :
    |wave hs u - wave hs v| ≤ band * amplitudeBudget hs * |u - v| := by
  induction hs with
  | nil => simp [wave, amplitudeBudget]
  | cons h rest ih =>
    have hh := harmonic_slope h u v
    have hf := hb.2 h (by simp)
    have hr := ih ⟨hb.1, fun k hk => hb.2 k (by simp [hk])⟩
    have hm : |h.amplitude| * |h.omega| * |u - v| ≤
        band * |h.amplitude| * |u - v| := by
      have := mul_le_mul_of_nonneg_left hf (abs_nonneg h.amplitude)
      have := mul_le_mul_of_nonneg_right this (abs_nonneg (u - v))
      nlinarith
    change |(h.value u + wave rest u) - (h.value v + wave rest v)| ≤ _
    rw [show (h.value u + wave rest u) - (h.value v + wave rest v) =
      (h.value u - h.value v) + (wave rest u - wave rest v) by ring]
    have ha := abs_add_le (h.value u - h.value v) (wave rest u - wave rest v)
    simp only [amplitudeBudget, List.map_cons, List.sum_cons] at *
    linarith

theorem bandlimited_forward_slope (hs : List Harmonic) (band u v : ℝ)
    (hb : BandLimited hs band) :
    |(u + wave hs u) - (v + wave hs v)| ≤
      (1 + band * amplitudeBudget hs) * |u - v| := by
  have hw := bandlimited_slope hs band u v hb
  rw [show u + wave hs u - (v + wave hs v) = (u - v) + (wave hs u - wave hs v) by ring]
  have ha := abs_add_le (u - v) (wave hs u - wave hs v)
  nlinarith

-- A codec's timestamp perturbation adds a controlled forward-phase residual.
theorem stored_phase_error (F : ℝ → ℝ) (exact stored q width gain epsilon : ℝ)
    (hg : 0 ≤ gain) (hf : ∀ x y, |F x - F y| ≤ gain * |x - y|)
    (he : |F exact - q| ≤ width) (hc : |stored - exact| ≤ epsilon) :
    |F stored - q| ≤ width + gain * epsilon := by
  have h₁ := hf stored exact
  have h₂ := mul_le_mul_of_nonneg_left hc hg
  have h₃ := abs_add_le (F stored - F exact) (F exact - q)
  rw [sub_add_sub_cancel] at h₃
  linarith

-- Storage capacity is independently declared; it is not hidden in BandLimited.
def Fits (capacity : Nat) (jumps : List Jump) : Prop := jumps.length ≤ capacity

theorem capacity_preserved_by_clipping (jumps : List Jump) (q : ℝ) (capacity : Nat)
    (h : Fits capacity jumps) : Fits capacity (jumps.filter (fun j => j.phase < q)) := by
  exact le_trans (List.length_filter_le ..) h

end PhaseTime.InverseGrid

namespace PhaseTime.InverseGrid

/-! The weaker cell-range certificate survives clipping an already interpolated
old branch. Requiring the clipped endpoint to be an exact inverse would be
incorrect: it was calculated by interpolation. The range bound does not grow
with repeated clipping. -/
def Piece.Enclosed (s : Piece) (F : ℝ → ℝ) (lo hi : ℝ) : Prop :=
  s.left.phase < s.right.phase ∧ s.left.time ≤ s.right.time ∧
  MonotoneOn F (Set.Icc s.left.time s.right.time) ∧
  lo ≤ F s.left.time ∧ F s.right.time ≤ hi

theorem enclosed_read (s : Piece) (F : ℝ → ℝ) (lo hi q : ℝ)
    (h : s.Enclosed F lo hi) (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase) :
    lo ≤ F (s.read q) ∧ F (s.read q) ≤ hi := by
  have ht := interpolation_inside s q h.1 h.2.1 hq
  have hl := h.2.2.1 ⟨le_refl _, h.2.1⟩ ht ht.1
  have hr := h.2.2.1 ht ⟨h.2.1, le_refl _⟩ ht.2
  exact ⟨le_trans h.2.2.2.1 hl, le_trans hr h.2.2.2.2⟩

-- Generic reference lemma: capture the old-side limit before replacing a suffix.
noncomputable def Piece.clip (s : Piece) (q : ℝ) : Piece :=
  ⟨s.left, ⟨q, s.read q⟩⟩

theorem clipping_preserves_range (s : Piece) (F : ℝ → ℝ) (lo hi q : ℝ)
    (h : s.Enclosed F lo hi) (hq : s.left.phase < q ∧ q ≤ s.right.phase) :
    (s.clip q).Enclosed F lo hi := by
  have ht := interpolation_inside s q h.1 h.2.1 ⟨le_of_lt hq.1, hq.2⟩
  have hf := enclosed_read s F lo hi q h ⟨le_of_lt hq.1, hq.2⟩
  refine ⟨hq.1, ht.1, ?_, h.2.2.2.1, hf.2⟩
  intro a ha b hb hab
  exact h.2.2.1 ⟨ha.1, le_trans ha.2 ht.2⟩ ⟨hb.1, le_trans hb.2 ht.2⟩ hab

-- Cutting a linear piece and retaining its calculated endpoint preserves all
-- older reads, rather than accumulating another interpolation error.
theorem clipping_preserves_reads (s : Piece) (cut q : ℝ)
    (ha : s.left.phase < cut) (hb : s.left.phase < s.right.phase) :
    (s.clip cut).read q = s.read q := by
  have h₁ : cut - s.left.phase ≠ 0 := ne_of_gt (sub_pos.mpr ha)
  have h₂ : s.right.phase - s.left.phase ≠ 0 := ne_of_gt (sub_pos.mpr hb)
  simp only [Piece.clip, Piece.read]
  field_simp
  ring

-- A forward run replaces a suffix of the readable history, starting at its
-- turnaround; queries beyond the supplied endpoint are unavailable.
-- Generic reference lemma: ascending samples replace their traversed coordinates.
noncomputable def overwrite (old : ℝ → Option ℝ) (fresh : Piece) (q : ℝ) : Option ℝ :=
  if q < fresh.left.phase then old q
  else if q ≤ fresh.right.phase then some (fresh.read q) else none

theorem overwrite_keeps_prefix (old : ℝ → Option ℝ) (fresh : Piece) (q : ℝ)
    (hq : q < fresh.left.phase) : overwrite old fresh q = old q := by
  simp [overwrite, hq]

theorem overwrite_uses_forward (old : ℝ → Option ℝ) (fresh : Piece) (q : ℝ)
    (hq : fresh.left.phase ≤ q ∧ q ≤ fresh.right.phase) :
    overwrite old fresh q = some (fresh.read q) := by
  simp [overwrite, not_lt.mpr hq.1, hq.2]

theorem overwrite_frontier (old : ℝ → Option ℝ) (fresh : Piece)
    (hp : fresh.left.phase < fresh.right.phase) :
    overwrite old fresh fresh.right.phase = some fresh.right.time := by
  rw [overwrite_uses_forward old fresh fresh.right.phase ⟨le_of_lt hp, le_refl _⟩,
    frontier_exact fresh hp]

theorem frontier_rejects_future (old : ℝ → Option ℝ) (fresh : Piece) (q : ℝ)
    (hp : fresh.left.phase < fresh.right.phase) (hq : fresh.right.phase < q) :
    overwrite old fresh q = none := by
  have hn : ¬ q < fresh.left.phase := not_lt.mpr (le_of_lt (lt_trans hp hq))
  simp [overwrite, hn, not_le.mpr hq]

theorem enclosed_phase_error (s : Piece) (F : ℝ → ℝ) (lo hi q width : ℝ)
    (h : s.Enclosed F lo hi) (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase)
    (hc : lo ≤ q ∧ q ≤ hi) (hw : hi - lo ≤ width) :
    |F (s.read q) - q| ≤ width := by
  have hb := enclosed_read s F lo hi q h hq
  apply abs_le.mpr
  constructor <;> linarith [hc.1, hc.2, hb.1, hb.2]

-- Hardware floats are not axiomatized. Given bounded endpoint errors and the
-- same selected branch/fraction, interpolation cannot amplify their maximum.
theorem interpolation_endpoint_error (a b da db alpha epsilon : ℝ)
    (ha : 0 ≤ alpha) (hb : alpha ≤ 1)
    (hda : |da| ≤ epsilon) (hdb : |db| ≤ epsilon) :
    |((1 - alpha) * (a + da) + alpha * (b + db)) -
      ((1 - alpha) * a + alpha * b)| ≤ epsilon := by
  rw [show ((1 - alpha) * (a + da) + alpha * (b + db)) -
      ((1 - alpha) * a + alpha * b) = (1 - alpha) * da + alpha * db by ring]
  have h₁ := abs_add_le ((1 - alpha) * da) (alpha * db)
  rw [abs_mul, abs_mul, abs_of_nonneg (sub_nonneg.mpr hb), abs_of_nonneg ha] at h₁
  have h₂ := mul_le_mul_of_nonneg_left hda (sub_nonneg.mpr hb)
  have h₃ := mul_le_mul_of_nonneg_left hdb ha
  nlinarith

-- Even a band-limited forward map can have zero forward speed. There is no
-- unconditional time-error constant hidden in the phase-error theorem.
theorem no_positive_speed_at_turn (F : ℝ → ℝ) (t : ℝ)
    (ht : HasDerivAt F 0 t) (speed : ℝ) (hs : 0 < speed) :
    ¬ HasDerivAt F speed t := by
  intro hd
  have he := ht.unique hd
  linarith

end PhaseTime.InverseGrid

namespace PhaseTime.InverseGrid

-- An explicit guard is preferable to inventing a cross-branch answer after
-- exceeding the declared global/per-cell representation capacity.
-- Generic reference lemma: exhausted representations do not invent interpolation results.
noncomputable def supportedRead (supported : Bool) (s : Piece) (q : ℝ) : Option ℝ :=
  if supported then some (s.read q) else none

theorem overflow_unavailable (s : Piece) (q : ℝ) : supportedRead false s q = none := by
  simp [supportedRead]

-- Generic reference lemma: shared storage uses 32 bytes per cell in aggregate.
def payloadBytes (cells boundaries : Nat) : Nat := 8 * cells + 4 * cells + 20 * boundaries

theorem compact_payload_budget : payloadBytes (2^20) (2^20) = 32 * 2^20 := by
  norm_num [payloadBytes]

-- A new jump is inserted only after clipping obsolete boundaries and checking
-- room for the new record. Pool allocation and pointer management are native
-- implementation details, with explicit failure rather than an assumed supply.
theorem insertion_fits (jumps : List Jump) (j : Jump) (capacity : Nat)
    (h : jumps.length < capacity) : Fits capacity (jumps ++ [j]) := by
  simp only [Fits, List.length_append, List.length_singleton]
  omega

end PhaseTime.InverseGrid

namespace PhaseTime.InverseGrid

theorem bandlimited_zero_speed :
    BandLimited [Harmonic.mk (-1) 1 0] 1 ∧
    HasDerivAt (fun u : ℝ => u + wave [Harmonic.mk (-1) 1 0] u) 0 0 := by
  constructor
  · simp [BandLimited]
  · have hd : HasDerivAt (fun u : ℝ => u - Real.sin u) (1 - Real.cos 0) 0 :=
      (hasDerivAt_id (0 : ℝ)).sub (Real.hasDerivAt_sin 0)
    simpa [wave, Harmonic.value, sub_eq_add_neg] using hd

end PhaseTime.InverseGrid

namespace PhaseTime.InverseGrid

/-! Cell construction: retain complete old pieces, clip the intersected piece,
and append the fresh forward piece. This is the piece-list view of the runtime's
grid plus one-sided knots; allocation and linked indices are not modeled. -/
noncomputable def retainBefore (pieces : List Piece) (cut : ℝ) : List Piece :=
  match pieces with
  | [] => []
  | s :: rest =>
    if cut ≤ s.left.phase then retainBefore rest cut
    else if s.right.phase ≤ cut then s :: retainBefore rest cut
    else s.clip cut :: retainBefore rest cut

def EnclosedHistory (pieces : List Piece) (F : ℝ → ℝ) (lo hi : ℝ) : Prop :=
  ∀ s ∈ pieces, s.Enclosed F lo hi

theorem retained_history_enclosed (pieces : List Piece) (F : ℝ → ℝ) (lo hi cut : ℝ)
    (h : EnclosedHistory pieces F lo hi) :
    EnclosedHistory (retainBefore pieces cut) F lo hi := by
  induction pieces with
  | nil => simp [EnclosedHistory, retainBefore]
  | cons s rest ih =>
    have hs := h s (by simp)
    have hr : EnclosedHistory rest F lo hi := fun p hp => h p (by simp [hp])
    have ht := ih hr
    simp only [retainBefore]
    split_ifs with hc he
    · exact ht
    · intro p hp
      rcases List.mem_cons.mp hp with hp | hp
      · simpa [hp] using hs
      · exact ht p hp
    · intro p hp
      rcases List.mem_cons.mp hp with hp | hp
      · subst p
        exact clipping_preserves_range s F lo hi cut hs
          ⟨lt_of_not_ge hc, le_of_lt (lt_of_not_ge he)⟩
      · exact ht p hp

noncomputable def replaceSuffix (old : List Piece) (fresh : Piece) : List Piece :=
  retainBefore old fresh.left.phase ++ [fresh]

theorem forward_preserves_enclosure (old : List Piece) (fresh : Piece)
    (F : ℝ → ℝ) (lo hi : ℝ) (ho : EnclosedHistory old F lo hi)
    (hf : fresh.Enclosed F lo hi) :
    EnclosedHistory (replaceSuffix old fresh) F lo hi := by
  intro s hs
  rcases List.mem_append.mp hs with hs | hs
  · exact retained_history_enclosed old F lo hi fresh.left.phase ho s hs
  · simpa using (List.mem_singleton.mp hs ▸ hf)

inductive BuiltHistory (F : ℝ → ℝ) (lo hi : ℝ) : List Piece → Prop
  | empty : BuiltHistory F lo hi []
  | forward {old : List Piece} {fresh : Piece} : BuiltHistory F lo hi old →
      fresh.Enclosed F lo hi → BuiltHistory F lo hi (replaceSuffix old fresh)

theorem built_history_enclosed (pieces : List Piece) (F : ℝ → ℝ) (lo hi : ℝ)
    (h : BuiltHistory F lo hi pieces) : EnclosedHistory pieces F lo hi := by
  induction h with
  | empty => simp [EnclosedHistory]
  | forward _ hf ih => exact forward_preserves_enclosure _ _ F lo hi ih hf

theorem built_history_phase_error (pieces : List Piece) (s : Piece) (F : ℝ → ℝ)
    (lo hi q width : ℝ) (h : BuiltHistory F lo hi pieces) (hs : s ∈ pieces)
    (hq : s.left.phase ≤ q ∧ q ≤ s.right.phase)
    (hc : lo ≤ q ∧ q ≤ hi) (hw : hi - lo ≤ width) :
    |F (s.read q) - q| ≤ width := by
  exact enclosed_phase_error s F lo hi q width
    (built_history_enclosed pieces F lo hi h s hs) hq hc hw

/-! Bridge from the engine-sampled piecewise-linear warp to a declared
continuous band-limited warp. A Lipschitz chord differs by at most K*step/2.
This bounds phase error even when a continuous turn is hidden between samples;
it does not establish that an unresolved continuous branch was selected. -/
theorem lipschitz_chord_error (W : ℝ → ℝ) (a b alpha gain : ℝ)
    (hab : a ≤ b) (ha : 0 ≤ alpha) (hb : alpha ≤ 1) (hg : 0 ≤ gain)
    (hW : ∀ x y, |W x - W y| ≤ gain * |x - y|) :
    |W ((1 - alpha) * a + alpha * b) -
      ((1 - alpha) * W a + alpha * W b)| ≤ gain * (b - a) / 2 := by
  let x := (1 - alpha) * a + alpha * b
  have hxa : x - a = alpha * (b - a) := by dsimp [x]; ring
  have hbx : b - x = (1 - alpha) * (b - a) := by dsimp [x]; ring
  have h₁ := hW x a
  have h₂ := hW x b
  rw [hxa, abs_of_nonneg (mul_nonneg ha (sub_nonneg.mpr hab))] at h₁
  rw [abs_sub_comm x b, hbx,
    abs_of_nonneg (mul_nonneg (sub_nonneg.mpr hb) (sub_nonneg.mpr hab))] at h₂
  have ht := abs_add_le ((1 - alpha) * (W x - W a)) (alpha * (W x - W b))
  rw [abs_mul, abs_mul, abs_of_nonneg (sub_nonneg.mpr hb), abs_of_nonneg ha] at ht
  have hm₁ := mul_le_mul_of_nonneg_left h₁ (sub_nonneg.mpr hb)
  have hm₂ := mul_le_mul_of_nonneg_left h₂ ha
  have hd : 0 ≤ gain * (b - a) := mul_nonneg hg (sub_nonneg.mpr hab)
  have hs := mul_nonneg hd (sq_nonneg (alpha - 1 / 2))
  change |W x - ((1 - alpha) * W a + alpha * W b)| ≤ _
  rw [show W x - ((1 - alpha) * W a + alpha * W b) =
    (1 - alpha) * (W x - W a) + alpha * (W x - W b) by ring]
  nlinarith

theorem continuous_phase_error (sampled continuous : ℝ → ℝ)
    (approx q width observationError : ℝ)
    (hg : |sampled approx - q| ≤ width)
    (ho : |continuous approx - sampled approx| ≤ observationError) :
    |continuous approx - q| ≤ width + observationError := by
  have ht := abs_add_le (continuous approx - sampled approx) (sampled approx - q)
  rw [sub_add_sub_cancel] at ht
  linarith

theorem bandlimited_chord_error (hs : List Harmonic) (band a b alpha : ℝ)
    (hB : BandLimited hs band) (hab : a ≤ b) (ha : 0 ≤ alpha) (hb : alpha ≤ 1) :
    |((1 - alpha) * a + alpha * b + wave hs ((1 - alpha) * a + alpha * b)) -
      ((1 - alpha) * (a + wave hs a) + alpha * (b + wave hs b))| ≤
      (1 + band * amplitudeBudget hs) * (b - a) / 2 := by
  have hA : 0 ≤ amplitudeBudget hs :=
    List.sum_nonneg (fun x hx => by
      obtain ⟨h, _, rfl⟩ := List.mem_map.mp hx
      exact abs_nonneg _)
  exact lipschitz_chord_error (fun u => u + wave hs u) a b alpha
    (1 + band * amplitudeBudget hs) hab ha hb
    (by nlinarith [mul_nonneg hB.1 hA])
    (fun u v => bandlimited_forward_slope hs band u v hB)

end PhaseTime.InverseGrid
