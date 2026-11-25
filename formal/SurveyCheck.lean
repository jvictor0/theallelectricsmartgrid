import DSP.Survey.Certificate
import Lean.Data.Json

open Lean

private def checkRow (row : Json) : Except String Json := do
  let values ← (← row.getArr?).mapM Json.getNat?
  match values with
  | #[a, b, d, e, n, h] =>
      pure (toJson (DSP.Survey.Certificate.accepts a b d e n h))
  | _ => throw "expected [a, b, d, e, n, h]"

def main (args : List String) : IO UInt32 := do
  let (input, destination) ← match args with
    | [] => pure (← (← IO.getStdin).readToEnd, none)
    | [input, output] => pure (← IO.FS.readFile input, some output)
    | _ => throw (IO.userError "Usage: survey_check [INPUT.json OUTPUT.json]")
  let result := do
    let rows ← (← Json.parse input).getArr?
    return Json.arr (← rows.mapM checkRow)
  match result with
  | .ok output =>
      match destination with
      | some path => IO.FS.writeFile path output.compress
      | none => IO.println output.compress
      return 0
  | .error reason =>
      IO.eprintln s!"survey_check: {reason}"
      return 1
