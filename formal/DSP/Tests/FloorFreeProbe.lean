import DSP.Tests.NaiveReuppingLadder
import DSP.Filters.QuadratureEnvelopeFollower

/-! Numerical exploration only. The sample operations are the existing Lean
ladder, with an explicitly supplied floor-free coefficient. Native tanh models
the real proof interpretation; tangent mode substitutes its slope at zero.
Finite-window measurements are not a convergence proof. -/

namespace DSP.Tests.FloorFreeProbe

inductive Detector where
  | power
  | quadratureCascade
  | quadratureSingle
  deriving BEq, Repr

def prepare (gain envelopeScale : Float) : DSP.NaiveReuppingLadder Float :=
  letI : Tanh Float := ⟨Float.tanh⟩
  let s := DSP.NaiveReuppingLadder.computeCoefficients ⟨0.01, 0.8, gain⟩
    (DSP.Tests.NaiveReuppingLadder.seeded 0)
  {s with
    inputEnvelope := {s.inputEnvelope with powerAlpha := min 1 (s.inputEnvelope.powerAlpha * envelopeScale)}
    feedbackEnvelope := {s.feedbackEnvelope with powerAlpha := min 1 (s.feedbackEnvelope.powerAlpha * envelopeScale)}}

def detectorPower (detector : Detector) (before after : BandPassedEnvelopeFollower Float) : Float :=
  if detector == .power then after.power else
    let oldSVF := if detector == .quadratureSingle then before.stage1 else before.stage2
    let svf := if detector == .quadratureSingle then after.stage1 else after.stage2
    (QuadratureEnvelopeFollower.observe ⟨oldSVF, before.power, before.powerAlpha⟩ svf).power

def step (tangent : Bool) (detector : Detector) (s : DSP.NaiveReuppingLadder Float) (input : Float) :
    DSP.NaiveReuppingLadder Float :=
  letI : Tanh Float := ⟨if tangent then id else Float.tanh⟩
  let pi := s.inputEnvelope.power
  let excess := if pi == 0 then 0 else max 0 (s.feedbackEnvelope.power - s.referencePowerGain * pi) / pi
  let k := s.baseFeedback / (1 + s.dampingScale * excess)
  let next := DSP.NaiveReuppingLadder.step {s with baseFeedback := k, dampingScale := 0} ⟨input⟩
  {next with
    baseFeedback := s.baseFeedback, dampingScale := s.dampingScale
    inputEnvelope := {next.inputEnvelope with power := detectorPower detector s.inputEnvelope next.inputEnvelope}
    feedbackEnvelope := {next.feedbackEnvelope with power := detectorPower detector s.feedbackEnvelope next.feedbackEnvelope}}

def probe (period : Nat) (amplitudeLabel : String) (amplitude gain envelopeScale : Float)
    (tangent : Bool) (detector : Detector) (blocks : Nat) : IO Unit := do
  let mut state := prepare gain envelopeScale
  let window := period * 512
  let mut previous := Array.replicate period (0 : Float)
  for block in [0:blocks] do
    let mut power : Float := 0
    let mut realPart : Float := 0
    let mut imagPart : Float := 0
    let mut thirdReal : Float := 0
    let mut thirdImag : Float := 0
    let mut feedback : Float := 0
    let mut kSquared : Float := 0
    let mut kSecondReal : Float := 0
    let mut kSecondImag : Float := 0
    let mut delayedReal : Float := 0
    let mut delayedImag : Float := 0
    let mut productReal : Float := 0
    let mut productImag : Float := 0
    let mut peak : Float := 0
    let mut kMin := state.baseFeedback
    let mut kMax : Float := 0
    let mut recurrenceError : Float := 0
    for i in [0:window] do
      let j := i % period
      let phase := 6.2831853071795864769 * j.toFloat / period.toFloat
      let delayed := state.stage4 / amplitude
      state := step tangent detector state (amplitude * phase.cos)
      let output := state.output / amplitude
      if output.isNaN || output.isInf then throw (IO.userError "nonfinite output in floor-free probe")
      let error := output - previous[j]!
      previous := previous.set! j output
      recurrenceError := recurrenceError + error * error
      power := power + output * output
      realPart := realPart + output * phase.cos
      imagPart := imagPart - output * phase.sin
      thirdReal := thirdReal + output * (3 * phase).cos
      thirdImag := thirdImag - output * (3 * phase).sin
      feedback := feedback + state.kEff
      kSquared := kSquared + state.kEff * state.kEff
      kSecondReal := kSecondReal + state.kEff * (2 * phase).cos
      kSecondImag := kSecondImag - state.kEff * (2 * phase).sin
      delayedReal := delayedReal + delayed * phase.cos
      delayedImag := delayedImag - delayed * phase.sin
      productReal := productReal + state.kEff * delayed * phase.cos
      productImag := productImag - state.kEff * delayed * phase.sin
      peak := max peak output.abs
      kMin := min kMin state.kEff
      kMax := max kMax state.kEff
    if block + 3 >= blocks then
      let scale := 2 / window.toFloat
      let kMean := feedback / window.toFloat
      IO.println s!"period={period} amplitude={amplitudeLabel} gain={gain} envelopeScale={envelopeScale} detector={repr detector} tangent={tangent} block={block} re={scale * realPart} im={scale * imagPart} rmsGain={(2 * power / window.toFloat).sqrt} h3={(scale * thirdReal)^2 + (scale * thirdImag)^2 |>.sqrt} repeatError={(recurrenceError / window.toFloat).sqrt} peak={peak} kMean={kMean} kMin={kMin} kMax={kMax} kStdDev={(max 0 (kSquared / window.toFloat - kMean*kMean)).sqrt} k2Amplitude={(scale*kSecondReal)^2 + (scale*kSecondImag)^2 |>.sqrt} mixRe={scale * (productReal - kMean*delayedReal)} mixIm={scale * (productImag - kMean*delayedImag)}"

end DSP.Tests.FloorFreeProbe

def main (args : List String) : IO Unit := do
  if args == ["smoothing"] then
    for scale in [1, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625] do
      DSP.Tests.FloorFreeProbe.probe 96 "1" 1 2 scale true .power (if scale < 0.05 then 16 else 8)
  else if args == ["quadrature"] then
    for detector in [DSP.Tests.FloorFreeProbe.Detector.quadratureCascade, .quadratureSingle] do
      for scale in [1, 8, 64, 512] do
        DSP.Tests.FloorFreeProbe.probe 96 "1" 1 2 scale true detector 8
  else if args == ["quadrature-check"] then
    for scale in [64, 512] do
      for period in [96, 384, 32] do
        DSP.Tests.FloorFreeProbe.probe period "1" 1 2 scale true .quadratureSingle 8
      DSP.Tests.FloorFreeProbe.probe 96 "1e-7" 0.0000001 2 scale false .quadratureSingle 8
      for gain in [1, 4, 8] do
        DSP.Tests.FloorFreeProbe.probe 96 "1" 1 gain scale true .quadratureSingle 8
  else if args.isEmpty then
    for period in [96, 384, 32] do
      for (label, amplitude) in [("1e-3", 0.001), ("1e-5", 0.00001), ("1e-7", 0.0000001)] do
        DSP.Tests.FloorFreeProbe.probe period label amplitude 2 1 false .power 8
      DSP.Tests.FloorFreeProbe.probe period "1" 1 2 1 true .power 8
    DSP.Tests.FloorFreeProbe.probe 96 "1e-7" 0.0000001 0.1 1 false .power 8
  else
    throw (IO.userError "expected no arguments, smoothing, quadrature, or quadrature-check")
