import DSP.Filters.OnePoleZOHLowPass
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTLadder
import DSP.Filters.LinkwitzRileyCrossover

/-! Native execution checks for the generic definitions. These are runtime
tests, not proofs of floating-point equivalence to the real-number model. -/

open DSP

/-- Run a state-only step over inputs, reading one stored field after each sample. -/
def observe {S R : Type} (step : S → Sample R → S) (field : S → R) (initial : S)
    (inputs : List R) : List R × S :=
  inputs.foldl (fun (outputs, state) x =>
    let next := step state ⟨x⟩
    (outputs ++ [field next], next)) ([], initial)

def constant (n : Nat) (x : Float) : List Float := List.replicate n x

def tunedSVF (g k : Float) : TPTSVF Float :=
  let a1 := 1.0 / (g * (g + k) + 1.0)
  { k, a1, a2 := g * a1, a3 := g * (g * a1), ic1 := 0.0, ic2 := 0.0, lowPass := 0.0,
    bandPass := 0.0, highPass := 0.0, notch := 0.0 }

def close (a b : Float) : Bool := (a - b).abs < 1.0e-9

def main : IO Unit := do
  let (zoh, zohFinal) := observe OnePoleZOHLowPass.step (·.y)
    ({ alpha := 0.5, y := 0.0 } : OnePoleZOHLowPass Float) [1.0, 0.0, -1.0]
  unless zoh == [0.5, 0.25, -0.375] && zohFinal.alpha == 0.5 do
    throw (IO.userError "Float ZOH one-pole changing-input trajectory failed")
  let (passthrough, _) := observe OnePoleZOHLowPass.step (·.y)
    ({ alpha := 1.0, y := 12.0 } : OnePoleZOHLowPass Float) [-3.0]
  let (held, _) := observe OnePoleZOHLowPass.step (·.y)
    ({ alpha := 0.0, y := 12.0 } : OnePoleZOHLowPass Float) [-3.0]
  unless passthrough == [-3.0] && held == [12.0] do
    throw (IO.userError "Float ZOH one-pole coefficient endpoints failed")
  let (zoh32, _) := observe OnePoleZOHLowPass.step (·.y)
    ({ alpha := 0.5, y := 0.0 } : OnePoleZOHLowPass Float32) [1.0, 0.0, -1.0]
  unless zoh32 == [0.5, 0.25, -0.375] do
    throw (IO.userError "Float32 ZOH one-pole changing-input trajectory failed")

  -- g = 1/3 gives G = 1/4: y = s + (x - s)/4, then s becomes 2y - s.
  let lowPass : OnePoleTPTLowPass Float := { G := 0.25, s := 0.0, y := 0.0 }
  let (outputs, final) := observe OnePoleTPTLowPass.step (·.y) lowPass [1.0, 1.0, 0.0]
  unless outputs == [0.25, 0.625, 0.5625] && final.s == 0.375 && final.G == 0.25 do
    throw (IO.userError s!"Float TPT one-pole trajectory failed: {outputs}, {final.s}")
  let (wire, _) := observe OnePoleTPTLowPass.step (·.y)
    ({ G := 1.0, s := 5.0, y := 0.0 } : OnePoleTPTLowPass Float) [-3.0, 2.0]
  let (frozen, _) := observe OnePoleTPTLowPass.step (·.y)
    ({ G := 0.0, s := 5.0, y := 0.0 } : OnePoleTPTLowPass Float) [-3.0, 2.0]
  unless wire == [-3.0, 2.0] && frozen == [5.0, 5.0] do
    throw (IO.userError "Float TPT one-pole coefficient endpoints failed")
  let (outputs32, _) := observe OnePoleTPTLowPass.step (·.y)
    ({ G := 0.25, s := 0.0, y := 0.0 } : OnePoleTPTLowPass Float32) [1.0, 1.0, 0.0]
  unless outputs32 == [0.25, 0.625, 0.5625] do
    throw (IO.userError "Float32 TPT one-pole trajectory failed")
  let (highs, _) := observe OnePoleTPTHighPass.step (·.y)
    ({ lowPass, y := 0.0 } : OnePoleTPTHighPass Float) [1.0, 1.0, 0.0]
  unless highs == [0.75, 0.375, -0.5625] do
    throw (IO.userError s!"Float TPT highpass trajectory failed: {highs}")

  -- Tuned SVF sections settle to unit DC gain in the lowpass and zero elsewhere.
  let (_, svf) := observe TPTSVF.step (·.lowPass) (tunedSVF 0.5 1.0) (constant 4000 1.0)
  unless close svf.lowPass 1.0 && close svf.bandPass 0.0 && close svf.highPass 0.0 &&
      close svf.notch 1.0 do
    throw (IO.userError "Float SVF DC responses failed")
  let morphed : MorphingTPTSVF Float :=
    { svf := tunedSVF 0.5 1.0, cosine := 1.0, sine := 0.0, blend := 0.0, output := 0.0 }
  let inputs := [1.0, -2.0, 0.5, 3.0]
  let (morphs, _) := observe MorphingTPTSVF.step (·.output) morphed inputs
  let (lows, _) := observe TPTSVF.step (·.lowPass) (tunedSVF 0.5 1.0) inputs
  unless morphs == lows do
    throw (IO.userError "Float morph at blend 0 differs from the lowpass")

  -- Without feedback the ladder is exactly four cascaded lowpasses.
  let ladder : TPTLadder Float :=
    { k := 0.0, drive := 0.0, stage1 := lowPass, stage2 := lowPass, stage3 := lowPass,
      stage4 := lowPass }
  let (ladderOut, _) := observe TPTLadder.step (·.stage4.y) ladder inputs
  let cascade := fun (xs : List Float) => (observe OnePoleTPTLowPass.step (·.y) lowPass xs).1
  unless ladderOut == cascade (cascade (cascade (cascade inputs))) do
    throw (IO.userError "Float ladder without feedback differs from the cascade")
  let resonant : TPTLadder Float := { ladder with k := 2.0 }
  let (_, settled) := observe TPTLadder.step (·.stage4.y) resonant (constant 4000 1.0)
  unless close settled.stage4.y (1.0 / 3.0) do
    throw (IO.userError s!"Float ladder DC gain 1 / (1 + k) failed: {settled.stage4.y}")

  let crossover : LinkwitzRileyCrossover Float :=
    { first := tunedSVF 0.3 1.8478, lowSecond := tunedSVF 0.3 0.7654,
      highSecond := tunedSVF 0.3 0.7654 }
  let (_, split) := observe LinkwitzRileyCrossover.step (·.lowSecond.lowPass) crossover
    (constant 4000 1.0)
  unless close split.lowSecond.lowPass 1.0 && close split.highSecond.highPass 0.0 do
    throw (IO.userError "Float crossover DC split failed")
  IO.println "Filter native execution: one-pole, SVF, morph, ladder and crossover checks passed."
