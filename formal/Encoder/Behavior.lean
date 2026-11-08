import Encoder.StateMachine
import Encoder.Motion
import Veil

set_option veil.smt.trust false
set_option veil.solver "grind"
set_option autoImplicit true

attribute [grind =] Encoder.setOutput_current

/-! Observable host-frame behavior, independent of evaluation path. A host
action represents one 48 kHz input sample, following any queued edits/input
updates. The reference kernel is pure; its numeric/graph equations are proved
separately in Evaluation, Motion and Schedule. No prepared/fallback decision,
schedule revision or storage layout occurs in this behavioral machine.

The modeled ramp is the existing leaf trajectory. A replacement trajectory
must establish the motion/convergence contracts separately. This does not
claim smoothness across arbitrary patch loads or discrete topology changes.
-/
veil module EncoderBehavior

param timing : Encoder.SettlingTiming
param initialValue : Encoder.DefaultValue
param ratio : Nat
param bipolar : Bool
type inputSnapshot
type target
immutable individual startup : inputSnapshot
immutable function numeric : target → Rat
immutable function evaluate : (Encoder.Scene → Rat) → inputSnapshot → Encoder.Lane → Rat

-- @cpp ramp: the current values advance independently of source motion.
individual cell : Encoder.Cell
-- @cpp changed: an input snapshot includes source values, amplitudes and gestures.
individual live : inputSnapshot
individual consumed : inputSnapshot
individual evaluatedCell : Encoder.Cell
-- @cpp host_gate: one transition per host frame, not a reduced control rate.
individual host : Nat
individual atHostBoundary : Bool
-- @cpp history: previous/current output are captured before eager publication.
individual output : Encoder.Output
-- @cpp publish: getters observe the published cache without evaluating the graph.
individual cache : Encoder.Cache

#gen_state

after_init {
  cell := Encoder.initialFrom timing initialValue.value
  live := startup
  consumed := startup
  evaluatedCell := cell
  host := 0
  atHostBoundary := true
  output := ⟨evaluate cell.values startup, evaluate cell.values startup, 0⟩
  cache := ⟨Encoder.publish output host ratio bipolar⟩
}

action inputs (next : inputSnapshot) {
  live := next
  atHostBoundary := false
}

action edit (scene : Encoder.Scene) (value : target) {
  require Encoder.UnitInterval (numeric value)
  cell := Encoder.edit timing cell scene (numeric value)
  atHostBoundary := false
}

action reset (allScenes : Bool) (left right : Encoder.Scene) (position : target) {
  require Encoder.UnitInterval (numeric position)
  cell := Encoder.Reset.resetCell timing
    (Encoder.Reset.selected allScenes left right (numeric position)) initialValue.value cell
  atHostBoundary := false
}

action host_frame {
  cell := Encoder.compute timing true cell
  consumed := live
  evaluatedCell := cell
  host := host + 1
  output := Encoder.setOutput output host (evaluate evaluatedCell.values consumed)
  cache := ⟨Encoder.publish output host ratio bipolar⟩
  atHostBoundary := true
}

invariant [ramp_coherent] Encoder.Reset.Ready timing cell
invariant [current_inputs] atHostBoundary → consumed = live
invariant [current_ramp] atHostBoundary → evaluatedCell.values = cell.values
invariant [evaluated_output] output.current = evaluate evaluatedCell.values consumed
invariant [published_cache] cache.values = Encoder.publish output host ratio bipolar

#gen_spec
#check_invariants
#gen_theorems

end EncoderBehavior
