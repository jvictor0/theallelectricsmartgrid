import Encoder.Reachability
import Encoder.Prepared
import Encoder.Evaluation
import Encoder.Control
import Encoder.Schedule
import Encoder.Crossfade
import Encoder.Curve
import Encoder.HeadReachability
import Encoder.HeadProtocol
import Encoder.ResetEvaluation
import Encoder.GestureTree
import Encoder.GestureMachine
import Encoder.HeadGestureMachine
import Lean.Util.CollectAxioms

/-! Build-time trust audit. Inspect transitive dependencies, including generated
Veil verification conditions, rather than searching only for proof keywords.
The allowlist is Lean's standard logical foundations. Solver/admission/native
evaluation axioms are not accepted.
-/
open Lean in
run_cmd do
  let env ← getEnv
  let required := #[
    ``Encoder.ramp_finishes,
    ``Encoder.mix_bounded,
    ``Encoder.same_host_preserves_history,
    ``Encoder.Schedule.prepared_equals_recursive,
    ``EncoderRuntime.reachable_safe,
    ``EncoderRuntime.record_target_ramp_bounds,
    ``EncoderRuntime.publish_published_cache,
    ``EncoderPrepared.host_output_matches_evaluated,
    ``EncoderPrepared.synchronize_materialized_matches_evaluated,
    ``EncoderPrepared.host_prepared_requires_current_schedule]
  for name in required do
    unless (env.find? name).any (fun c => c matches .thmInfo _) do
      throwError "missing proved encoder contract: {name}"
  let raw ← IO.FS.readFile "formal/Encoder/requirements.json"
  let contracts ← match Json.parse raw >>= Json.getArr? with
    | .ok xs => pure xs
    | .error e => throwError "invalid EARS contract manifest: {e}"
  if contracts.isEmpty then
    throwError "EARS contract manifest is empty"
  for contract in contracts do
    let names ← match contract.getObjValAs? (Array String) "theorems" with
      | .ok xs => pure xs
      | .error e => throwError "invalid theorem list: {e}"
    if names.isEmpty then
      throwError "EARS contract has no theorem"
    for text in names do
      let name := text.toName
      unless (env.find? name).any (fun c => c matches .thmInfo _) do
        throwError "EARS manifest names a missing theorem: {name}"
  let headRaw ← IO.FS.readFile "formal/Encoder/head.json"
  let headContracts ← match Json.parse headRaw >>= (fun j => j.getObjValAs? (Array Json) "contracts") with
    | .ok xs => pure xs
    | .error e => throwError "invalid HEAD contract manifest: {e}"
  for contract in headContracts do
    let status ← match contract.getObjValAs? String "status" with
      | .ok s => pure s
      | .error e => throwError "invalid HEAD status: {e}"
    let names ← match contract.getObjValAs? (Array String) "theorems" with
      | .ok xs => pure xs
      | .error e => throwError "invalid HEAD theorem list: {e}"
    if names.isEmpty && status != "not-applicable" then
      throwError "HEAD contract has no theorem"
    for text in names do
      let name := text.toName
      unless (env.find? name).any (fun c => c matches .thmInfo _) do
        throwError "HEAD manifest names a missing theorem: {name}"
  let allowed := #[``propext, ``Classical.choice, ``Quot.sound]
  let mut declarations := 0
  let mut theorems := 0
  for (name, info) in env.constants.toList do
    let localSpec := ["Encoder.", "EncoderRuntime.", "EncoderPrepared.",
      "EncoderHeadRuntime.", "EncoderHeadProtocol.",
      "EncoderGestureRuntime.", "EncoderHeadGestureRuntime."].any
      (fun ns => name.toString.startsWith ns)
    if localSpec then
      declarations := declarations + 1
      if info matches .thmInfo _ then
        theorems := theorems + 1
      if info matches .axiomInfo _ then
        throwError "encoder specification introduces an axiom: {name}"
      for dependency in (← collectAxioms name) do
        unless allowed.contains dependency do
          throwError "unapproved axiom {dependency} in {name}"
  logInfo m!"Encoder proof audit: {contracts.size} EARS contracts, {headContracts.size} HEAD classifications, {theorems} theorems, {declarations} declarations; only standard Lean axioms."
