import PhaseTime.Reachability
import PhaseTime.Slew
import PhaseTime.InverseGrid
import PhaseTime.AnalysisStability
import PhaseTime.FixedCell
import PhaseTime.Concurrency
import Lean.Util.CollectAxioms

open Lean in
run_cmd do
  let env ← getEnv
  let raw ← IO.FS.readFile "formal/PhaseTime/requirements.json"
  let contracts ← match Json.parse raw >>= Json.getArr? with
    | .ok xs => pure xs
    | .error e => throwError "invalid phase-time contracts: {e}"
  if contracts.isEmpty then throwError "empty phase-time contracts"
  for contract in contracts do
    let names ← match contract.getObjValAs? (Array String) "theorems" with
      | .ok xs => pure xs
      | .error e => throwError "invalid theorem list: {e}"
    if names.isEmpty then throwError "phase-time contract has no theorem"
    for text in names do
      let name := text.toName
      unless (env.find? name).any (fun c => c matches .thmInfo _) do
        throwError "missing phase-time theorem: {name}"
  let allowed := #[``propext, ``Classical.choice, ``Quot.sound]
  let mut theorems := 0
  for (name, info) in env.constants.toList do
    if name.toString.startsWith "PhaseTime." || name.toString.startsWith "PhaseRuntime." then
      if info matches .thmInfo _ then theorems := theorems + 1
      if info matches .axiomInfo _ then throwError "phase-time model introduces an axiom: {name}"
      for dependency in (← collectAxioms name) do
        unless allowed.contains dependency do
          throwError "unapproved axiom {dependency} in {name}"
  logInfo m!"Phase-time proof audit: {contracts.size} contracts, {theorems} theorems; only standard Lean axioms."
