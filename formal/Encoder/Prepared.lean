import Veil

set_option veil.smt.trust false
set_option veil.solver "grind"
set_option autoImplicit true

/-! Connected, named-root control protocol. Snapshots contain relevant scene,
target, source/amplitude and gesture inputs; `evaluate` is pure, with numerical
and schedule contracts proved in Evaluation/Schedule. A tree image includes all
derived child values; `rootOf` projects its root output. It need not be injective.
The module deliberately permits an edit or live-input change before the next
host frame, which is the stale-snapshot case SynchronizePreparedState handles.
-/
veil module EncoderPrepared

type snapshot
type treeImage
type outputValue
param maximum : Nat
immutable function evaluate : snapshot → treeImage
immutable function rootOf : treeImage → outputValue
immutable individual startup : snapshot

-- @cpp snapshot: scene/source inputs and m_preparedScene1/2/Blend snapshots
individual live : snapshot
individual saved : snapshot
-- @cpp synchronization: materialized child values versus current root output
individual tree : treeImage
individual output : outputValue
individual deferred : Bool
-- @cpp eligibility: force/revision/path guard in EncoderMatrix::Process
individual force : Nat
individual revision : Nat
individual scheduledRevision : Nat
individual editing : Bool
-- Ghost: records which branch the last host evaluation actually used.
individual usedPrepared : Bool
individual forceAtEvaluation : Nat
individual revisionAtEvaluation : Nat
individual scheduleAtEvaluation : Nat

-- Ghost state distinguishing a forced reset evaluation from an input-triggered one.
individual gestureClearPending : Bool
individual gestureClearAtEvaluation : Bool
individual resetPending : Bool
individual resetAtEvaluation : Bool
individual liveAtEvaluation : snapshot
individual evaluatedThisHost : Bool

#gen_state

-- @cpp initialization: startup begins on the recursive path.
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
  output := rootOf (evaluate startup)
  deferred := false
  force := maximum
  revision := 0
  scheduledRevision := 0
  editing := true
  usedPrepared := false
  forceAtEvaluation := 0
  revisionAtEvaluation := 0
  scheduleAtEvaluation := 0
}

-- @cpp changed: live inputs can change before the next ProcessHostFrame.
action inputs (next : snapshot) {
  live := next
}

-- @cpp invalidate: editing synchronizes before changing targets/topology.
action edit (next : snapshot) {
  if deferred then
    tree := evaluate saved
    deferred := false
  live := next
  force := maximum
  revision := revision + 1
}

-- @cpp gesture_clear_registry: next is the concrete post-deletion snapshot.
-- Root forcing applies even when the removed gesture leaves an empty mask.
action clear_gesture (next : snapshot) {
  require 0 < maximum
  live := next
  force := maximum
  revision := revision + 1
  gestureClearPending := true
}

-- @cpp reset: `next` is the result of the concrete scoped tree reset.
-- Derived output/history retain their last evaluation until the forced host pass.
action reset (next : snapshot) {
  require 0 < maximum
  live := next
  force := maximum
  revision := revision + 1
  resetPending := true
}

-- @cpp synchronization: importantly, this reads `saved`, not `live`.
action synchronize {
  if deferred then
    tree := evaluate saved
    deferred := false
}

-- @cpp rebuild: CanPrepare checks descendants as well; this action is enabled
-- only after that external eligibility check. `force` abstracts that condition.
action rebuild {
  require force = 0
  tree := evaluate saved
  deferred := false
  scheduledRevision := revision
  editing := false
}

-- @cpp prepared_process: logical host evaluation of a connected root.
-- Ramp arithmetic is factored into EncoderRuntime; force is its countdown.
action host {
  gestureClearAtEvaluation := gestureClearPending
  gestureClearPending := false
  resetAtEvaluation := resetPending
  resetPending := false
  liveAtEvaluation := live
  evaluatedThisHost := false
  forceAtEvaluation := force
  revisionAtEvaluation := revision
  scheduleAtEvaluation := scheduledRevision
  usedPrepared := false
  if force ≠ 0 ∨ revision ≠ scheduledRevision ∨ editing then
    evaluatedThisHost := true
    tree := evaluate live
    output := rootOf tree
    saved := live
    deferred := false
    editing := true
    force := force - 1
  else if live ≠ saved then
    evaluatedThisHost := true
    output := rootOf (evaluate live)
    saved := live
    deferred := true
    usedPrepared := true
}

invariant [output_matches_evaluated] output = rootOf (evaluate saved)
invariant [materialized_matches_evaluated] ¬ deferred → tree = evaluate saved
invariant [prepared_requires_current_schedule]
  usedPrepared → forceAtEvaluation = 0 ∧ revisionAtEvaluation = scheduleAtEvaluation
invariant [force_bounded] force ≤ maximum

invariant [reset_forces_evaluation] resetPending → 0 < force
invariant [reset_evaluates] resetAtEvaluation → evaluatedThisHost
invariant [reset_uses_live_snapshot] resetAtEvaluation → saved = liveAtEvaluation
invariant [reset_uses_recursive_path] resetAtEvaluation → ¬ usedPrepared

invariant [gesture_clear_forces_evaluation] gestureClearPending → 0 < force
invariant [gesture_clear_evaluates] gestureClearAtEvaluation → evaluatedThisHost
invariant [gesture_clear_uses_live_snapshot] gestureClearAtEvaluation → saved = liveAtEvaluation
invariant [gesture_clear_uses_recursive_path] gestureClearAtEvaluation → ¬ usedPrepared

#gen_spec
#check_invariants
#gen_theorems

end EncoderPrepared
