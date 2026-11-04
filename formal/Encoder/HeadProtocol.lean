import Veil

set_option veil.smt.trust false
set_option veil.solver "grind"
set_option autoImplicit true

/-! HEAD's eager recursive protocol for a connected named root. Snapshots and
pure evaluation have the same abstraction boundary as EncoderPrepared. Derived
children are always materialized; there is no deferred snapshot or schedule.
-/
veil module EncoderHeadProtocol

type snapshot
type treeImage
type outputValue
param maximum : Nat
immutable function evaluate : snapshot → treeImage
immutable function rootOf : treeImage → outputValue
immutable individual startup : snapshot

-- @head recursive: recursive Compute produces the whole derived tree.
individual live : snapshot
individual saved : snapshot
individual tree : treeImage
individual output : outputValue
individual force : Nat

-- Ghost state distinguishing a forced reset evaluation from an input-triggered one.
individual gestureClearPending : Bool
individual gestureClearAtEvaluation : Bool
individual resetPending : Bool
individual resetAtEvaluation : Bool
individual liveAtEvaluation : snapshot
individual evaluatedThisHost : Bool

#gen_state

-- @head initialization: initial forced update and normalized defaults.
after_init {
  gestureClearPending := false
  gestureClearAtEvaluation := false
  resetPending := false
  resetAtEvaluation := false
  liveAtEvaluation := startup
  evaluatedThisHost := false
  live := startup
  saved := startup
  tree := evaluate startup
  output := rootOf tree
  force := maximum
}

-- @head changed: source, amplitude and gesture changes are observed at host time.
action inputs (next : snapshot) {
  live := next
}

-- @head invalidate: editing forces the next evaluation; no deferred image exists.
action edit (next : snapshot) {
  live := next
  force := maximum
}

-- @head gesture_clear_registry: next is the concrete post-deletion snapshot.
-- Root forcing applies even when the removed gesture leaves an empty mask.
action clear_gesture (next : snapshot) {
  require 0 < maximum
  live := next
  force := maximum
  gestureClearPending := true
}

-- @head reset: `next` is the result of the concrete scoped tree reset.
-- Derived output/history retain their last evaluation until the forced host pass.
action reset (next : snapshot) {
  require 0 < maximum
  live := next
  force := maximum
  resetPending := true
}

-- @head runtime: ProcessHostFrame invokes Compute(true); clean roots can skip.
action host {
  gestureClearAtEvaluation := gestureClearPending
  gestureClearPending := false
  resetAtEvaluation := resetPending
  resetPending := false
  liveAtEvaluation := live
  evaluatedThisHost := false
  if force ≠ 0 ∨ live ≠ saved then
    evaluatedThisHost := true
    tree := evaluate live
    output := rootOf tree
    saved := live
    force := force - 1
}

invariant [output_matches_evaluated] output = rootOf (evaluate saved)
invariant [materialized_matches_evaluated] tree = evaluate saved
invariant [force_bounded] force ≤ maximum

invariant [reset_forces_evaluation] resetPending → 0 < force
invariant [reset_evaluates] resetAtEvaluation → evaluatedThisHost
invariant [reset_uses_live_snapshot] resetAtEvaluation → saved = liveAtEvaluation

invariant [gesture_clear_forces_evaluation] gestureClearPending → 0 < force
invariant [gesture_clear_evaluates] gestureClearAtEvaluation → evaluatedThisHost
invariant [gesture_clear_uses_live_snapshot] gestureClearAtEvaluation → saved = liveAtEvaluation

#gen_spec
#check_invariants
#gen_theorems

end EncoderHeadProtocol
