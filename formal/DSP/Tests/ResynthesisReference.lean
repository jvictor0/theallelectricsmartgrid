import DSP.Resynthesis.Grain

open DSP DSP.Resynthesis

private def tau : Float := 6.2831853071795864769

private def math : GrainMath Float32 Float where
  widen := Float32.toFloat
  natSample := Float32.ofNat
  natPhase := Float.ofNat
  intPhase i := if i < 0 then -Float.ofNat i.natAbs else Float.ofNat i.natAbs
  phaseFloor := Float.floor
  phaseRound x :=
    let rounded := Int.ofNat x.abs.round.toUInt64.toNat
    if x < 0 then -rounded else rounded
  sqrt := Float32.sqrt
  magnitude z := (z.real * z.real + z.imag * z.imag).sqrt
  phase z := ((z.imag.atan2 z.real).toFloat / tau).toFloat32
  polar gain phase :=
    let angle := (phase - phase.floor) * tau
    ⟨gain * angle.cos.toFloat32, gain * angle.sin.toFloat32⟩
  thresholds peak := ((peak.toFloat * 0.000251188643150958).toFloat32,
    (peak.toFloat * 0.001).toFloat32)
  slewCoefficient cutoff := (1 - (-(tau * min 0.499 cutoff.toFloat)).exp).toFloat32
  hann {_} buffer := buffer

private def draw : StateM Nat Float32 := fun count =>
  ((Float32.ofNat (count % 8) - 4) / 8, count + 1)

private def spectrum (lane frame : Nat) : DFT Float32 4096 := ⟨fun bin =>
  let center := 32 + 8 * lane
  let amplitude : Float32 :=
    if frame = 3 then 0
    else if bin.val = center then 0.5
    else if bin.val + 1 = center then 0.25
    else if bin.val = center + 1 then 0.125
    else if bin.val = center + 8 then 0.0625
    else if bin.val = center + 9 then 0.03125
    else 0
  let amplitude := amplitude * (if frame % 2 = 0 then 1 else 0.75)
  if (frame + bin.val) % 2 = 0 then ⟨amplitude, 0⟩ else ⟨0, amplitude⟩⟩

private def controls (lane : Nat) : Controls Float32 :=
  ⟨if lane % 3 = 0 then ⟨1, 1⟩ else if lane % 3 = 1 then ⟨3, 2⟩ else ⟨1, 2⟩,
    0.75, 0.5, 1.0625, if lane % 2 = 0 then 0.5 else 0.0625⟩

private def run (width : Nat) (widthBound : width ≤ 4) : IO Unit := do
  let mut states : Vector (ChannelState Float32 Float 4096) width := Vector.ofFn fun _ => .clear
  let mut cursor := 0
  for grain in [1:6] do
    for lane in List.finRange width do
      let previous := spectrum lane.val (if grain = 4 then 1 else grain - 1)
      let current := spectrum lane.val grain
      let queue := PVDR.binaryQueue (n := 4096) (fun (a b : Float32) => decide (a < b))
      let ((st, out), next) := (processSpectrum math queue draw states[lane] (controls lane.val)
        previous current).run cursor
      states := states.set lane st
      cursor := next
      let center : ActiveBin 4096 := ⟨32 + 8 * lane.val - 1, by
        have h := lane.isLt
        omega⟩
      let gate := match st.gates.get center with
        | .low => 0
        | .triggered => 1
        | .high => 2
      let mut row := s!"{width} {grain} {lane.val} {cursor} {gate} {(st.magnitude.get center).toBits}"
      for osc in List.finRange 3 do
        row := row ++ s!" {((st.phase osc).get center).toBits}"
      for bin in List.finRange 128 do
        let z := out.coeff ⟨bin.val, by have h := bin.isLt; omega⟩
        row := row ++ s!" {z.real.toBits} {z.imag.toBits}"
      IO.println row

def main : IO Unit := do
  run 1 (by decide)
  run 3 (by decide)
  run 4 (by decide)
