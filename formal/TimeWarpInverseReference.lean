import PhaseTime.InverseRuntime

open PhaseTime.InverseRuntime

private def parseBits (text : String) : IO Float := do
  let some bits := text.toNat? | throw (IO.userError s!"Invalid float bits: {text}")
  if bits ≥ UInt64.size then throw (IO.userError s!"Float bits out of range: {text}")
  return Float.ofBits (UInt64.ofNat bits)

def main (args : List String) : IO Unit := do
  let [capacityText, resolutionText] := args
    | throw (IO.userError "Expected capacity and resolution bits")
  let some (capacity + 1) := capacityText.toNat?
    | throw (IO.userError "Expected a positive capacity")
  let resolution ← parseBits resolutionText
  let input ← IO.getStdin
  let output ← IO.getStdout
  let mut state : State Float Float32 capacity := {}
  repeat
    let line ← input.getLine
    if line.isEmpty then break
    let [command, a, b] := line.trimAscii.toString.splitOn " "
      | throw (IO.userError s!"Invalid trace row: {line}")
    let phase ← parseBits a
    let time ← parseBits b
    match command with
    | "W" => state := write state resolution ⟨phase, time⟩
    | "R" =>
      match read state.payload (phase * resolution) time with
      | some found => output.putStrLn (toString found.toBits)
      | none => output.putStrLn "missing"
    | _ => throw (IO.userError s!"Unknown trace command: {command}")
