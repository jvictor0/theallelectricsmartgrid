import DSP.Resynthesis.Grain
import DSP.Resynthesis.PVDRRoots
import DSP.IfLess.Real
import Mathlib.Algebra.Order.Archimedean.Real.Basic
import Mathlib.Tactic.Linarith

namespace DSP.Resynthesis

theorem principalArgument_bounds (phase : ℝ) :
    -(0.5 : ℝ) < principalArgument phase (↑⌊phase⌋) ∧
      principalArgument phase (↑⌊phase⌋) ≤ 0.5 := by
  have lower := Int.floor_le phase
  have upper := Int.lt_floor_add_one phase
  simp only [principalArgument, IfLess.ifLess]
  split <;> constructor <;> linarith

theorem instantaneousFrequency_bounds (center hop phase previous : ℝ) (hop_pos : 0 < hop) :
    -(0.5 : ℝ) / hop <
        instantaneousFrequency (fun x : ℝ => ↑⌊x⌋) center hop phase previous - center ∧
      instantaneousFrequency (fun x : ℝ => ↑⌊x⌋) center hop phase previous - center ≤
        0.5 / hop := by
  obtain ⟨lower, upper⟩ := principalArgument_bounds (phaseResidual center hop phase previous)
  simpa only [instantaneousFrequency, correctedFrequency, add_sub_cancel_right] using
    And.intro ((div_lt_div_iff_of_pos_right hop_pos).2 lower)
      ((div_le_div_iff_of_pos_right hop_pos).2 upper)

private theorem foldl_get_of_not_mem {P : Type} {n : ℕ}
    (step : BinTable P n → ActiveBin n → BinTable P n)
    (keeps : ∀ table other bin, other ≠ bin → (step table other).get bin = table.get bin)
    (order : List (ActiveBin n)) (phases : BinTable P n) (bin : ActiveBin n)
    (absent : bin ∉ order) :
    (order.foldl step phases).get bin = phases.get bin := by
  induction order generalizing phases with
  | nil => rfl
  | cons other order ih =>
      have hne : other ≠ bin := fun h => absent (by simp [h])
      have htail : bin ∉ order := fun h => absent (List.mem_cons_of_mem other h)
      rw [List.foldl_cons, ih _ htail, keeps _ _ _ hne]

section Fixup

variable {n : ℕ}

private theorem bin_get_set {P : Type} (phases : BinTable P n) (bin other : ActiveBin n)
    (phase : P) :
    (phases.set bin phase).get other = if bin = other then phase else phases.get other := by
  simp [BinTable.get, BinTable.set, Vector.getElem_set, Fin.ext_iff]

/-- A root's previous phase is read before its only update in the order. -/
theorem fixupPhases_root (math : GrainMath ℝ ℝ) (detune : ℝ) (input : PVDRInput ℝ ℝ n)
    (gates : BinTable GateState n) (graph : PVDRGraph ℝ n) (phases : BinTable ℝ n)
    (bin : ActiveBin n) (before after : List (ActiveBin n))
    (order : graph.order = before ++ bin :: after) (absent_before : bin ∉ before)
    (absent_after : bin ∉ after) (root : (graph.link.get bin).root = bin) :
    (fixupPhases math detune input gates graph phases).get bin =
      if gates.get bin == .triggered then math.widen (input.phase.get bin)
      else phases.get bin + (graph.link.get bin).delta * math.widen detune := by
  unfold fixupPhases
  rw [order, List.foldl_append, List.foldl_cons]
  rw [foldl_get_of_not_mem _ (by
    intro table other key hne
    simp [bin_get_set, hne]) _ _ _ absent_after]
  rw [bin_get_set, if_pos rfl, if_pos root]
  rw [foldl_get_of_not_mem _ (by
    intro table other key hne
    simp [bin_get_set, hne]) _ _ _ absent_before]
  rfl

/-- A follower reads the root's final phase when no later update writes either bin. -/
theorem fixupPhases_follower (math : GrainMath ℝ ℝ) (detune : ℝ)
    (input : PVDRInput ℝ ℝ n) (gates : BinTable GateState n) (graph : PVDRGraph ℝ n)
    (phases : BinTable ℝ n) (bin : ActiveBin n) (before after : List (ActiveBin n))
    (order : graph.order = before ++ bin :: after) (absent_after : bin ∉ after)
    (root_distinct : (graph.link.get bin).root ≠ bin)
    (root_absent_after : (graph.link.get bin).root ∉ after) :
    (fixupPhases math detune input gates graph phases).get bin =
      (fixupPhases math detune input gates graph phases).get (graph.link.get bin).root +
        (graph.link.get bin).delta := by
  unfold fixupPhases
  rw [order, List.foldl_append, List.foldl_cons]
  rw [foldl_get_of_not_mem _ (by
    intro table other key hne
    simp [bin_get_set, hne]) _ _ _ absent_after]
  rw [foldl_get_of_not_mem _ (by
    intro table other key hne
    simp [bin_get_set, hne]) _ _ _ root_absent_after]
  simp [bin_get_set, root_distinct, Ne.symm root_distinct, followerPhase]

end Fixup

/-- Fixup over the analyzed graph gives each follower its root's final phase
plus the difference of their input phases. -/
theorem analyze_fixup_follower {n : ℕ} {Q RNG : Type} (math : GrainMath ℝ ℝ)
    (hop detune : ℝ) (queue : PVDR.Queue ℝ Q n) (law : PVDR.QueueMembership queue)
    (nextPhase : StateM RNG ℝ) (gates : BinTable GateState n) (input : PVDRInput ℝ ℝ n)
    (phases : BinTable ℝ n) (rng : RNG) :
    let result := ((PVDR.analyze id hop math.thresholds queue nextPhase gates input).run rng).1
    let graph := result.2
    let fixed := fixupPhases math detune input result.1 graph phases
    ∀ bin ∈ graph.order,
      let root := (graph.link.get bin).root
      root ≠ bin → fixed.get bin = fixed.get root + (input.phase.get bin - input.phase.get root) := by
  dsimp only
  intro bin mem distinct
  let result := ((PVDR.analyze id hop math.thresholds queue nextPhase gates input).run rng).1
  let graph := result.2
  let root := (graph.link.get bin).root
  obtain ⟨before, between, after, order⟩ :
      ∃ before between after, graph.order = before ++ root :: between ++ bin :: after :=
    PVDR.analyze_root_precedes_bin hop math.thresholds queue law nextPhase gates input rng bin mem distinct
  have unique : graph.order.Nodup :=
    PVDR.analyze_order_nodup id hop math.thresholds queue nextPhase gates input rng
  rw [order] at unique
  have normalized : (before ++ (root :: (between ++ (bin :: after)))).Nodup := by
    simpa only [List.append_assoc, List.cons_append] using unique
  have rootTail : (root :: (between ++ (bin :: after))).Nodup :=
    (List.nodup_append.mp normalized).2.1
  have rootUnique := List.nodup_cons.mp rootTail
  have rootAbsent : root ∉ after := fun present =>
    rootUnique.1 (List.mem_append_right _ (List.mem_cons_of_mem _ present))
  have binTail : (bin :: after).Nodup := (List.nodup_append.mp rootUnique.2).2.1
  have binAbsent : bin ∉ after := (List.nodup_cons.mp binTail).1
  have followerOrder : graph.order = (before ++ root :: between) ++ bin :: after := order
  have delta := (PVDR.analyze_roots hop math.thresholds queue law nextPhase gates input rng bin mem).2.2 distinct
  have fixed := fixupPhases_follower math detune input result.1 graph phases bin _ after
    followerOrder binAbsent distinct rootAbsent
  change (graph.link.get bin).delta = input.phase.get bin - input.phase.get root at delta
  rw [delta] at fixed
  exact fixed

end DSP.Resynthesis
