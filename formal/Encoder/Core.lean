import Std

/-! Exact-arithmetic model of the encoder leaf and its output history.
`Rat` deliberately abstracts finite C++ floats; see README.md for the boundary.
Source-reference keys resolve to checked anchors in traceability.json.
-/
namespace Encoder

-- @cpp dimensions: SceneManager::x_numScenes and QuadFloat storage.
-- @head dimensions: shared arithmetic/control operation at the pinned revision.
abbrev Scene := Fin 8
abbrev Lane := Fin 4

-- @cpp numeric: StateEncoderCell::{ToValue,GetSceneTargetNormalizedValue}
-- @head numeric: shared arithmetic/control operation at the pinned revision.
def blend (left right t : Rat) : Rat := left * (1 - t) + right * t
def toValue (bipolar : Bool) (u : Rat) : Rat := if bipolar then 2 * u - 1 else u
def clamp01 (x : Rat) : Rat := if x < 0 then 0 else if 1 < x then 1 else x
def UnitInterval (x : Rat) : Prop := 0 ≤ x ∧ x ≤ 1

theorem clamp01_bounded (x : Rat) : UnitInterval (clamp01 x) := by
  unfold clamp01 UnitInterval
  split <;> (try split) <;> grind

theorem blend_left (a b : Rat) : blend a b 0 = a := by unfold blend; grind
theorem blend_right (a b : Rat) : blend a b 1 = b := by unfold blend; grind
theorem blend_midpoint (a b : Rat) : blend a b (1/2) = (a+b)/2 := by
  unfold blend; grind

-- @cpp ramp: TickDistributor and StateEncoderCell::MoveValuesTowardsTargets
-- @head ramp: shared arithmetic/control operation at the pinned revision.
structure Timing where
  minimum : Nat
  maximum : Nat
  positive : 0 < minimum
  ordered : minimum ≤ maximum

instance : Inhabited Timing := ⟨⟨96, 2400, by decide, by decide⟩⟩

structure Cell where
  values : Scene → Rat
  targets : Scene → Rat
  age : Nat
  remaining : Nat
  force : Nat
  recorded : Nat
  deriving Inhabited

-- @cpp initialization: BankedEncoderCell construction and CreateEncoder defaults.
-- @head initialization: shared arithmetic/control operation at the pinned revision.
def initialFrom (timing : Timing) (value : Rat) : Cell :=
  ⟨fun _ => value, fun _ => value, timing.maximum, 0, timing.maximum, 0⟩

structure DefaultValue where
  value : Rat
  normalized : UnitInterval value

instance : Inhabited DefaultValue := ⟨⟨0, by unfold UnitInterval; grind⟩⟩

-- @cpp ramp: TickDistributor::RecordChange
-- @head ramp: shared arithmetic/control operation at the pinned revision.
def duration (timing : Timing) (age : Nat) : Nat :=
  min timing.maximum (max timing.minimum age)

-- @cpp target: StateEncoderCell::SetTarget (does not itself record).
-- @head target: shared arithmetic/control operation at the pinned revision.
def setTarget (timing : Timing) (s : Cell) (scene : Scene) (value : Rat) : Cell :=
  if s.targets scene = value then s else
  { s with targets := fun i => if i = scene then value else s.targets i
           age := 0
           remaining := if s.age = 0 then s.remaining else duration timing s.age
           force := timing.maximum }

-- @cpp recording: StateEncoderCell::SetAndRecordValue
-- @head recording: shared arithmetic/control operation at the pinned revision.
def edit (timing : Timing) (s : Cell) (scene : Scene) (value : Rat) : Cell :=
  { setTarget timing s scene value with recorded := s.recorded + 1 }

def move (s : Cell) (i : Scene) : Rat :=
  if s.remaining = 0 then s.values i
  else if s.remaining = 1 then s.targets i
  else clamp01 (s.values i + (s.targets i - s.values i) / (s.remaining : Rat))

-- @cpp compute: BankedEncoderCell::Compute; only advanceState advances time.
-- @head compute: shared arithmetic/control operation at the pinned revision.
def compute (timing : Timing) (advance : Bool) (s : Cell) : Cell :=
  if advance ∧ s.force > 0 then
    { s with values := move s
             age := min timing.maximum (s.age + 1)
             remaining := s.remaining - 1
             force := s.force - 1 }
  else s

def Valid (timing : Timing) (s : Cell) : Prop :=
  s.remaining ≤ s.force ∧ s.force ≤ timing.maximum ∧ s.age ≤ timing.maximum ∧
  (∀ i, UnitInterval (s.values i)) ∧ (∀ i, UnitInterval (s.targets i))

theorem initialFrom_valid (t : Timing) (value : Rat) (hv : UnitInterval value) :
    Valid t (initialFrom t value) := by
  simp [Valid, initialFrom, hv]

theorem edit_records_once (t : Timing) (s : Cell) (i : Scene) (v : Rat) :
    (edit t s i v).recorded = s.recorded + 1 := rfl

theorem unchanged_target_noop (t : Timing) (s : Cell) (i : Scene) :
    setTarget t s i (s.targets i) = s := by simp [setTarget]

theorem target_changes_do_not_jump (t : Timing) (s : Cell) (i : Scene) (v : Rat) :
    (setTarget t s i v).values = s.values := by unfold setTarget; split <;> rfl

theorem other_scenes_unchanged (t : Timing) (s : Cell) (i j : Scene) (v : Rat)
    (h : j ≠ i) : (setTarget t s i v).targets j = s.targets j := by
  unfold setTarget; split <;> simp_all

theorem changed_target_forces (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (h : s.targets i ≠ v) : (setTarget t s i v).force = t.maximum := by
  simp [setTarget, h]

theorem same_sample_edits_keep_duration (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (h : s.age = 0) : (setTarget t s i v).remaining = s.remaining := by
  unfold setTarget; split <;> simp_all

theorem setTarget_valid (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (hs : Valid t s) (hv : UnitInterval v) : Valid t (setTarget t s i v) := by
  unfold setTarget
  split
  · exact hs
  · rcases hs with ⟨hr, hf, ha, hvs, hts⟩
    refine ⟨?_, Nat.le_refl _, Nat.zero_le _, hvs, ?_⟩
    · dsimp
      split
      · omega
      · exact Nat.min_le_left _ _
    · intro j
      change UnitInterval (if j = i then v else s.targets j)
      split
      · exact hv
      · exact hts j

theorem edit_valid (t : Timing) (s : Cell) (i : Scene) (v : Rat)
    (hs : Valid t s) (hv : UnitInterval v) : Valid t (edit t s i v) := by
  exact setTarget_valid t s i v hs hv

-- @cpp persistence: StateEncoderCell::ToJSON and CopyToScene.
-- @head persistence: shared arithmetic/control operation at the pinned revision.
def savedValues (s : Cell) (bipolar : Bool) : Scene → Rat := fun i => toValue bipolar (s.targets i)
def copyScene (t : Timing) (s : Cell) (left right destination : Scene) (b : Rat) : Cell :=
  edit t s destination (blend (s.targets left) (s.targets right) b)

theorem saving_preserves_pending_targets (s : Cell) (bipolar : Bool) (i : Scene) :
    savedValues s bipolar i = toValue bipolar (s.targets i) := rfl

theorem copying_uses_targets (t : Timing) (s : Cell) (l r d : Scene) (b : Rat) :
    (copyScene t s l r d b).targets d = blend (s.targets l) (s.targets r) b := by
  unfold copyScene edit setTarget
  split <;> simp_all

theorem compute_readonly (t : Timing) (s : Cell) : compute t false s = s := by
  simp [compute]

theorem compute_does_not_record (t : Timing) (s : Cell) (a : Bool) :
    (compute t a s).recorded = s.recorded := by unfold compute; split <;> rfl

theorem compute_preserves_targets (t : Timing) (s : Cell) (a : Bool) :
    (compute t a s).targets = s.targets := by unfold compute; split <;> rfl

theorem idle_compute_noop (t : Timing) (s : Cell) (h : s.force = 0) :
    compute t true s = s := by simp [compute, h]

theorem compute_valid (t : Timing) (s : Cell) (a : Bool) (hs : Valid t s) :
    Valid t (compute t a s) := by
  unfold compute
  split
  · rcases hs with ⟨hr, hf, ha, hvs, hts⟩
    refine ⟨by dsimp; omega, by dsimp; omega, Nat.min_le_left _ _, ?_, hts⟩
    intro i
    unfold move
    split
    · exact hvs i
    · split
      · exact hts i
      · exact clamp01_bounded _
  · exact hs

def run (t : Timing) : Nat → Cell → Cell
  | 0, s => s
  | n+1, s => run t n (compute t true s)

theorem force_countdown (t : Timing) (n : Nat) (s : Cell) :
    (run t n s).force = s.force - n := by
  induction n generalizing s with
  | zero => simp [run]
  | succ n ih =>
    simp only [run, ih]
    by_cases h : 0 < s.force
    · simp [compute, h]; omega
    · have hz : s.force = 0 := by omega
      simp [compute, hz]

-- @cpp refresh: SetStateRecursive starts the full counter even without a ramp.
-- @head refresh: shared arithmetic/control operation at the pinned revision.
theorem endpoint_refresh_keeps_force_active (t : Timing) (s : Cell) (n : Nat)
    (h : n < t.maximum) :
    (run t n { s with force := t.maximum }).force > 0 := by
  rw [force_countdown]
  dsimp
  omega

theorem run_preserves_targets (t : Timing) (n : Nat) (s : Cell) :
    (run t n s).targets = s.targets := by
  induction n generalizing s with
  | zero => rfl
  | succ n ih => simp [run, ih, compute_preserves_targets]

theorem ramp_finishes (t : Timing) (n : Nat) (s : Cell)
    (hn : s.remaining = n+1) (hf : n+1 ≤ s.force) :
    (run t (n+1) s).values = s.targets ∧ (run t (n+1) s).remaining = 0 := by
  induction n generalizing s with
  | zero =>
    have hp : 0 < s.force := by omega
    simp only [run, compute, true_and, hp, ↓reduceIte, hn]
    constructor
    · funext i; simp [move, hn]
    · trivial
  | succ n ih =>
    have hp : 0 < s.force := by omega
    have hr : (compute t true s).remaining = n+1 := by simp [compute, hp, hn]
    have hf' : n+1 ≤ (compute t true s).force := by simp [compute, hp]; omega
    simpa [run, compute_preserves_targets] using ih (compute t true s) hr hf'

-- @cpp history: BankedEncoderCell::SetOutput and GetInterpolatedValue
-- @head history: shared arithmetic/control operation at the pinned revision.
structure Output where
  current : Lane → Rat
  previous : Lane → Rat
  stamp : Nat
  deriving Inhabited

def setOutput (s : Output) (host : Nat) (value : Lane → Rat) : Output :=
  ⟨value, if s.stamp = host then s.previous else s.current, host⟩

def previousWeight (slot ratio : Nat) : Rat := ((ratio - 1 - slot : Nat) : Rat) / (ratio : Rat)
def readOutput (s : Output) (host slot ratio : Nat) (bipolar : Bool) (lane : Lane) : Rat :=
  toValue bipolar (if s.stamp = host then
    s.current lane + (s.previous lane - s.current lane) * previousWeight slot ratio
    else s.current lane)

-- @cpp publish: EncoderMatrix::Publish; no separate cache traversal.
-- @head publish: shared arithmetic/control operation at the pinned revision.
def publish (s : Output) (host ratio : Nat) (bipolar : Bool) : Nat → Lane → Rat :=
  fun slot lane => readOutput s host slot ratio bipolar lane

structure Cache where
  values : Nat → Lane → Rat
  deriving Inhabited

theorem same_host_preserves_history (s : Output) (h : Nat) (v w : Lane → Rat) :
    (setOutput (setOutput s h v) h w).previous = (setOutput s h v).previous := by
  simp [setOutput]

theorem first_write_saves_previous (s : Output) (h : Nat) (v : Lane → Rat)
    (hn : s.stamp ≠ h) : (setOutput s h v).previous = s.current := by simp [setOutput, hn]

theorem last_slot_current (s : Output) (h ratio : Nat) (b : Bool) (lane : Lane) :
    readOutput s h (ratio-1) ratio b lane = toValue b (s.current lane) := by
  unfold readOutput previousWeight
  simp only [Nat.sub_self]
  congr 1
  split <;> grind

theorem ratio_one_current (s : Output) (h : Nat) (b : Bool) (lane : Lane) :
    readOutput s h 0 1 b lane = toValue b (s.current lane) := by
  exact last_slot_current s h 1 b lane

theorem unchanged_frame_constant (s : Output) (h slot ratio : Nat) (b : Bool) (lane : Lane)
    (hn : s.stamp ≠ h) : publish s h ratio b slot lane = toValue b (s.current lane) := by
  simp [publish, readOutput, hn]

end Encoder
