import DSP.Tests.NaiveReuppingLadder

/-! Numerical exploration only. The sample operations are the existing Lean
ladder, with an explicitly supplied floor-free coefficient. Native tanh models
the real proof interpretation; tangent mode substitutes its slope at zero.
Finite-window measurements are not a convergence proof. -/

namespace DSP.Tests.FloorFreeProbe

def prepare (gain : Float) : DSP.NaiveReuppingLadder Float :=
  letI : Tanh Float := ⟨Float.tanh⟩
  DSP.NaiveReuppingLadder.computeCoefficients ⟨0.01, 0.8, gain⟩
    (DSP.Tests.NaiveReuppingLadder.seeded 0)

def step (tangent : Bool) (s : DSP.NaiveReuppingLadder Float) (input : Float) :
    DSP.NaiveReuppingLadder Float :=
  letI : Tanh Float := ⟨if tangent then id else Float.tanh⟩
  let pi := s.inputEnvelope.power
  let excess := if pi == 0 then 0 else max 0 (s.feedbackEnvelope.power - s.referencePowerGain * pi) / pi
  let k := s.baseFeedback / (1 + s.dampingScale * excess)
  let next := DSP.NaiveReuppingLadder.step {s with baseFeedback := k, dampingScale := 0} ⟨input⟩
  {next with baseFeedback := s.baseFeedback, dampingScale := s.dampingScale}

def probe (period : Nat) (amplitudeLabel : String) (amplitude gain : Float) (tangent : Bool) : IO Unit := do
  let mut state := prepare gain
  let window := period * 512
  let mut previous := Array.replicate period (0 : Float)
  for block in [0:8] do
    let mut power : Float := 0
    let mut realPart : Float := 0
    let mut imagPart : Float := 0
    let mut thirdReal : Float := 0
    let mut thirdImag : Float := 0
    let mut feedback : Float := 0
    let mut kMin := state.baseFeedback
    let mut kMax : Float := 0
    let mut recurrenceError : Float := 0
    for i in [0:window] do
      let j := i % period
      let phase := 6.2831853071795864769 * j.toFloat / period.toFloat
      state := step tangent state (amplitude * phase.cos)
      let output := state.output / amplitude
      let error := output - previous[j]!
      previous := previous.set! j output
      recurrenceError := recurrenceError + error * error
      power := power + output * output
      realPart := realPart + output * phase.cos
      imagPart := imagPart - output * phase.sin
      thirdReal := thirdReal + output * (3 * phase).cos
      thirdImag := thirdImag - output * (3 * phase).sin
      feedback := feedback + state.kEff
      kMin := min kMin state.kEff
      kMax := max kMax state.kEff
    if block >= 5 then
      let scale := 2 / window.toFloat
      IO.println s!"period={period} amplitude={amplitudeLabel} gain={gain} tangent={tangent} block={block} re={scale * realPart} im={scale * imagPart} rmsGain={(2 * power / window.toFloat).sqrt} h3={(scale * thirdReal)^2 + (scale * thirdImag)^2 |>.sqrt} repeatError={(recurrenceError / window.toFloat).sqrt} kMean={feedback / window.toFloat} kMin={kMin} kMax={kMax}"

end DSP.Tests.FloorFreeProbe

def main : IO Unit := do
  for period in [96, 384, 32] do
    for (label, amplitude) in [("1e-3", 0.001), ("1e-5", 0.00001), ("1e-7", 0.0000001)] do
      DSP.Tests.FloorFreeProbe.probe period label amplitude 2 false
    DSP.Tests.FloorFreeProbe.probe period "1" 1 2 true
  DSP.Tests.FloorFreeProbe.probe 96 "1e-7" 0.0000001 0.1 false
