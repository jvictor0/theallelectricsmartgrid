import DSP.Filters.DelayKernels
import DSP.DelayBuffer.Basic

namespace DSP.Tests.DelayCodegen
open DelayKernels

def fields {R : Type} (c : Coefficients R) : List R :=
  [c.delay,c.a,c.b,c.r,c.delayGain,c.g,c.k,c.h,c.gain,c.sign,c.bypass]
def memory {R : Type} (s : TunedDecayComb.Branch R) : List R := [s.allpass,s.ic1,s.ic2,s.y]

def rows {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 7] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [Neg R] [FMA R] [IfLess R] [Trig R] [SquareRoot R] [DelayMath R]
    (f sign duration damp rate x delayed seed bypass : R) : List (String × List R × List R) :=
  let c : Coefficients R := ⟨7,0.6,0.8,0.97,0.8,0.3,1.2,1 / (1 + 1.2 * 0.3 + 0.3 * 0.3),0.8,sign,bypass⟩
  let s : CombState R := ⟨c, ⟨seed,seed+0.2,seed-0.3,seed+0.7⟩⟩
  let next := combStep s ⟨x,delayed⟩
  let ap : AllPassState R := ⟨sign * 0.7,seed,seed-0.3⟩
  let apn := allPassStep ap ⟨x,delayed⟩
  let tuned := prepare c ⟨f,sign,duration,damp,rate⟩
  let q := ResponseFrequency.fromHalfAngle f (Trig.sin2pi (f / 2)) (Trig.cos2pi (f / 2))
  let c1 := {c with a := -0.2, b := SquareRoot.sqrt 0.96, delay := 3, sign := -sign, bypass := 0}
  let response := (DelayKernels.response ⟨c,c1,0.3⟩ q).2
  [("comb", fields c ++ memory s.memory ++ [x,delayed], memory next.memory),
    ("allpass", [ap.gain,ap.y,ap.written,x,delayed], [apn.gain,apn.y,apn.written]),
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
    for (tag, input, expected) in rows f sign duration damp rate x delayed seed bypass do
      out.putStrLn s!"{tag} 64 {String.intercalate " " ((input ++ expected).map (fun v => toString v.toBits))}"
    for (tag, input, expected) in rows f.toFloat32 sign.toFloat32 duration.toFloat32 damp.toFloat32
        rate.toFloat32 x.toFloat32 delayed.toFloat32 seed.toFloat32 bypass.toFloat32 do
      out.putStrLn s!"{tag} 32 {String.intercalate " " ((input ++ expected).map (fun v => toString v.toBits))}"
  ring out 1 (by decide)
  ring out 3 (by decide)
  ring out 8 (by decide)
  ring out 4096 (by decide)
  ring out 16384 (by decide)
  for capacity in [1,2,3,4,7,8,16] do
    addresses out capacity

end DSP.Tests.DelayCodegen

def main := DSP.Tests.DelayCodegen.main
