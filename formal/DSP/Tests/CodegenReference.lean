import DSP.Filters.Capture
import DSP.Tests.CodegenFixtures
import DSP.Tests.ResponseCases
import DSP.Tests.Splitter
import DSP.Tests.NaiveReuppingLadder
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTBaseWidth
import DSP.Filters.TPTSVF
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder

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

def lowPassFields {R : Type} (f : OnePoleTPTLowPass R) : List R := [f.G, f.s, f.y]

def highPassFields {R : Type} (f : OnePoleTPTHighPass R) : List R := lowPassFields f.lowPass ++ [f.y]

def baseWidthFields {R : Type} (f : TPTBaseWidth R) : List R :=
  highPassFields f.highPass ++ lowPassFields f.lowPass

def svfFields {R : Type} (f : TPTSVF R) : List R :=
  [f.k, f.a1, f.a2, f.a3, f.ic1, f.ic2, f.lowPass, f.bandPass, f.highPass, f.notch, f.allPass]

def svf4Fields {R : Type} (f : TPTSVF4Pole R) : List R := svfFields f.stage1 ++ svfFields f.stage2

def ladderFields {R : Type} (f : TPTLadder R) : List R :=
  [f.k, f.drive] ++ lowPassFields f.stage1 ++ lowPassFields f.stage2 ++
    lowPassFields f.stage3 ++ lowPassFields f.stage4

def morphFields {R : Type} (f : MorphingTPTSVF R) : List R :=
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
  let (state, r) := randomValues r 7 (-10.0) 10.0
  let a1 := 1.0 / (g * (g + k) + 1.0)
  let svf : TPTSVF Float :=
    { k := k, a1 := a1, a2 := g * a1, a3 := g * (g * a1), ic1 := state[0]!
      ic2 := state[1]!, lowPass := state[2]!, bandPass := state[3]!, highPass := state[4]!
      notch := state[5]!, allPass := state[6]! }
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

def bits32 (values : List Float32) : String :=
  " ".intercalate (values.map fun v => toString v.toBits)

def lowPass32 (f : OnePoleTPTLowPass Float) : OnePoleTPTLowPass Float32 :=
  ⟨f.G.toFloat32, f.s.toFloat32, f.y.toFloat32⟩

def svf32 (f : TPTSVF Float) : TPTSVF Float32 :=
  ⟨f.k.toFloat32, f.a1.toFloat32, f.a2.toFloat32, f.a3.toFloat32,
    f.ic1.toFloat32, f.ic2.toFloat32, f.lowPass.toFloat32, f.bandPass.toFloat32,
    f.highPass.toFloat32, f.notch.toFloat32, f.allPass.toFloat32⟩

/-- Execute Float32 arithmetic directly; converting the Float result would
not test the target's rounding at each operation. -/
def emit32 {S : Type} (out : IO.FS.Stream) (kind : String) (fields : S → List Float32)
    (step : S → Sample Float32 → S) (initial : S) (seed : UInt64) : IO Unit := do
  let mut state := initial
  let mut r := seed
  for i in [:64] do
    r := nextRandom r
    let x := if i == 0 then Float32.ofBits 1 else
      (Float.ofNat (r.toNat % 2000001) / 1000.0 - 1000.0).toFloat32
    let next := step state ⟨x⟩
    out.putStrLn s!"{kind}32 {bits32 (fields state)} {x.toBits} {bits32 (fields next)}"
    state := next

/-- Pure response calls have no evolving state. Keep the exact prepared input
bits, so this comparison tests response arithmetic independently of trig libraries. -/
def emitMagnitude (out : IO.FS.Stream) (alpha sinHalf : Float) : IO Unit := do
  -- Arbitrary prepared data tests the compiler, not trigonometric preparation.
  -- Keep fields distinct so the shared-record probe detects incorrect projections.
  let frequency : ResponseFrequency Float :=
    ⟨sinHalf / 2, sinHalf, sinHalf + 0.125, ⟨-sinHalf, 0.75 - sinHalf⟩⟩
  let result := OnePoleZOHLowPass.Snapshot.magnitudeSquared ⟨alpha⟩ frequency
  let probe := DSP.Tests.frequencyProbe ⟨alpha, -alpha⟩ frequency
  out.putStrLn s!"zohResponse {bits [alpha, frequency.cyclesPerSample, frequency.sinHalf,
    frequency.cosHalf, frequency.zInverse.real, frequency.zInverse.imag, result, probe]}"

def emitMagnitude32 (out : IO.FS.Stream) (alpha sinHalf : Float32) : IO Unit := do
  let frequency : ResponseFrequency Float32 :=
    ⟨sinHalf / 2, sinHalf, sinHalf + 0.125, ⟨-sinHalf, 0.75 - sinHalf⟩⟩
  let result := OnePoleZOHLowPass.Snapshot.magnitudeSquared ⟨alpha⟩ frequency
  let probe := DSP.Tests.frequencyProbe ⟨alpha, -alpha⟩ frequency
  out.putStrLn s!"zohResponse32 {bits32 [alpha, frequency.cyclesPerSample, frequency.sinHalf,
    frequency.cosHalf, frequency.zInverse.real, frequency.zInverse.imag, result, probe]}"

def emitMagnitudeCases (out : IO.FS.Stream) : IO Unit := do
  -- Adjacent cases deliberately mix the zero-alpha branch with positive lanes.
  -- Tiny positive coefficients include subnormal squares in binary32.
  let alphas : List Float := [0.0, -0.0, 0.5, 1.0, Float.ofBits 0x3fefffffffffffff,
    0.000001, 1e-12, 1e-150]
  let alphas32 : List Float32 := [0.0, -0.0, 0.5, 1.0, Float32.ofBits 0x3f7fffff,
    0.000001, 1e-12, 1e-20]
  for sinHalf in ([0.0, -0.0, Float.ofBits 1, 1e-150, 0.000001, 0.5, 1.0, -0.5, -1.0] : List Float) do
    for alpha in alphas do emitMagnitude out alpha sinHalf
  for sinHalf in ([0.0, -0.0, Float32.ofBits 1, 1e-20, 0.000001, 0.5, 1.0, -0.5, -1.0] : List Float32) do
    for alpha in alphas32 do emitMagnitude32 out alpha sinHalf
  let mut random : UInt64 := 0xfedcba987654321
  for _ in [:2048] do
    let (alpha, r) := uniform random 0.0 1.0
    let (sinHalf, r) := uniform r (-1.0) 1.0
    random := r
    emitMagnitude out alpha sinHalf
    emitMagnitude32 out alpha.toFloat32 sinHalf.toFloat32

/-- Each line is one snapshot cohort and one prepared frequency, followed by
all fourteen TPT response outputs. Float32 is executed independently. -/
def emitAllResponses (out : IO.FS.Stream) (v : Array Float) (sinHalf cosHalf : Float) : IO Unit := do
  let f : ResponseFrequency Float := ⟨sinHalf / 2, sinHalf, cosHalf, ⟨-sinHalf, cosHalf⟩⟩
  let values := DSP.Tests.responseValues v[0]! v[1]! v[2]! v[3]! v[4]! v[5]! v[6]! v[7]! v[8]! v[9]! f
  out.putStrLn s!"allResponses {bits (v.toList ++ [f.cyclesPerSample, f.sinHalf, f.cosHalf, f.zInverse.real, f.zInverse.imag] ++ values)}"
  let w := v.map Float.toFloat32
  let f32 : ResponseFrequency Float32 := ⟨f.cyclesPerSample.toFloat32, sinHalf.toFloat32,
    cosHalf.toFloat32, ⟨f.zInverse.real.toFloat32, f.zInverse.imag.toFloat32⟩⟩
  let values32 := DSP.Tests.responseValues w[0]! w[1]! w[2]! w[3]! w[4]! w[5]! w[6]! w[7]! w[8]! w[9]! f32
  out.putStrLn s!"allResponses32 {bits32 (w.toList ++ [f32.cyclesPerSample, f32.sinHalf, f32.cosHalf,
    f32.zInverse.real, f32.zInverse.imag] ++ values32)}"

def emitAllResponseCases (out : IO.FS.Stream) : IO Unit := do
  for g in ([0.0, 1e-6, 0.1, 1.0, 100.0] : List Float) do
    for pair in ([(0.0,1.0),(1.0,0.0),(0.5,0.8660254037844386)] : List (Float × Float)) do
      emitAllResponses out #[g, 0.7, 0.3, 1.5, 0.5, 1.0, 1.5, 1.0, -0.3, 0.7] pair.1 pair.2
  let mut random : UInt64 := 0xabcdef012345
  for _ in [:512] do
    let (v, r) := randomValues random 10 0.01 2.0
    let (s, r) := uniform r 0.0 1.0
    let (c, r) := uniform r 0.0 1.0
    random := r
    emitAllResponses out v s c

def svfSnapshotFields {R : Type} (s : TPTSVF.Snapshot R) : List R := [s.g,s.k]
def morphSnapshotFields {R : Type} (s : MorphingTPTSVF.Snapshot R) : List R :=
  svfSnapshotFields s.svf ++ [s.low,s.band,s.high]
def fourSnapshotFields {R : Type} (s : TPTSVF4Pole.Snapshot R) : List R :=
  svfSnapshotFields s.stage1 ++ svfSnapshotFields s.stage2

/-- Independent native transcendental reference for coefficient tuning. Generated
C++ deliberately uses the Math4096 approximation instead. -/
def tuningRows {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 8]
    [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R] [ExpDecay R]
    (fc1 fc2 q1 q2 blend feedback seed : R) : List (String × List R × List R × List R) :=
  let zoh : OnePoleZOHLowPass R := ⟨0,seed⟩
  let lp : OnePoleTPTLowPass R := ⟨0,seed,seed+1⟩
  let hp : OnePoleTPTHighPass R := ⟨lp,seed-1⟩
  let svf : TPTSVF R := ⟨0,0,0,0,seed,seed+1,seed-1,seed+2,seed-2,seed+8,seed+1⟩
  let second : TPTSVF R := ⟨0,0,0,0,seed-8,seed+2,seed+1,seed,seed-1,seed-2,seed-8⟩
  let base : TPTBaseWidth R := ⟨hp,⟨0,seed+2,seed-2⟩⟩
  let morph : MorphingTPTSVF R := ⟨svf,0,0,0,seed+8⟩
  let four : TPTSVF4Pole R := ⟨svf,second⟩
  let ladder : TPTLadder R := ⟨0,seed,lp,⟨0,seed+2,seed-2⟩,lp,⟨0,seed+8,seed-8⟩⟩
  let z := OnePoleZOHLowPass.computeCoefficients ⟨fc1⟩ zoh
  let zfields := fun f : OnePoleZOHLowPass R => [f.alpha,f.y]
  let rows := [
    ("OnePoleZOHLowPass", [fc1], zfields zoh, zfields z ++ [(OnePoleZOHLowPass.prepareCoefficients ⟨fc1⟩ zoh).2.alpha]),
    ("OnePoleTPTLowPass", [fc1], lowPassFields lp,
      lowPassFields (OnePoleTPTLowPass.computeCoefficients ⟨fc1⟩ lp) ++ ([(OnePoleTPTLowPass.prepareCoefficients ⟨fc1⟩ lp).2.g])),
    ("OnePoleTPTHighPass", [fc1], highPassFields hp,
      highPassFields (OnePoleTPTHighPass.computeCoefficients ⟨fc1⟩ hp) ++ ([(OnePoleTPTHighPass.prepareCoefficients ⟨fc1⟩ hp).2.g])),
    ("TPTBaseWidth", [fc1,fc2], baseWidthFields base,
      baseWidthFields (TPTBaseWidth.computeCoefficients ⟨⟨fc1⟩,⟨fc2⟩⟩ base) ++ (let s := (TPTBaseWidth.prepareCoefficients ⟨⟨fc1⟩,⟨fc2⟩⟩ base).2; [s.highPass.g,s.lowPass.g])),
    ("TPTSVF", [fc1,q1], svfFields svf,
      svfFields (TPTSVF.computeCoefficients ⟨fc1,q1⟩ svf) ++ (svfSnapshotFields (TPTSVF.prepareCoefficients ⟨fc1,q1⟩ svf).2)),
    ("MorphingTPTSVF", [fc1,q1,blend], morphFields morph,
      morphFields (MorphingTPTSVF.computeCoefficients ⟨⟨fc1,q1⟩,blend⟩ morph) ++ (morphSnapshotFields (MorphingTPTSVF.prepareCoefficients ⟨⟨fc1,q1⟩,blend⟩ morph).2)),
    ("TPTLadder", [fc1,feedback], ladderFields ladder,
      ladderFields (TPTLadder.computeCoefficients ⟨fc1,feedback⟩ ladder) ++
        (let s := (TPTLadder.prepareCoefficients ⟨fc1,feedback⟩ ladder).2; [s.g1,s.g2,s.g3,s.g4,s.k])),
    ("LinkwitzRileySplitter4Band", [fc1,fc2,fc1],
      DSP.Tests.Splitter.fields svfFields (DSP.Tests.Splitter.seeded 4 seed),
      DSP.Tests.Splitter.fields svfFields (LinkwitzRileySplitter.computeCoefficients
        (DSP.Tests.Splitter.spec4 fc1 fc2 fc1) (DSP.Tests.Splitter.seeded 4 seed)))]
  rows ++ ["TPTSVF4PoleLowPass", "TPTSVF4PoleBandPass", "TPTSVF4PoleHighPass"].map (fun name =>
    (name, [fc1,q1,fc2,q2], svf4Fields four,
      svf4Fields (TPTSVF4Pole.computeCoefficients ⟨⟨fc1,q1⟩,⟨fc2,q2⟩⟩ four) ++ (fourSnapshotFields (TPTSVF4Pole.prepareCoefficients ⟨⟨fc1,q1⟩,⟨fc2,q2⟩⟩ four).2)))

def emitTuningCases (out : IO.FS.Stream) : IO Unit := do
  let mut random : UInt64 := 0x739dac
  for i in [:256] do
    let (u,r) := uniform random 0.0001 0.49
    let (v,r) := uniform r 0.0001 0.49
    let (q1,r) := uniform r 0.25 20
    let (q2,r) := uniform r 0.25 20
    let (b,r) := uniform r 0 1
    let (k,r) := uniform r 0 3.95
    random := r
    let fc1 := if i < 6 then (#[0.0001,0.001,0.01,0.1,0.249,0.49] : Array Float)[i]! else u
    let fc2 := v
    let blend := if i < 3 then (#[0,0.5,1] : Array Float)[i]! else b
    let seed := if i % 7 == 0 then Float.ofBits 0x7ff8000000000042
      else if i % 7 == 1 then -0.0 else Float.ofNat i
    for (name,spec,old,next) in tuningRows fc1 fc2 q1 q2 blend k seed do
      out.putStrLn s!"tuning64 {name} {bits (spec ++ old ++ next)}"
    for (name,spec,old,next) in tuningRows fc1.toFloat32 fc2.toFloat32 q1.toFloat32 q2.toFloat32
        blend.toFloat32 k.toFloat32 seed.toFloat32 do
      out.putStrLn s!"tuning32 {name} {bits32 (spec ++ old ++ next)}"

def main : IO Unit := do
  let out ← IO.getStdout
  emitTuningCases out
  emitMagnitudeCases out
  emitAllResponseCases out
  let coefficients : Array Float := #[0.0, 1.0, 0.5, 0.999, Float.ofBits 0x3fefffffffffffff,
    Float.ofBits 1]
  let seeds : Array Float := #[0.0, -0.0, 1.0, -1.0, 1000.0, Float.ofBits 1,
    Float.ofBits 0x0010000000000000]
  let mut random : UInt64 := 0x123456789abcdef
  for i in [:32] do
    let cutoff := (#[0.000001, 0.003, 0.03, 0.125, 0.1959, 0.25, 0.5, 0.6] : Array Float)[i % 8]!
    let gain := (#[0.001, 0.5, 2.0, 10.0] : Array Float)[i / 8]!
    let resonance := Float.ofNat (i % 5) / 4
    let seed := Float.ofNat i / 40 - 0.4
    let state := NaiveReuppingLadder.computeCoefficients ⟨cutoff, resonance, gain⟩
      (DSP.Tests.NaiveReuppingLadder.seeded seed)
    let state32 := NaiveReuppingLadder.computeCoefficients
      ⟨cutoff.toFloat32, resonance.toFloat32, gain.toFloat32⟩
      (DSP.Tests.NaiveReuppingLadder.seeded seed.toFloat32)
    random ← emit out "naive" (fun s => bits (DSP.Tests.NaiveReuppingLadder.fields s))
      NaiveReuppingLadder.step state random 64
    emit32 out "naive" DSP.Tests.NaiveReuppingLadder.fields NaiveReuppingLadder.step state32 random
    random ← emit out "envelope" (fun s => bits (DSP.Tests.NaiveReuppingLadder.envelopeFields s))
      BandPassedEnvelopeFollower.step state.inputEnvelope random 64
    emit32 out "envelope" DSP.Tests.NaiveReuppingLadder.envelopeFields
      BandPassedEnvelopeFollower.step state32.inputEnvelope random
    let saturator : NormalizedTanh Float := ⟨state.saturation, seed⟩
    random ← emit out "normalizedTanh" (fun s => bits [s.coefficients.gain,s.coefficients.tanhGain,s.output])
      NormalizedTanh.step saturator random 64
    emit32 out "normalizedTanh" (fun s => [s.coefficients.gain,s.coefficients.tanhGain,s.output])
      NormalizedTanh.step ⟨state32.saturation, seed.toFloat32⟩ random
  for alpha in coefficients do
    for initial in seeds do
      emit32 out "zoh" (fun f : OnePoleZOHLowPass Float32 => [f.alpha, f.y])
        OnePoleZOHLowPass.step ⟨alpha.toFloat32, initial.toFloat32⟩ random
      random ← emit out "zoh" (fun f : OnePoleZOHLowPass Float => bits [f.alpha, f.y])
        OnePoleZOHLowPass.stepFloat { alpha, y := initial } random 128
  for G in coefficients do
    for initial in seeds do
      emit32 out "tpt" lowPassFields OnePoleTPTLowPass.step
        ⟨G.toFloat32, initial.toFloat32, (-initial).toFloat32⟩ random
      random ← emit out "tpt" (fun f => bits (lowPassFields f)) OnePoleTPTLowPass.stepFloat
        { G, s := initial, y := -initial } random 128
  for _ in [:32] do
    let (lowPass, r) := randomLowPass random
    let (y, r) := uniform r (-10.0) 10.0
    emit32 out "tptHigh" highPassFields OnePoleTPTHighPass.step
      ⟨lowPass32 lowPass, y.toFloat32⟩ r
    random ← emit out "tptHigh" (fun f => bits (highPassFields f)) OnePoleTPTHighPass.stepFloat
      { lowPass, y } r 64
    let (highLowPass, r) := randomLowPass random
    let (y, r) := uniform r (-10.0) 10.0
    let (lowPass, r) := randomLowPass r
    emit32 out "baseWidth" baseWidthFields TPTBaseWidth.step
      ⟨⟨lowPass32 highLowPass, y.toFloat32⟩, lowPass32 lowPass⟩ r
    random ← emit out "baseWidth" (fun f => bits (baseWidthFields f)) TPTBaseWidth.stepFloat
      { highPass := { lowPass := highLowPass, y }, lowPass } r 64
    let (svf, r) := randomSVF random
    emit32 out "svf" svfFields TPTSVF.step (svf32 svf) r
    random ← emit out "svf" (fun f => bits (svfFields f)) TPTSVF.stepFloat svf r 64
    let (svf, r) := randomSVF random
    let (angle, r) := uniform r 0.0 1.5707963267948966
    let (output, r) := uniform r (-10.0) 10.0
    let (blend, r) := uniform r 0.0 1.0
    let blend := if (r >>> 40) % 4 == 0 then 0.5 else blend
    let morph : MorphingTPTSVF Float :=
      { svf, cosine := angle.cos, sine := angle.sin, blend, output }
    emit32 out "morph" morphFields MorphingTPTSVF.step
      ⟨svf32 svf, angle.cos.toFloat32, angle.sin.toFloat32, blend.toFloat32, output.toFloat32⟩ r
    random ← emit out "morph" (fun f => bits (morphFields f)) MorphingTPTSVF.stepFloat morph r 64
    for (kind, step) in [("svf4High", TPTSVF4Pole.stepHighPassFloat),
        ("svf4Low", TPTSVF4Pole.stepLowPassFloat), ("svf4Band", TPTSVF4Pole.stepBandPassFloat)] do
      let (stage1, r) := randomSVF random
      let (stage2, r) := randomSVF r
      let step32 := if kind == "svf4High" then TPTSVF4Pole.stepHighPass (R := Float32)
        else if kind == "svf4Low" then TPTSVF4Pole.stepLowPass else TPTSVF4Pole.stepBandPass
      emit32 out kind svf4Fields step32 ⟨svf32 stage1, svf32 stage2⟩ r
      random ← emit out kind (fun f => bits (svf4Fields f)) step { stage1, stage2 } r 64
    let (k, r) := uniform random 0.0 3.9
    let (drive, r) := uniform r (-10.0) 10.0
    let (stage1, r) := randomLowPass r
    let (stage2, r) := randomLowPass r
    let (stage3, r) := randomLowPass r
    let (stage4, r) := randomLowPass r
    emit32 out "ladder" ladderFields TPTLadder.step
      ⟨k.toFloat32, drive.toFloat32, lowPass32 stage1, lowPass32 stage2,
        lowPass32 stage3, lowPass32 stage4⟩ r
    random ← emit out "ladder" (fun f => bits (ladderFields f)) TPTLadder.stepFloat
      { k, drive, stage1, stage2, stage3, stage4 } r 64
  for j in [:16] do
    let seed := Float.ofNat j / 8 - 1
    let spec := DSP.Tests.Splitter.spec4 (0.12 + Float.ofNat j / 200) 0.023 0.37
    let initial := LinkwitzRileySplitter.computeCoefficients spec (DSP.Tests.Splitter.seeded 4 seed)
    random ← emit out "splitter" (fun f => bits (DSP.Tests.Splitter.fields svfFields f))
      LinkwitzRileySplitter.step4Band initial random 64
    let spec32 := DSP.Tests.Splitter.spec4 spec.tree.value.toFloat32
      spec.tree.low.value.toFloat32 spec.tree.high.value.toFloat32
    let initial32 := LinkwitzRileySplitter.computeCoefficients spec32
      (DSP.Tests.Splitter.seeded 4 seed.toFloat32)
    emit32 out "splitter" (DSP.Tests.Splitter.fields svfFields) LinkwitzRileySplitter.step4Band initial32 random
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

  let model32 : DSP.StateSpace 2 Float32 :=
    { A := pair (pair 0.75 (-0.125)) (pair 0.25 0.5)
      B := pair 0.5 (-0.25), C := pair 0.75 (-0.5), D := 0.125 }
  for seed in [0.0, -0.0, 1.0, -1.0] do
    let mut state : DSP.Coupled.State Float32 := { model := model32, q := pair seed (-seed) }
    for i in [:64] do
      let x := (Float.ofNat i / 7.0).toFloat32
      let (next, output) := DSP.Coupled.processor state { x }
      let fields := [next.model.A 0 0, next.model.A 0 1, next.model.A 1 0, next.model.A 1 1,
        next.model.B 0, next.model.B 1, next.model.C 0, next.model.C 1, next.model.D,
        next.q 0, next.q 1, output.y]
      out.putStrLn s!"coupled32 {(state.q 0).toBits} {(state.q 1).toBits} {x.toBits} {bits32 fields}"
      state := next

  for seed in [0.0, -0.0, 1.0, -2.0] do
    let mut state : DSP.Tests.ArithmeticState Float := ⟨seed, seed + 0.125⟩
    let mut state32 : DSP.Tests.ArithmeticState Float32 := ⟨seed.toFloat32, (seed + 0.125).toFloat32⟩
    for i in [:64] do
      let x := if i % 4 == 0 then state.left else if i % 4 == 1 then 0.0
        else if i % 4 == 2 then -1.0 else Float.ofBits 0x7ff8000000000000
      let x32 := if i % 4 == 3 then Float32.ofBits 0x7fc00000 else x.toFloat32
      let (next, output) := DSP.Tests.arithmeticProcessor state ⟨x⟩
      let (next32, output32) := DSP.Tests.arithmeticProcessor state32 ⟨x32⟩
      out.putStrLn s!"arithmetic {state.left.toBits} {state.right.toBits} {x.toBits} {bits [next.left, next.right, output.x]}"
      out.putStrLn s!"arithmetic32 {state32.left.toBits} {state32.right.toBits} {x32.toBits} {bits32 [next32.left, next32.right, output32.x]}"
      state := next
      state32 := next32

  -- Known IEEE results cover cancellation, intermediate overflow, subnormals,
  -- signed zero and infinity. Separate multiply/add fails the first two cases.
  let fusedCases : List (UInt64 × UInt64 × UInt64 × UInt64) := [
    (0x3ff0000000000001, 0x3feffffffffffffe, 0xbff0000000000000, 0xb970000000000000),
    (0x7fefffffffffffff, 0x4000000000000000, 0xffefffffffffffff, 0x7fefffffffffffff),
    (0x0010000000000000, 0x3fe0000000000000, 0x8000000000000001, 0x0007ffffffffffff),
    (0x8000000000000000, 0x3ff0000000000000, 0x8000000000000000, 0x8000000000000000),
    (0x0000000000000000, 0x3ff0000000000000, 0x8000000000000000, 0x0000000000000000),
    (0x7ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x7ff0000000000000)]
  for (a, b, c, expected) in fusedCases do
    let (state, output) := DSP.Tests.fmaProcessor ⟨Float.ofBits a, Float.ofBits c⟩ ⟨Float.ofBits b⟩
    unless output.x.toBits == expected do
      throw (IO.userError s!"Float FMA rounding failed: {a} {b} {c}; got {output.x.toBits}, expected {expected}")
    out.putStrLn s!"fma {a} {c} {b} {bits [state.left, state.right, output.x]}"
  let fusedCases32 : List (UInt32 × UInt32 × UInt32 × UInt32) := [
    (0x3f800001, 0x3f7ffffe, 0xbf800000, 0xa8800000),
    (0x7f7fffff, 0x40000000, 0xff7fffff, 0x7f7fffff),
    (0x00800000, 0x3f000000, 0x80000001, 0x003fffff),
    (0x80000000, 0x3f800000, 0x80000000, 0x80000000),
    (0x00000000, 0x3f800000, 0x80000000, 0x00000000),
    (0x7f800000, 0x3f800000, 0x3f800000, 0x7f800000)]
  for (a, b, c, expected) in fusedCases32 do
    let (state, output) := DSP.Tests.fmaProcessor ⟨Float32.ofBits a, Float32.ofBits c⟩ ⟨Float32.ofBits b⟩
    unless output.x.toBits == expected do
      throw (IO.userError s!"Float32 FMA rounding failed: {a} {b} {c}; got {output.x.toBits}, expected {expected}")
    out.putStrLn s!"fma32 {a} {c} {b} {bits32 [state.left, state.right, output.x]}"
