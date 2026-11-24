import DSP.Filters.DelayKernels
import DSP.DelayBuffer.Basic
import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.Tests.DelayCodegen
open DelayKernels

def fields {R : Type} (c : Coefficients R) : List R :=
  [c.delay,c.a,c.b,c.r,c.delayGain,c.g,c.k,c.h,c.gain,c.sign,c.bypass]
def memory {R : Type} (s : TunedDecayComb.Branch R) : List R := [s.allpass,s.ic1,s.ic2,s.y]

def allPassRow {R : Type} [OfScientific R] [Mul R] [Sub R] [Neg R] [FMA R]
    (sign x delayed seed : R) : List R × List R :=
  let ap : AllPassState R := ⟨sign * 0.7,seed,seed-0.3⟩
  let apn := allPassStep ap ⟨x,delayed⟩
  ([ap.gain,ap.y,ap.written,x,delayed], [apn.gain,apn.y,apn.written])

def combRows (f sign duration damp rate x delayed seed bypass : Float) :
    List (String × List Float × List Float) :=
  let c : Coefficients Float := ⟨7,0.6,0.8,0.97,0.8,0.3,1.2,1 / (1 + 1.2 * 0.3 + 0.3 * 0.3),0.8,sign,bypass⟩
  let s : CombState Float := ⟨c, ⟨seed,seed+0.2,seed-0.3,seed+0.7⟩⟩
  let next := combStep s ⟨x,delayed⟩
  let tuned := prepare c ⟨f,sign,duration,damp,rate⟩
  let q := ResponseFrequency.fromHalfAngle f (Trig.sin2pi (f / 2)) (Trig.cos2pi (f / 2))
  let c1 := {c with a := -0.2, b := SquareRoot.sqrt 0.96, delay := 3, sign := -sign, bypass := 0}
  let response := (DelayKernels.response ⟨c,c1,0.3⟩ q).2
  [("comb", fields c ++ memory s.memory ++ [x,delayed], memory next.memory),
    ("tuning", [f,sign,duration,damp,rate], fields tuned),
    ("response", fields c ++ fields c1 ++ [0.3,f,q.sinHalf,q.cosHalf,q.zInverse.real,q.zInverse.imag],
      [response.numerator.real,response.numerator.imag,response.denominator.real,response.denominator.imag])]

def ring (out : IO.FS.Stream) (capacity : Nat) (hc : 0 < capacity) : IO Unit := do
  let mut s : DelayBuffer capacity Float := DelayBuffer.empty hc
  for n in [:capacity * 3 + 17] do
    let age := (n * 13 + 7) % capacity
    let a : Fin capacity := ⟨age, Nat.mod_lt _ hc⟩
    let x := n.toFloat * 0.25 - 17.0
    let read := s.read a
    let next := s.push x
    out.putStrLn s!"ring 64 {capacity} {age} {x.toBits} {read.toBits} {next.head.val}"
    s := next

def addresses (out : IO.FS.Stream) (capacity : Nat) : IO Unit := do
  for head in List.finRange capacity do
    let s : DelayBuffer capacity Float := ⟨Vector.ofFn (fun i => i.val.toFloat), head⟩
    for age in List.finRange capacity do
      let read := s.read age
      let next := s.push 0.0
      out.putStrLn s!"address 64 {capacity} {head.val} {age.val} {read.toBits} {next.head.val}"

private def reverbSpec (n : Nat) : SquiggleBoyQuadReverb.LogicalSpec Float where
  delaySamples ch :=
    let offset := ((n / 13 + ch.val * 3) % 263).toFloat / 4
    let capacity := squiggleBoyReverbDelayCapacity.toFloat
    match (n + ch.val * 17) % 251 with
    | 0 => 1
    | 1 => capacity - 1 - offset
    | 2 => 240 + offset
    | 3 => 4096 + offset
    | 4 => 1.25
    | 5 => capacity
    | _ => 3 + offset
  feedback ch := (5 + ((n / 411 + ch.val * 2) % 18).toFloat) / 32
  dampingBase ch := (3 + ((n / 17 + ch.val * 3) % 13).toFloat) / 128
  dampingWidth ch := (8 + ((n / 73 + ch.val * 7) % 23).toFloat) / 8
  saturatorGain ch := (4 + ((n / 109 + ch.val * 5) % 21).toFloat) / 8

private def reverbAudio (n : Nat) (ch : Fin 4) : Float :=
  if n == 0 then (ch.val + 1).toFloat / 4
  else if n % 901 < 631 then (((n * (15 + 2 * ch.val)) % 31).toFloat - 15) / 128
  else 0

private def reverbRows {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3]
    [OfNat R 4] [OfNat R 5] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R]
    [FMA R] [Tanh R] [IfLess R] [Trig R] [DelayMath R] [DelayBuffer.SampleIndex R]
    [WanderingRandomLFO.SampleValue R]
    (out : IO.FS.Stream) (precision : Nat) (cast : Float → R)
    (bits : R → String) : IO Unit := do
  let specAt := fun n =>
    let spec := reverbSpec n
    ({ delaySamples := cast ∘ spec.delaySamples, feedback := cast ∘ spec.feedback
       dampingBase := cast ∘ spec.dampingBase, dampingWidth := cast ∘ spec.dampingWidth
       saturatorGain := cast ∘ spec.saturatorGain } :
      SquiggleBoyQuadReverb.LogicalSpec R)
  let mut state := SquiggleBoyQuadReverb.init (specAt 0)
  for n in [:24064] do
    let spec := specAt n
    let audio := cast ∘ reverbAudio n
    state := (state.computeCoefficients spec).step ⟨audio⟩
    let fields := [spec.delaySamples, spec.feedback, spec.dampingBase, spec.dampingWidth,
      spec.saturatorGain, audio, state.y]
    let values := fields.flatMap fun field => (List.finRange 4).map (bits ∘ field)
    out.putStrLn s!"reverb {precision} {String.intercalate " " values}"

private def splineRows {R : Type} [WanderingRandomLFO.SampleValue R] [FMA R]
    (out : IO.FS.Stream) (precision : Nat) (bits : R → String) : IO Unit := do
  let mut state := WanderingRandomLFO.Stream.init (R := R) 6 11
  for _ in [:512] do
    let (next, value) := state.step
    out.putStrLn s!"spline {precision} {bits value}"
    state := next

private def framedSplineRows {R : Type} [WanderingRandomLFO.SampleValue R]
    [Sub R] [Div R] [FMA R]
    (out : IO.FS.Stream) (precision : Nat) (bits : R → String) : IO Unit := do
  let mut state := WanderingRandomLFO.FramedStream.init (R := R) 6 11
  for _ in [:512] do
    let (next, value) := state.step
    out.putStrLn s!"spline_framed {precision} {bits value}"
    state := next

def main : IO Unit := do
  let out ← IO.getStdout
  for n in [:180] do
    let f : Float := if n % 6 == 0 then 0.0000611 else if n % 6 == 1 then 0.25 else
      0.0001 + (n % 53).toFloat * 0.0047
    let sign : Float := if n % 3 == 0 then -1 else if n % 3 == 1 then 0 else 1
    let duration : Float := -9.965784284662087 + (n % 17).toFloat * 0.75
    let damp : Float := 1 + (n % 7).toFloat
    let rate : Float := if n % 2 == 0 then 48000 else 192000
    let x : Float := (n.toFloat * 0.137).sin
    let delayed : Float := (n.toFloat * 0.31).cos
    let seed : Float := (n % 13).toFloat * 0.1 - 0.6
    let bypass : Float := if n % 4 == 0 then 1 else 0
    for (tag, input, expected) in combRows f sign duration damp rate x delayed seed bypass do
      out.putStrLn s!"{tag} 64 {String.intercalate " " ((input ++ expected).map (fun v => toString v.toBits))}"
    let (input, expected) := allPassRow sign x delayed seed
    out.putStrLn s!"allpass 64 {String.intercalate " " ((input ++ expected).map (fun v => toString v.toBits))}"
    let (input32, expected32) := allPassRow sign.toFloat32 x.toFloat32 delayed.toFloat32 seed.toFloat32
    out.putStrLn s!"allpass 32 {String.intercalate " " ((input32 ++ expected32).map (fun v => toString v.toBits))}"
  ring out 1 (by decide)
  ring out 3 (by decide)
  ring out 8 (by decide)
  ring out 4096 (by decide)
  ring out 16384 (by decide)
  for capacity in [1,2,3,4,7,8,16] do
    addresses out capacity
  reverbRows out 32 Float.toFloat32 (fun x => toString x.toBits)
  reverbRows out 64 id (fun x => toString x.toBits)
  splineRows out 32 (fun x : Float32 => toString x.toBits)
  splineRows out 64 (fun x : Float => toString x.toBits)
  framedSplineRows out 32 (fun x : Float32 => toString x.toBits)
  framedSplineRows out 64 (fun x : Float => toString x.toBits)

end DSP.Tests.DelayCodegen

def main := DSP.Tests.DelayCodegen.main
