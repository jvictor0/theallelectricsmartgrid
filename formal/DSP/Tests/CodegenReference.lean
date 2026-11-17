import DSP.Tests.CodegenFixtures
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTBaseWidth
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder
import DSP.Filters.LinkwitzRileyCrossover

/-! Generate inputs and expected raw IEEE bits by executing the Lean functions.
The C++ differential harness consumes the same inputs and compares every field.
State-only filters are flattened in field declaration order, which is also
the generated C++ layout. -/

open DSP

def bits (values : List Float) : String := " ".intercalate (values.map fun v => toString v.toBits)

def nextRandom (r : UInt64) : UInt64 := r * 6364136223846793005 + 1442695040888963407

/-- A value in [lo, hi] and the advanced generator. -/
def uniform (r : UInt64) (lo hi : Float) : Float × UInt64 :=
  let r := nextRandom r
  (lo + (hi - lo) * Float.ofNat ((r >>> 11).toNat % 1000001) / 1000000.0, r)

def lowPassFields (f : OnePoleTPTLowPass Float) : List Float := [f.G, f.s, f.y]

def highPassFields (f : OnePoleTPTHighPass Float) : List Float := lowPassFields f.lowPass ++ [f.y]

def baseWidthFields (f : TPTBaseWidth Float) : List Float :=
  highPassFields f.highPass ++ lowPassFields f.lowPass

def svfFields (f : TPTSVF Float) : List Float :=
  [f.k, f.a1, f.a2, f.a3, f.ic1, f.ic2, f.lowPass, f.bandPass, f.highPass, f.notch]

def svf4Fields (f : TPTSVF4Pole Float) : List Float := svfFields f.stage1 ++ svfFields f.stage2

def ladderFields (f : TPTLadder Float) : List Float :=
  [f.k, f.drive] ++ lowPassFields f.stage1 ++ lowPassFields f.stage2 ++
    lowPassFields f.stage3 ++ lowPassFields f.stage4

def crossoverFields (f : LinkwitzRileyCrossover Float) : List Float :=
  svfFields f.first ++ svfFields f.lowSecond ++ svfFields f.highSecond

def morphFields (f : MorphingTPTSVF Float) : List Float :=
  svfFields f.svf ++ [f.cosine, f.sine, f.blend, f.output]

/-- Random coefficients and states. Gains are solved as the app does, so long
trajectories stay finite; states and stored outputs start anywhere. -/
def randomValues (r : UInt64) (count : Nat) (lo hi : Float) : Array Float × UInt64 := Id.run do
  let mut r := r
  let mut values := #[]
  for _ in [:count] do
    let (value, next) := uniform r lo hi
    values := values.push value
    r := next
  return (values, r)

def randomLowPass (r : UInt64) : OnePoleTPTLowPass Float × UInt64 :=
  let (g, r) := uniform r 0.0 3.0
  let (state, r) := randomValues r 2 (-10.0) 10.0
  ({ G := g / (1.0 + g), s := state[0]!, y := state[1]! }, r)

def randomSVF (r : UInt64) : TPTSVF Float × UInt64 :=
  let (g, r) := uniform r 0.0 3.0
  let (k, r) := uniform r 0.05 2.0
  let (state, r) := randomValues r 6 (-10.0) 10.0
  let a1 := 1.0 / (g * (g + k) + 1.0)
  let svf : TPTSVF Float :=
    { k := k, a1 := a1, a2 := g * a1, a3 := g * (g * a1), ic1 := state[0]!
      ic2 := state[1]!, lowPass := state[2]!, bandPass := state[3]!, highPass := state[4]!
      notch := state[5]! }
  (svf, r)

def emit {S : Type} (out : IO.FS.Stream) (kind : String) (line : S → String)
    (step : S → Sample Float → S) (initial : S) (r : UInt64) (steps : Nat) : IO UInt64 := do
  let mut state := initial
  let mut r := r
  for i in [:steps] do
    r := nextRandom r
    let x := if i == 0 then Float.ofBits 1 else
      Float.ofNat (r.toNat % 2000001) / 1000.0 - 1000.0
    let next := step state ⟨x⟩
    out.putStrLn s!"{kind} {line state} {x.toBits} {line next}"
    state := next
  return r

def main : IO Unit := do
  let out ← IO.getStdout
  let coefficients : Array Float := #[0.0, 1.0, 0.5, 0.999, Float.ofBits 0x3fefffffffffffff,
    Float.ofBits 1]
  let seeds : Array Float := #[0.0, -0.0, 1.0, -1.0, 1000.0, Float.ofBits 1,
    Float.ofBits 0x0010000000000000]
  let mut random : UInt64 := 0x123456789abcdef
  for alpha in coefficients do
    for initial in seeds do
      random ← emit out "zoh" (fun f : OnePoleZOHLowPass Float => bits [f.alpha, f.y])
        OnePoleZOHLowPass.stepFloat { alpha, y := initial } random 128
  for G in coefficients do
    for initial in seeds do
      random ← emit out "tpt" (fun f => bits (lowPassFields f)) OnePoleTPTLowPass.stepFloat
        { G, s := initial, y := -initial } random 128
  for _ in [:32] do
    let (lowPass, r) := randomLowPass random
    let (y, r) := uniform r (-10.0) 10.0
    random ← emit out "tptHigh" (fun f => bits (highPassFields f)) OnePoleTPTHighPass.stepFloat
      { lowPass, y } r 64
    let (highLowPass, r) := randomLowPass random
    let (y, r) := uniform r (-10.0) 10.0
    let (lowPass, r) := randomLowPass r
    random ← emit out "baseWidth" (fun f => bits (baseWidthFields f)) TPTBaseWidth.stepFloat
      { highPass := { lowPass := highLowPass, y }, lowPass } r 64
    let (svf, r) := randomSVF random
    random ← emit out "svf" (fun f => bits (svfFields f)) TPTSVF.stepFloat svf r 64
    let (svf, r) := randomSVF random
    let (angle, r) := uniform r 0.0 1.5707963267948966
    let (output, r) := uniform r (-10.0) 10.0
    let (blend, r) := uniform r 0.0 1.0
    let blend := if (r >>> 40) % 4 == 0 then 0.5 else blend
    let morph : MorphingTPTSVF Float :=
      { svf, cosine := angle.cos, sine := angle.sin, blend, output }
    random ← emit out "morph" (fun f => bits (morphFields f)) MorphingTPTSVF.stepFloat morph r 64
    for (kind, step) in [("svf4High", TPTSVF4Pole.stepHighPassFloat),
        ("svf4Low", TPTSVF4Pole.stepLowPassFloat), ("svf4Band", TPTSVF4Pole.stepBandPassFloat)] do
      let (stage1, r) := randomSVF random
      let (stage2, r) := randomSVF r
      random ← emit out kind (fun f => bits (svf4Fields f)) step { stage1, stage2 } r 64
    let (k, r) := uniform random 0.0 3.9
    let (drive, r) := uniform r (-10.0) 10.0
    let (stage1, r) := randomLowPass r
    let (stage2, r) := randomLowPass r
    let (stage3, r) := randomLowPass r
    let (stage4, r) := randomLowPass r
    random ← emit out "ladder" (fun f => bits (ladderFields f)) TPTLadder.stepFloat
      { k, drive, stage1, stage2, stage3, stage4 } r 64
    let (first, r) := randomSVF random
    let (lowSecond, r) := randomSVF r
    let (highSecond, r) := randomSVF r
    random ← emit out "crossover" (fun f => bits (crossoverFields f))
      LinkwitzRileyCrossover.stepFloat { first, lowSecond, highSecond } r 64
  for i in [:1024] do
    random := nextRandom random
    let left := Float.ofNat (random.toNat % 100001) / 100.0 - 500.0
    let right := Float.ofNat i / 7.0
    let x := left / 3.0
    let reset := i % 2 == 0
    let (next, output) := DSP.Tests.swapProcessor { left, right } { value := x, reset }
    out.putStrLn s!"swap {left.toBits} {right.toBits} {x.toBits} {if reset then 1 else 0} {next.left.toBits} {next.right.toBits} {output.oldLeft.toBits} {output.sum.toBits}"
    let gain := (left / 7.0).toFloat32
    let input := if i % 5 == 0 then gain else (x + 10.0).toFloat32
    let (next32, output32) := DSP.Tests.gainProcessor { gain } { x := input }
    out.putStrLn s!"gain {gain.toBits} {input.toBits} {next32.gain.toBits} {output32.y.toBits} {if output32.same then 1 else 0}"

  let pair {R : Type} (a b : R) : Fin 2 → R := Fin.cases a (fun _ => b)
  let model : DSP.StateSpace 2 Float :=
    { A := pair (pair 0.75 (-0.125)) (pair 0.25 0.5)
      B := pair 0.5 (-0.25), C := pair 0.75 (-0.5), D := 0.125 }
  let mut coupled : DSP.Coupled.State Float := { model, q := pair 1.0 (-0.0) }
  for i in [:1024] do
    random := nextRandom random
    let x := if i == 0 then Float.ofBits 1 else Float.ofNat (random.toNat % 100001) / 100.0 - 500.0
    let (next, output) := DSP.Coupled.processorFloat coupled { x }
    out.putStrLn s!"coupled {coupled.q 0 |>.toBits} {coupled.q 1 |>.toBits} {x.toBits} {next.model.A 0 0 |>.toBits} {next.model.A 0 1 |>.toBits} {next.model.A 1 0 |>.toBits} {next.model.A 1 1 |>.toBits} {next.model.B 0 |>.toBits} {next.model.B 1 |>.toBits} {next.model.C 0 |>.toBits} {next.model.C 1 |>.toBits} {next.model.D.toBits} {next.q 0 |>.toBits} {next.q 1 |>.toBits} {output.y.toBits}"
    coupled := next
