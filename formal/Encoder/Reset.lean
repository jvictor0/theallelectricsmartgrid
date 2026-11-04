import Encoder.Edits
import Encoder.Control

namespace Encoder.Reset

-- @cpp reset_scope: SceneManager::IsSceneActive and StateEncoderCell::SetValue.
-- @head reset_scope: identical selected-scene predicate and ascending scene loop.
def selected (allScenes : Bool) (left right : Scene) (blend : Rat) (i : Scene) : Bool :=
  allScenes || decide ((i = left ∧ blend < 1) ∨ (i = right ∧ 0 < blend))

theorem all_scenes_selected (l r i : Scene) (b : Rat) : selected true l r b i = true := by
  simp [selected]

theorem endpoint_scope (l r i : Scene) :
    selected false l r 0 i = decide (i = l) ∧
    selected false l r 1 i = decide (i = r) := by
  simp [selected]

-- @cpp ramp: TickDistributor::RecordChange/RecordSample and MoveValuesTowardsTargets.
-- @head ramp: the same counter/target rules establish the stronger invariant.
-- A reachable settled cell agrees with its target. Age zero means a live ramp,
-- so another edit in that same sample cannot inherit a zero-length ramp.
def Ready (timing : Timing) (s : Cell) : Prop :=
  Valid timing s ∧ (s.age = 0 → 0 < s.remaining) ∧ (s.remaining = 0 → s.values = s.targets)

theorem initial_ready (t : Timing) (v : Rat) (hv : UnitInterval v) :
    Ready t (initialFrom t v) := by
  have hp : 0 < t.maximum := Nat.lt_of_lt_of_le t.positive t.ordered
  exact ⟨initialFrom_valid t v hv, by simp [initialFrom]; omega, by simp [initialFrom]⟩

theorem setTarget_ready (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (hs : Ready t s) (hv : UnitInterval v) : Ready t (setTarget t s i v) := by
  have ha := hs.2.1
  refine ⟨setTarget_valid t s i v hs.1 hv, ?_, ?_⟩
  · unfold setTarget
    split
    · exact hs.2.1
    · have hd : 0 < duration t s.age := by
        unfold duration
        have hp := t.positive
        have hm := t.ordered
        omega
      simp only
      split <;> grind
  · unfold setTarget
    split
    · exact hs.2.2
    · have hd : 0 < duration t s.age := by
        unfold duration
        have hp := t.positive
        have hm := t.ordered
        omega
      intro hz
      have hp : 0 < (if s.age = 0 then s.remaining else duration t s.age) := by
        split <;> grind
      simp only at hz
      exact False.elim (by omega)

theorem edit_ready (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (hs : Ready t s) (hv : UnitInterval v) : Ready t (edit t s i v) :=
  setTarget_ready t s i v hs hv

theorem turn_ready (t : Timing) (s : Cell) (i : Scene) (b z : Bool) (d : Rat)
    (hs : Ready t s) : Ready t (Edits.turn t s i b z d) := by
  unfold Edits.turn
  split
  · exact hs
  · exact edit_ready t s i _ hs (Edits.combine_bounded _ _ _ _)

theorem compute_ready (t : Timing) (s : Cell) (advance : Bool) (hs : Ready t s) :
    Ready t (compute t advance s) := by
  refine ⟨compute_valid t s advance hs.1, ?_, ?_⟩
  · unfold compute
    split
    · have hp := t.positive
      have hm := t.ordered
      simp only
      omega
    · exact hs.2.1
  · unfold compute
    split
    · intro h
      funext i
      unfold move
      split
      · exact congrFun (hs.2.2 (by assumption)) i
      · split
        · rfl
        · simp only at h
          omega
    · exact hs.2.2

-- A uniform bound also covers already-settled cells and resets during a ramp.
theorem run_lands (t : Timing) (n : Nat) (s : Cell) (hs : Ready t s)
    (hn : s.remaining ≤ n) : (run t n s).values = s.targets := by
  induction n generalizing s with
  | zero => exact hs.2.2 (by omega)
  | succ n ih =>
    have hc := compute_ready t s true hs
    have hr : (compute t true s).remaining ≤ n := by
      unfold compute
      split
      · dsimp; omega
      · have hf := hs.1.1
        simp_all
    simpa only [run, compute_preserves_targets] using ih (compute t true s) hc hr

theorem run_max_lands (t : Timing) (s : Cell) (hs : Ready t s) :
    (run t t.maximum s).values = s.targets :=
  run_lands t t.maximum s hs (Nat.le_trans hs.1.1 hs.1.2.1)

-- @cpp reset_write: SetValue invokes SetAndRecordValue once per selected scene.
-- @head reset_write: the same recorded writes, including unchanged targets.
def writeScenes (t : Timing) (scope : Scene → Bool) (value : Rat) : List Scene → Cell → Cell
  | [], s => s
  | i::rest, s => writeScenes t scope value rest (if scope i then edit t s i value else s)

theorem edit_target (t : Timing) (s : Cell) (i j : Scene) (v : Rat) :
    (edit t s i v).targets j = if j = i then v else s.targets j := by
  unfold edit setTarget
  split <;> simp_all

theorem write_targets (t : Timing) (scope : Scene → Bool) (v : Rat) (is : List Scene)
    (s : Cell) (i : Scene) :
    (writeScenes t scope v is s).targets i = if i ∈ is ∧ scope i then v else s.targets i := by
  induction is generalizing s with
  | nil => simp [writeScenes]
  | cons j js ih =>
    by_cases hj : scope j = true
    · simp only [writeScenes, hj, ↓reduceIte, ih, edit_target, List.mem_cons]
      split_ifs <;> grind only
    · simp only [writeScenes, hj, ih, List.mem_cons]
      split_ifs <;> grind only

theorem write_no_jump (t : Timing) (scope : Scene → Bool) (v : Rat) (is : List Scene) (s : Cell) :
    (writeScenes t scope v is s).values = s.values := by
  induction is generalizing s with
  | nil => rfl
  | cons i rest ih =>
    simp only [writeScenes, ih]
    split
    · exact target_changes_do_not_jump t s i v
    · rfl

theorem write_valid (t : Timing) (scope : Scene → Bool) (v : Rat) (is : List Scene)
    (s : Cell) (hs : Valid t s) (hv : UnitInterval v) : Valid t (writeScenes t scope v is s) := by
  induction is generalizing s with
  | nil => exact hs
  | cons i rest ih =>
    unfold writeScenes
    apply ih
    split
    · exact edit_valid t s i v hs hv
    · exact hs

theorem write_ready (t : Timing) (scope : Scene → Bool) (v : Rat) (is : List Scene)
    (s : Cell) (hs : Ready t s) (hv : UnitInterval v) : Ready t (writeScenes t scope v is s) := by
  induction is generalizing s with
  | nil => exact hs
  | cons i rest ih =>
    unfold writeScenes
    apply ih
    split
    · exact edit_ready t s i v hs hv
    · exact hs

theorem write_record_count (t : Timing) (scope : Scene → Bool) (v : Rat) (is : List Scene) (s : Cell) :
    (writeScenes t scope v is s).recorded = s.recorded + (is.filter scope).length := by
  induction is generalizing s with
  | nil => simp [writeScenes]
  | cons i rest ih =>
    simp only [writeScenes, ih]
    split <;> simp_all [edit]
    omega

-- @cpp reset: RevertToDefault/ZeroModulators force each visited modulator cell.
-- @head reset: identical leaf semantics; working-tree invalidation also changes its revision.
def resetCell (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell) : Cell :=
  { writeScenes t scope v (List.finRange 8) s with force := t.maximum }

theorem reset_targets (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell) (i : Scene) :
    (resetCell t scope v s).targets i = if scope i then v else s.targets i := by
  simp [resetCell, write_targets]

theorem reset_no_jump (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell) :
    (resetCell t scope v s).values = s.values := write_no_jump _ _ _ _ _

theorem reset_forces (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell) :
    (resetCell t scope v s).force = t.maximum := rfl

theorem reset_valid (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell)
    (hs : Valid t s) (hv : UnitInterval v) : Valid t (resetCell t scope v s) := by
  have h := write_valid t scope v (List.finRange 8) s hs hv
  exact ⟨Nat.le_trans h.1 h.2.1, Nat.le_refl _, h.2.2⟩

theorem reset_ready (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell)
    (hs : Ready t s) (hv : UnitInterval v) : Ready t (resetCell t scope v s) := by
  have h := write_ready t scope v (List.finRange 8) s hs hv
  refine ⟨⟨?_, Nat.le_refl _, h.1.2.2⟩, h.2⟩
  exact Nat.le_trans h.1.1 h.1.2.1

theorem reset_finishes (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell)
    (hs : Ready t s) (hv : UnitInterval v) (i : Scene) :
    (run t t.maximum (resetCell t scope v s)).values i = if scope i then v else s.targets i := by
  rw [run_max_lands t _ (reset_ready t scope v s hs hv), reset_targets]

theorem reset_records (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell) :
    (resetCell t scope v s).recorded = s.recorded + ((List.finRange 8).filter scope).length :=
  write_record_count _ _ _ _ _

theorem reset_is_dirty (t : Timing) (scope : Scene → Bool) (v : Rat) (s : Cell)
    (sources : List Source) (gestures : List Gesture) (inputs : Inputs) :
    dirty False (resetCell t scope v s).force sources gestures inputs inputs := by
  have hp : t.maximum ≠ 0 := by have := t.positive; have := t.ordered; omega
  exact Or.inr (Or.inl hp)

theorem reset_rejects_prepared (t : Timing) (p : Preparation) :
    fallback {p with force := t.maximum} = true := by
  apply active_ramp_falls_back
  have := t.positive
  have := t.ordered
  dsimp
  omega

end Encoder.Reset
