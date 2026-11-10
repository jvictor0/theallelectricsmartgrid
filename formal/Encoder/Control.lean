import Encoder.Core

namespace Encoder

-- @cpp dimensions: BankedEncoderCell modulation/gesture slot counts.
-- @head dimensions: shared arithmetic/control operation at the pinned revision.
abbrev Source := Fin 15
abbrev Gesture := Fin 16

-- @cpp changed: manager gathering maps shared source changes into local slots;
-- direct replay uses ModulatorValues::ComputeChanged, and gesture changes use
-- GestureValues::ComputeChanged.
-- @head changed: shared arithmetic/control operation at the pinned revision.
structure Inputs where
  source : Source → Lane → Rat
  amplitude : Source → Lane → Rat
  gesture : Gesture → Rat
  selected : Gesture → Bool
  deriving Inhabited

def sourceChanged (old now : Inputs) (i : Source) : Prop :=
  old.source i ≠ now.source i ∨ old.amplitude i ≠ now.amplitude i

def gestureChanged (old now : Inputs) (i : Gesture) : Prop :=
  old.gesture i ≠ now.gesture i ∨ old.selected i ≠ now.selected i

-- @cpp dirty: BankedEncoderCell::Compute and EncoderMatrix::Process
-- @head dirty: shared arithmetic/control operation at the pinned revision.
def dirty (blendChanged : Prop) (force : Nat) (sources : List Source)
    (gestures : List Gesture) (old now : Inputs) : Prop :=
  blendChanged ∨ force ≠ 0 ∨
  (∃ i ∈ sources, sourceChanged old now i) ∨ (∃ i ∈ gestures, gestureChanged old now i)

theorem amplitude_change_invalidates (b : Prop) (f : Nat) (ss : List Source)
    (gs : List Gesture) (old now : Inputs) (i : Source)
    (active : i ∈ ss) (changed : old.amplitude i ≠ now.amplitude i) :
    dirty b f ss gs old now := by
  exact Or.inr (Or.inr (Or.inl ⟨i, active, Or.inr changed⟩))

theorem gesture_selection_invalidates (b : Prop) (f : Nat) (ss : List Source)
    (gs : List Gesture) (old now : Inputs) (i : Gesture)
    (active : i ∈ gs) (changed : old.selected i ≠ now.selected i) :
    dirty b f ss gs old now := by
  exact Or.inr (Or.inr (Or.inr ⟨i, active, Or.inr changed⟩))

theorem unchanged_inputs_skip (ss : List Source) (gs : List Gesture) (s : Inputs) :
    ¬ dirty False 0 ss gs s s := by simp [dirty, sourceChanged, gestureChanged]

-- @cpp invalidate: BankedEncoderCell::SetForceUpdateRecursive.
-- @head invalidate: shared arithmetic/control operation at the pinned revision.
-- An explicit ancestor path includes the edited cell. No bounded tree depth.
def forcePath (maximum : Nat) (path : List Nat) (counters : Nat → Nat) : Nat → Nat :=
  fun i => if i ∈ path then maximum else counters i

theorem ancestor_forced (maximum : Nat) (path : List Nat) (counters : Nat → Nat)
    (i : Nat) (h : i ∈ path) : forcePath maximum path counters i = maximum := by
  simp [forcePath, h]

theorem unrelated_counter_unchanged (maximum : Nat) (path : List Nat) (counters : Nat → Nat)
    (i : Nat) (h : i ∉ path) : forcePath maximum path counters i = counters i := by
  simp [forcePath, h]

-- @cpp eligibility: EncoderMatrix::CanPrepare and Process fallback guard.
inductive Path where
  | simple | graph | editing
  deriving DecidableEq, Inhabited

structure Preparation where
  connected : Bool
  force : Nat
  revision : Nat
  scheduledRevision : Nat
  path : Path
  deriving Inhabited

def fallback (s : Preparation) : Bool :=
  s.force != 0 || s.revision != s.scheduledRevision || s.path == .editing || !s.connected

theorem prepared_requires_settled (s : Preparation) (h : fallback s = false) :
    s.force = 0 ∧ s.revision = s.scheduledRevision ∧ s.path ≠ .editing ∧ s.connected = true := by
  simpa [fallback, Bool.or_eq_false_iff, and_assoc] using h

theorem revision_mismatch_falls_back (s : Preparation) (h : s.revision ≠ s.scheduledRevision) :
    fallback s = true := by simp [fallback, h]

theorem active_ramp_falls_back (s : Preparation) (h : s.force ≠ 0) :
    fallback s = true := by simp [fallback, h]

-- @cpp synchronization: BankedEncoderCell::SynchronizePreparedState
structure Snapshot where
  left : Scene
  right : Scene
  blend : Rat
  inputs : Inputs
  deriving Inhabited

structure Deferred where
  live : Snapshot
  evaluated : Snapshot
  pending : Bool
  treeValues : Nat → Rat
  cell : Cell
  output : Output

def synchronize (evaluate : Snapshot → Nat → Rat) (s : Deferred) : Deferred :=
  if s.pending then { s with treeValues := evaluate s.evaluated, pending := false } else s

theorem synchronization_uses_evaluated_snapshot (eval : Snapshot → Nat → Rat) (s : Deferred)
    (h : s.pending = true) :
    (synchronize eval s).treeValues = eval s.evaluated := by simp [synchronize, h]

theorem synchronization_preserves_live_and_history (eval : Snapshot → Nat → Rat) (s : Deferred) :
    (synchronize eval s).live = s.live ∧ (synchronize eval s).cell = s.cell ∧
    (synchronize eval s).output = s.output := by
  unfold synchronize; split <;> simp

-- @cpp host_gate: EncoderBankBank::Process and ProcessHostFrame
-- @head host_gate: shared arithmetic/control operation at the pinned revision.
def onEngineSample {α : Type} (isHostFrame : Bool) (hostProcess : α → α) (s : α) : α :=
  if isHostFrame then hostProcess s else s

theorem non_host_sample_does_not_advance_matrix {α : Type} (p : α → α) (s : α) :
    onEngineSample false p s = s := rfl

-- @cpp roots: EncoderMatrix::Rebuild named-root iteration (no visible-page filter).
-- @head roots: shared arithmetic/control operation at the pinned revision.
def eligibleRoot (present named : Bool) : Bool := present && named

theorem named_hidden_root_is_eligible : eligibleRoot true true = true := rfl

end Encoder
