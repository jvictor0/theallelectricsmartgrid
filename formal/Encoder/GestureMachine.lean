import Encoder.GestureLifecycle
import Veil

set_option veil.smt.trust false
set_option veil.solver "grind"
set_option autoImplicit true

attribute [grind .] Encoder.Reset.initial_ready Encoder.Reset.edit_ready Encoder.Reset.compute_ready
  Encoder.Gestures.fresh_valid Encoder.Gestures.setScenes_valid Encoder.Gestures.toggle_valid
  Encoder.Gestures.deactivate_valid Encoder.compute_valid Encoder.initialFrom_valid Encoder.edit_valid

/-! One optional gesture and its parent. Parent target edits/compute may interleave
with allocation, activation, toggle, deletion and gesture processing. Gesture
safety is Valid, not Ready: activation's timer counterexample is proved separately.
-/
veil module EncoderGestureRuntime

param timing : Encoder.Timing
param initialValue : Encoder.DefaultValue
param bipolar : Bool
type target
immutable function numeric : target → Rat

individual parent : Encoder.Cell
individual gesture : Encoder.Gestures.State
individual allocated : Bool

#gen_state

-- @cpp gesture_allocate: logically absent slots have an initialized ghost payload.
after_init {
  parent := Encoder.initialFrom timing initialValue.value
  gesture := Encoder.Gestures.fresh timing bipolar
  allocated := false
}

-- @cpp recording: target edits in the parent may precede activation mid-ramp.
action parent_target (scene : Encoder.Scene) (value : target) {
  require Encoder.UnitInterval (numeric value)
  parent := Encoder.edit timing parent scene (numeric value)
}

-- @cpp compute: independent host advancement of the parent.
action parent_compute (advance : Bool) {
  parent := Encoder.compute timing advance parent
}

-- @cpp gesture_allocate: AddGesture preserves an existing allocation.
action ensure {
  if ¬ allocated then
    gesture := Encoder.Gestures.fresh timing bipolar
    allocated := true
}

-- @cpp gesture_capture: no target or current assignment occurs in inactive scenes.
action set_active (left right : Encoder.Scene) (position : target) (value : Bool) {
  require allocated
  require Encoder.UnitInterval (numeric position)
  gesture := Encoder.Gestures.setScenes timing parent value
    (Encoder.Gestures.visits left right (numeric position)) gesture
}

-- @cpp gesture_toggle: raw flips intentionally have no capture/recording behavior.
action toggle (left right : Encoder.Scene) (position : target) {
  require allocated
  require Encoder.UnitInterval (numeric position)
  gesture := Encoder.Gestures.toggle (Encoder.Gestures.visits left right (numeric position)) gesture
}

-- @cpp gesture_clear: clearing a missing slot is a no-op; an existing slot remains allocated.
action clear (left right : Encoder.Scene) (position : target) {
  require Encoder.UnitInterval (numeric position)
  if allocated then
    gesture := Encoder.Gestures.deactivate (Encoder.Gestures.visits left right (numeric position)) gesture
}

-- @cpp compute: the gesture uses its own timer, not the parent's timer.
action gesture_compute (advance : Bool) {
  require allocated
  gesture := {gesture with cell := Encoder.compute timing advance gesture.cell}
}

invariant [parent_coherent] Encoder.Reset.Ready timing parent
invariant [parent_bounds] Encoder.Valid timing parent
invariant [gesture_bounds] Encoder.Valid timing gesture.cell
invariant [unallocated_unchanged] ¬ allocated → gesture = Encoder.Gestures.fresh timing bipolar

#gen_spec
#check_invariants
#gen_theorems

end EncoderGestureRuntime
