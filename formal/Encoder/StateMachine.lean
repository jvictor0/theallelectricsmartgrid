import Encoder.Reset
import Encoder.Edits
import Veil
import Encoder.Core

set_option veil.smt.trust false
set_option veil.solver "grind"
-- Veil's generated declarations implicitly bind its module parameters.
set_option autoImplicit true

attribute [grind .] Encoder.Reset.initial_ready Encoder.Reset.setTarget_ready Encoder.Reset.edit_ready
  Encoder.Reset.compute_ready Encoder.Reset.turn_ready Encoder.Reset.reset_ready Encoder.Reset.reset_valid
attribute [grind .] Encoder.initialFrom_valid Encoder.setTarget_valid Encoder.edit_valid Encoder.compute_valid Encoder.Edits.turn_valid

/-! Veil transition system for an encoder cell. Other nodes run the same
machine; Schedule.lean gives the dependency-order argument. External target
values are required to be normalized, as the knob-edit path supplies them.
Patch decoding is outside that precondition's proof boundary.
-/
veil module EncoderRuntime

param timing : Encoder.SettlingTiming
param initialValue : Encoder.DefaultValue
param ratio : Nat
param bipolar : Bool
type target
type frame
immutable function numeric : target → Rat
immutable function vector : target → Encoder.Lane → Rat
immutable function timestamp : frame → Nat

-- @cpp ramp: StateEncoderCell and TickDistributor
individual cell : Encoder.Cell
-- @cpp history: BankedEncoderCell::SetOutput
individual output : Encoder.Output
-- @cpp publish: EncoderMatrix::Publish. `published` is a ghost boundary flag:
-- caches are observable as current only after publication has completed.
individual cache : Encoder.Cache
individual published : Bool
individual cacheHost : Nat

#gen_state

-- @cpp initialization: normalized defaults and an initially forced update.
after_init {
  cell := Encoder.initialFrom timing initialValue.value
  output := ⟨fun _ => initialValue.value, fun _ => initialValue.value, 0⟩
  cache := ⟨fun _ _ => 0⟩
  published := false
  cacheHost := 0
}

-- @cpp recording: SetAndRecordValue; computation does not record edits.
action record_target (scene : Encoder.Scene) (value : target) {
  require Encoder.UnitInterval (numeric value)
  cell := Encoder.edit timing cell scene (numeric value)
}

-- @cpp target: StateEncoderCell::SetTarget
action set_target (scene : Encoder.Scene) (value : target) {
  require Encoder.UnitInterval (numeric value)
  cell := Encoder.setTarget timing cell scene (numeric value)
}

-- @cpp combine: one normalized scene write after edit routing/weighting.
action turn (scene : Encoder.Scene) (delta : target) (towardsZero : Bool) {
  cell := Encoder.Edits.turn timing cell scene bipolar towardsZero (numeric delta)
}

-- @cpp reset: this cell's recorded reset writes; tree clearing is proved in ResetEvaluation.
action reset (allScenes : Bool) (left right : Encoder.Scene) (position : target) {
  require Encoder.UnitInterval (numeric position)
  cell := Encoder.Reset.resetCell timing
    (Encoder.Reset.selected allScenes left right (numeric position)) initialValue.value cell
}

-- @cpp compute: BankedEncoderCell::Compute
action compute (advance : Bool) {
  cell := Encoder.compute timing advance cell
}

-- @cpp history: numerical evaluation is proved separately in Evaluation/Schedule.
action set_output (host : frame) (value : target) {
  output := Encoder.setOutput output (timestamp host) (vector value)
  published := false
}

-- @cpp publish: the production loop publishes every oversample slot.
action publish (host : frame) {
  cache := ⟨Encoder.publish output (timestamp host) ratio bipolar⟩
  cacheHost := timestamp host
  published := true
}

invariant [ramp_bounds] Encoder.Valid timing cell
invariant [ramp_coherent] Encoder.Reset.Ready timing cell
invariant [published_cache] published → cache.values = Encoder.publish output cacheHost ratio bipolar

#gen_spec
#check_invariants
#gen_theorems

end EncoderRuntime
