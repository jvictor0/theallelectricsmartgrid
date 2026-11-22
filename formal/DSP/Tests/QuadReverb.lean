import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.Tests.QuadReverb

private def close (a b : Float) : Bool := (a - b).abs < 1.0e-12

private def checkCubicTap : IO Unit := do
  let mut buffer : DelayBuffer 4 (Fin 2 → Float) := DelayBuffer.empty (by decide)
  for n in [:6] do
    let x := n.toFloat
    buffer := buffer.push ![x * x, 3 * x - 2]
  let interpolated := buffer.readCubic ![2.5, -2.75]
  unless close (interpolated 0) 12.25 && close (interpolated 1) 7.75 do
    throw (IO.userError "Reverb cubic taps failed on independent fractional delays and zero reflection")
  let offset := buffer.readCubicOffset ![2, 2] 0.5
  unless close (offset 0) 12.25 && close (offset 1) 8.5 do
    throw (IO.userError "Reverb fractional offset failed on a wrapped ring")
  let reflected := buffer.readCubic ![5.5, 5.25]
  unless close (reflected 0) 12.25 && close (reflected 1) 7.75 do
    throw (IO.userError "Reverb cubic taps failed to reflect at capacity")
  let newest := buffer.readCubic (fun _ => 1)
  unless close (newest 0) 25 && close (newest 1) 13 do
    throw (IO.userError "Integer reverb delay failed at the wrapped write head")
  let oldest := buffer.readCubic ![0, 4]
  unless close (oldest 0) 4 && close (oldest 1) 4 do
    throw (IO.userError "Reverb cubic delay failed at zero or full capacity")

def check : IO Unit := do
  checkCubicTap
  let spec : SquiggleBoyQuadReverb.LogicalSpec Float :=
    { delaySamples := fun _ => 2, feedback := fun _ => 0.8
      dampingBase := fun _ => 0.1, dampingWidth := fun _ => 2 }
  let initial := SquiggleBoyQuadReverb.init spec
  let silence : Fin 4 → Float := fun _ => 0
  let noFeedback := { spec with feedback := fun _ => 0 }
  let tick := fun (state : SquiggleBoyQuadReverb Float) tuning (audio : Fin 4 → Float) =>
    (state.computeCoefficients tuning).step ⟨audio⟩
  let first := tick initial spec ![1, 0, 0, 0]
  let second := tick first spec silence
  let fourth := tick (tick second spec silence) spec silence
  let fourthDry := tick (tick second noFeedback silence) noFeedback silence
  let high := Trig.tanPi (0.1 : Float)
  let low := Trig.tanPi (0.2 : Float)
  let immediateGain := 0.36 * (1 - high / (1 + high)) * (low / (1 + low))
  let saturator := NormalizedTanh.prepare (R := Fin 4 → Float) ⟨fun _ => 0.5⟩
  let firstPass := NormalizedTanh.apply saturator ![0.1296, 0, 0, 0]
  let mixed := squiggleBoyReverbMix firstPass
  let feedback := squiggleBoyReverbMix (NormalizedTanh.apply saturator (second.y * spec.feedback))
  for ch in List.finRange 4 do
    let expected := immediateGain * mixed ch
    unless first.y ch == 0 && close (second.y ch) expected && (second.y ch).abs > 1.0e-5 do
      throw (IO.userError s!"Reverb first pass failed at channel {ch}")
    unless close (fourth.y ch - fourthDry.y ch) (immediateGain * feedback ch) do
      throw (IO.userError s!"Reverb did not feed the previous output directly back at channel {ch}")
  IO.println "Quad reverb: cubic delay, four-channel first pass and internal feedback checks passed."

end DSP.Tests.QuadReverb
