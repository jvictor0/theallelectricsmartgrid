import Encoder.ResetEvaluation

namespace Encoder.Gestures

abbrev State := Reset.GestureState

-- @cpp gesture_order: scene 2 is visited before scene 1, with no deduplication.
-- @head gesture_order: the same endpoint predicates and ordering.
def visits (left right : Scene) (position : Rat) : List Scene :=
  (if 0 < position then [right] else []) ++ (if position < 1 then [left] else [])

theorem visited_iff (l r i : Scene) (p : Rat) :
    i ∈ visits l r p ↔ Reset.selected false l r p i = true := by
  simp only [visits, Reset.selected]
  split_ifs <;> simp_all
  grind

theorem endpoints (l r : Scene) : visits l r 0 = [l] ∧ visits l r 1 = [r] := by
  norm_num [visits]

theorem aliased_interior (s : Scene) (p : Rat) (hp : 0 < p ∧ p < 1) :
    visits s s p = [s,s] := by simp [visits, hp.1, hp.2]

-- @cpp gesture_allocate: AddGesture is an ensure operation; new cells inherit polarity.
-- @head gesture_allocate: same constructor and lazy allocation.
def fresh (t : Timing) (bipolar : Bool) : State :=
  ⟨initialFrom t (Edits.neutral bipolar), fun _ => false, 0, 0⟩

def ensure (t : Timing) (b : Bool) (slot : Option State) : Option State :=
  some (slot.getD (fresh t b))

theorem ensure_existing (t : Timing) (b : Bool) (g : State) : ensure t b (some g) = some g := rfl

theorem ensure_missing (t : Timing) (b : Bool) : ensure t b none = some (fresh t b) := rfl

theorem ensure_idempotent (t : Timing) (b : Bool) (slot : Option State) :
    ensure t b (ensure t b slot) = ensure t b slot := by cases slot <;> rfl

theorem fresh_valid (t : Timing) (b : Bool) : Valid t (fresh t b).cell := by
  apply initialFrom_valid
  cases b <;> norm_num [Edits.neutral, UnitInterval]

-- @cpp gesture_flag: SetActive(active, scene) records even redundant flag writes.
-- @head gesture_flag: same per-scene activation recording request.
def flag (value : Bool) (i : Scene) (g : State) : State :=
  {g with active := fun j => if j = i then value else g.active j
          recordedActivations := g.recordedActivations + 1}

-- @cpp gesture_capture: SetActive(active) copies current, then records the target,
-- only for an inactive scene being enabled. It does not copy the parent's timer.
-- @head gesture_capture: identical capture order and unchanged-target fast path.
def capture (t : Timing) (parent : Cell) (i : Scene) (g : State) : State :=
  {g with cell := edit t {g.cell with values := fun j => if j = i then parent.values j else g.cell.values j} i (parent.targets i)}

def setScene (t : Timing) (parent : Cell) (value : Bool) (i : Scene) (g : State) : State :=
  flag value i (if value && !g.active i then capture t parent i g else g)

def setScenes (t : Timing) (parent : Cell) (value : Bool) (scenes : List Scene) (g : State) : State :=
  scenes.foldl (fun g i => setScene t parent value i g) g

theorem flag_cell (v : Bool) (i : Scene) (g : State) : (flag v i g).cell = g.cell := rfl

theorem capture_current_target (t : Timing) (parent : Cell) (i : Scene) (g : State) :
    (capture t parent i g).cell.values i = parent.values i ∧
    (capture t parent i g).cell.targets i = parent.targets i ∧
    (capture t parent i g).cell.recorded = g.cell.recorded + 1 := by
  by_cases h : g.cell.targets i = parent.targets i <;> simp [capture, edit, setTarget, h]

theorem setScene_active (t : Timing) (parent : Cell) (v : Bool) (i : Scene) (g : State) :
    (setScene t parent v i g).active i = v := by simp [setScene, flag]

theorem active_not_recaptured (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (h : g.active i = true) : (setScene t parent true i g).cell = g.cell := by
  simp [setScene, h, flag]

theorem deactivate_preserves_cell (t : Timing) (parent : Cell) (i : Scene) (g : State) :
    (setScene t parent false i g).cell = g.cell := rfl

theorem setScene_other (t : Timing) (parent : Cell) (v : Bool) (i j : Scene) (g : State)
    (h : j ≠ i) :
    (setScene t parent v i g).active j = g.active j ∧
    (setScene t parent v i g).cell.values j = g.cell.values j ∧
    (setScene t parent v i g).cell.targets j = g.cell.targets j := by
  simp only [setScene]
  split
  · by_cases ht : g.cell.targets i = parent.targets i <;> simp [flag, capture, edit, setTarget, h, ht]
  · simp [flag, h]

theorem setScene_valid (t : Timing) (parent : Cell) (v : Bool) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Valid t g.cell) : Valid t (setScene t parent v i g).cell := by
  unfold setScene
  split
  · apply edit_valid
    · rcases hg with ⟨hr,hf,ha,hvs,hts⟩
      refine ⟨hr,hf,ha,?_,hts⟩
      intro j
      dsimp
      split
      · exact hp.2.2.2.1 j
      · exact hvs j
    · exact hp.2.2.2.2 i
  · exact hg

theorem setScenes_valid (t : Timing) (parent : Cell) (v : Bool) (scenes : List Scene) (g : State)
    (hp : Valid t parent) (hg : Valid t g.cell) : Valid t (setScenes t parent v scenes g).cell := by
  induction scenes generalizing g with
  | nil => exact hg
  | cons i rest ih => exact ih _ (setScene_valid t parent v i g hp hg)

theorem setScenes_records (t : Timing) (parent : Cell) (v : Bool) (scenes : List Scene) (g : State) :
    (setScenes t parent v scenes g).recordedActivations = g.recordedActivations + scenes.length := by
  induction scenes generalizing g with
  | nil => simp [setScenes]
  | cons i rest ih =>
    simp only [setScenes, List.foldl_cons] at *
    rw [ih]
    simp only [setScene, flag]
    split <;> simp [capture, Nat.add_assoc, Nat.add_comm]

theorem setScenes_unvisited (t : Timing) (parent : Cell) (v : Bool) (scenes : List Scene)
    (g : State) (j : Scene) (h : j ∉ scenes) :
    (setScenes t parent v scenes g).active j = g.active j ∧
    (setScenes t parent v scenes g).cell.values j = g.cell.values j ∧
    (setScenes t parent v scenes g).cell.targets j = g.cell.targets j := by
  induction scenes generalizing g with
  | nil => exact ⟨rfl,rfl,rfl⟩
  | cons i rest ih =>
    have hj : j ≠ i := by simp_all
    have hr : j ∉ rest := by simp_all
    have a := ih (setScene t parent v i g) hr
    have b := setScene_other t parent v i j g hj
    exact ⟨a.1.trans b.1, a.2.1.trans b.2.1, a.2.2.trans b.2.2⟩

-- The final flag is assigned, not toggled, so aliased scene visits are harmless
-- for activation state but still produce two activation recording requests.
theorem setScenes_flag (t : Timing) (parent : Cell) (v : Bool) (scenes : List Scene) (g : State)
    (j : Scene) : (setScenes t parent v scenes g).active j =
      if j ∈ scenes then v else g.active j := by
  induction scenes generalizing g with
  | nil => simp [setScenes]
  | cons i rest ih =>
    simp only [setScenes, List.foldl_cons] at *
    rw [ih]
    by_cases hj : j = i
    · subst j
      simp [setScene_active]
    · have ho := (setScene_other t parent v i j g hj).1
      simp [List.mem_cons, hj, ho]

-- @cpp gesture_toggle: ToggleActive writes flags directly: no capture or recording.
-- @head gesture_toggle: sequential flips, including duplicate scene indices.
def toggleScene (i : Scene) (g : State) : State :=
  {g with active := fun j => if j = i then !g.active j else g.active j}

def toggle (scenes : List Scene) (g : State) : State := scenes.foldl (fun g i => toggleScene i g) g

theorem toggle_preserves_data (scenes : List Scene) (g : State) :
    (toggle scenes g).cell = g.cell ∧ (toggle scenes g).recordedActivations = g.recordedActivations := by
  induction scenes generalizing g with
  | nil => exact ⟨rfl,rfl⟩
  | cons i rest ih => exact ih (toggleScene i g)

theorem toggle_same_scene_twice (s : Scene) (g : State) : toggle [s,s] g = g := by
  cases g
  simp only [toggle, toggleScene, List.foldl_cons, List.foldl_nil]
  congr
  funext i
  split <;> simp_all

-- @cpp gesture_clear: ClearGesture and DeactivateGestureCurrentScene only clear flags.
-- @head gesture_clear: stored values/targets and the allocation survive deletion.
def deactivate (scenes : List Scene) (g : State) : State :=
  scenes.foldl (fun g i => flag false i g) g

theorem deactivate_data (scenes : List Scene) (g : State) :
    (deactivate scenes g).cell = g.cell ∧ (deactivate scenes g).weight = g.weight := by
  induction scenes generalizing g with
  | nil => exact ⟨rfl,rfl⟩
  | cons i rest ih => exact ih (flag false i g)

theorem deactivate_flags (scenes : List Scene) (g : State) (i : Scene) :
    (deactivate scenes g).active i = if i ∈ scenes then false else g.active i := by
  induction scenes generalizing g with
  | nil => simp [deactivate]
  | cons j rest ih =>
    simp only [deactivate, List.foldl_cons] at *
    rw [ih]
    simp only [flag, List.mem_cons]
    split_ifs <;> simp_all

theorem deactivate_records (scenes : List Scene) (g : State) :
    (deactivate scenes g).recordedActivations = g.recordedActivations + scenes.length := by
  induction scenes generalizing g with
  | nil => simp [deactivate]
  | cons i rest ih => simpa [deactivate, flag, Nat.add_assoc, Nat.add_comm] using ih (flag false i g)

theorem deactivated_weight_zero (scope : Reset.Scope) (g : State) :
    Reset.effectiveWeight scope (deactivate (visits scope.left scope.right scope.blend) g) = 0 := by
  unfold Reset.effectiveWeight
  rw [(deactivate_data _ g).2]
  change Reset.banked {scope with allScenes := false}
    (fun i => if (deactivate (visits scope.left scope.right scope.blend) g).active i then g.weight else 0) = 0
  apply Reset.banked_selected_constant
  intro i hi
  have hm := (visited_iff scope.left scope.right i scope.blend).mpr hi
  simp [deactivate_flags, hm]

-- Capture continuity is immediate for a newly activated scene, even mid-ramp.
theorem activating_inherits (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (h : g.active i = false) :
    (setScene t parent true i g).cell.values i = parent.values i ∧
    (setScene t parent true i g).cell.targets i = parent.targets i := by
  simpa [setScene, h, flag] using (capture_current_target t parent i g).imp_right And.left

theorem activation_values (t : Timing) (parent : Cell) (i j : Scene) (g : State) :
    (setScene t parent true i g).cell.values j =
      if j = i ∧ g.active j = false then parent.values j else g.cell.values j := by
  by_cases he : j = i
  · subst j
    cases ha : g.active i
    · simpa [ha] using (activating_inherits t parent i g ha).1
    · simp [active_not_recaptured t parent i g ha]
  · simpa [he] using (setScene_other t parent true i j g he).2.1

theorem activation_scene_values (t : Timing) (parent : Cell) (scenes : List Scene) (g : State)
    (j : Scene) :
    (setScenes t parent true scenes g).cell.values j =
      if j ∈ scenes ∧ g.active j = false then parent.values j else g.cell.values j := by
  induction scenes generalizing g with
  | nil => simp [setScenes]
  | cons i rest ih =>
    simp only [setScenes, List.foldl_cons] at *
    rw [ih, activation_values]
    by_cases he : j = i
    · subst j
      simp [setScene_active]
    · have ha := (setScene_other t parent true i j g he).1
      simp [ha, List.mem_cons, he]

-- A single newly enabled gesture initially evaluates to the parent's unmodulated
-- banked base. Existing active scenes must already match for this claim; arbitrary
-- pre-existing gestures need not be continuous when another weight is introduced.
theorem capture_blend_continuity (t : Timing) (scope : Reset.Scope) (parent : Cell) (g : State)
    (h : ∀ i ∈ visits scope.left scope.right scope.blend, g.active i = false) :
    Reset.banked scope (setScenes t parent true (visits scope.left scope.right scope.blend) g).cell.values =
      Reset.banked scope parent.values := by
  unfold Reset.banked Encoder.blend
  rw [activation_scene_values, activation_scene_values]
  have hl := h scope.left
  have hr := h scope.right
  rcases scope.normalized with ⟨h0,h1⟩
  by_cases hp : 0 < scope.blend
  · by_cases hq : scope.blend < 1
    · simp only [visits, hp, hq, ↓reduceIte, List.cons_append, List.nil_append, List.mem_cons,
        List.not_mem_nil, or_false, true_or, or_true, forall_const] at hl hr ⊢
      simp [hl,hr]
    · have he : scope.blend = 1 := by linarith
      simp_all [visits]
  · have he : scope.blend = 0 := by linarith
    simp_all [visits]

theorem aliased_activation_records_once (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (h : g.active i = false) :
    (setScenes t parent true [i,i] g).cell.recorded = g.cell.recorded + 1 ∧
    (setScenes t parent true [i,i] g).recordedActivations = g.recordedActivations + 2 := by
  constructor
  · simp only [setScenes, List.foldl_cons, List.foldl_nil]
    rw [active_not_recaptured t parent i _ (setScene_active t parent true i g)]
    simp [setScene, h, flag, capture, edit]
  · simpa using setScenes_records t parent true [i,i] g

-- If the old target already equals the parent's target, copying the current
-- value does not schedule a ramp. This is an implementation limitation.
theorem unchanged_capture_keeps_timer (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (h : g.cell.targets i = parent.targets i) :
    (capture t parent i g).cell.remaining = g.cell.remaining ∧
    (capture t parent i g).cell.age = g.cell.age := by
  simp [capture, edit, setTarget, h]

theorem zero_remaining_stays (t : Timing) (s : Cell) (h : s.remaining = 0) (frames : Nat) :
    (run t frames s).values = s.values := by
  induction frames generalizing s with
  | zero => rfl
  | succ n ih =>
    simp only [run]
    have hc : (compute t true s).remaining = 0 := by
      by_cases hf : 0 < s.force <;> simp [compute, h, hf]
    rw [ih _ hc]
    funext i
    by_cases hf : 0 < s.force <;> simp [compute, move, h, hf]

theorem capture_valid (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Valid t g.cell) : Valid t (capture t parent i g).cell := by
  apply edit_valid
  · rcases hg with ⟨hr,hf,ha,hvs,hts⟩
    refine ⟨hr,hf,ha,?_,hts⟩
    intro j
    dsimp
    split
    · exact hp.2.2.2.1 j
    · exact hvs j
  · exact hp.2.2.2.2 i

theorem capture_changed_ready (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell)
    (changed : g.cell.targets i ≠ parent.targets i) : Reset.Ready t (capture t parent i g).cell := by
  have hv := capture_valid t parent i g hp hg.1
  have hd : 0 < duration t g.cell.age := by
    unfold duration
    have ha := t.positive
    have hb := t.ordered
    omega
  have hr : 0 < (capture t parent i g).cell.remaining := by
    simp only [capture, edit, setTarget, changed, ↓reduceIte]
    split
    · exact hg.2.1 ‹g.cell.age = 0›
    · exact hd
  exact ⟨hv, fun _ => hr, fun h => False.elim (by omega)⟩

theorem changed_capture_finishes (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell)
    (changed : g.cell.targets i ≠ parent.targets i) :
    (run t t.maximum (capture t parent i g).cell).values i = parent.targets i := by
  rw [Reset.run_max_lands t _ (capture_changed_ready t parent i g hp hg changed)]
  exact (capture_current_target t parent i g).2.1

-- Minimal extra precondition: if capture neither changes the target nor inherits
-- a running ramp, the copied current value must already equal the copied target.
-- @cpp gesture_capture: excludes precisely the unchanged-target, idle-ramp defect.
-- @head gesture_capture: the same condition applies to the identical HEAD operation.
def CaptureCanSettle (parent : Cell) (i : Scene) (g : State) : Prop :=
  g.cell.targets i = parent.targets i → g.cell.remaining = 0 → parent.values i = parent.targets i

theorem capture_ready (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell) (safe : CaptureCanSettle parent i g) :
    Reset.Ready t (capture t parent i g).cell := by
  by_cases he : g.cell.targets i = parent.targets i
  · refine ⟨capture_valid t parent i g hp hg.1, ?_, ?_⟩
    · simpa only [capture, edit, setTarget, he, ↓reduceIte] using hg.2.1
    · intro hr
      have hz : g.cell.remaining = 0 := by
        simpa only [capture, edit, setTarget, he, ↓reduceIte] using hr
      have hs := safe he hz
      funext j
      by_cases hj : j = i
      · subst j
        simpa [capture, edit, setTarget, he] using hs
      · simpa [capture, edit, setTarget, he, hj] using congrFun (hg.2.2 hz) j
  · exact capture_changed_ready t parent i g hp hg he

-- As with every bounded-ramp theorem, the target is held fixed and processing
-- advances for the stated number of host frames; no intervening edits are modeled.
theorem capture_finishes (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell) (safe : CaptureCanSettle parent i g) :
    (run t t.maximum (capture t parent i g).cell).values i = parent.targets i := by
  rw [Reset.run_max_lands t _ (capture_ready t parent i g hp hg safe)]
  exact (capture_current_target t parent i g).2.1

theorem activating_finishes (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell) (inactive : g.active i = false)
    (safe : CaptureCanSettle parent i g) :
    (run t t.maximum (setScene t parent true i g).cell).values i = parent.targets i := by
  simpa only [setScene, inactive, Bool.not_false, Bool.and_self, ↓reduceIte, flag] using
    capture_finishes t parent i g hp hg safe

-- Necessity as well as sufficiency establishes minimality under the normal
-- validity/coherence assumptions, rather than merely imposing "target changed".
theorem capture_finishes_iff (t : Timing) (parent : Cell) (i : Scene) (g : State)
    (hp : Valid t parent) (hg : Reset.Ready t g.cell) :
    (run t t.maximum (capture t parent i g).cell).values i = parent.targets i ↔
      CaptureCanSettle parent i g := by
  constructor
  · intro landed he hz
    have hr : (capture t parent i g).cell.remaining = 0 :=
      (unchanged_capture_keeps_timer t parent i g he).1.trans hz
    rw [zero_remaining_stays t _ hr, (capture_current_target t parent i g).1] at landed
    exact landed
  · exact capture_finishes t parent i g hp hg

-- Accepted defect for now (user decision): preserve this counterexample and the
-- implementation behavior. The conditional theorem above excludes exactly its
-- failure case; accepting the defect does not assume unconditional convergence.
-- A kernel-checked counterexample: a settled unipolar gesture is reactivated
-- while its parent is ramping back to zero. The copied current value sticks.
def exampleTiming : Timing := ⟨96,2400,by decide,by decide⟩
def exampleParent : Cell := setTarget exampleTiming (initialFrom exampleTiming 1) 0 0
def exampleCaptured : State := setScene exampleTiming exampleParent true 0 (fresh exampleTiming false)

theorem accepted_counterexample_excluded :
    ¬ CaptureCanSettle exampleParent 0 (fresh exampleTiming false) := by
  unfold CaptureCanSettle
  decide

theorem capture_stalled_counterexample :
    Reset.Ready exampleTiming exampleParent ∧
    Reset.Ready exampleTiming (fresh exampleTiming false).cell ∧
    exampleCaptured.cell.values 0 = 1 ∧ exampleCaptured.cell.targets 0 = 0 ∧
    exampleCaptured.cell.remaining = 0 ∧
    ∀ frames, (run exampleTiming frames exampleCaptured.cell).values 0 = 1 := by
  have hp := Reset.setTarget_ready exampleTiming (initialFrom exampleTiming 1) 0 0
    (Reset.initial_ready exampleTiming 1 (by norm_num [UnitInterval])) (by norm_num [UnitInterval])
  refine ⟨hp, Reset.initial_ready _ _ (by norm_num [Edits.neutral, UnitInterval]),
    by decide, by decide, by decide, ?_⟩
  intro n
  rw [zero_remaining_stays _ _ (by decide)]
  decide

-- The subsequent knob edit does not necessarily repair the state: pushing an
-- already-zero target downward clamps to the same target and still starts no ramp.
theorem clamped_edit_does_not_repair_capture :
    let afterEdit := Edits.turn exampleTiming exampleCaptured.cell 0 false false (-1)
    afterEdit.targets 0 = 0 ∧ afterEdit.remaining = 0 ∧
    ∀ frames, (run exampleTiming frames afterEdit).values 0 = 1 := by
  dsimp only
  have ht : exampleCaptured.cell.targets 0 = 0 := by decide
  have he : Edits.turn exampleTiming exampleCaptured.cell 0 false false (-1) =
      {exampleCaptured.cell with recorded := exampleCaptured.cell.recorded + 1} := by
    norm_num [Edits.turn, Edits.combine, ht, clamp01, edit, setTarget]
  rw [he]
  refine ⟨by decide, by decide, ?_⟩
  intro n
  rw [zero_remaining_stays _ _ (by decide)]
  decide

theorem capture_not_always_coherent : ¬ Reset.Ready exampleTiming exampleCaptured.cell := by
  intro h
  have he := congrFun (h.2.2 (by decide)) 0
  have hc := capture_stalled_counterexample
  rw [hc.2.2.1, hc.2.2.2.1] at he
  norm_num at he

theorem toggle_valid (t : Timing) (scenes : List Scene) (g : State) (h : Valid t g.cell) :
    Valid t (toggle scenes g).cell := by rw [(toggle_preserves_data scenes g).1]; exact h

theorem deactivate_valid (t : Timing) (scenes : List Scene) (g : State) (h : Valid t g.cell) :
    Valid t (deactivate scenes g).cell := by rw [(deactivate_data scenes g).1]; exact h

end Encoder.Gestures
