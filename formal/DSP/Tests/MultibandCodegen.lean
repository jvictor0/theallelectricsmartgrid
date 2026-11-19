import DSP.Filters.MultibandSaturator.Exports
import DSP.Tests.MultibandSaturator

open DSP
namespace DSP.Tests.MultibandCodegen

def stateFields {R : Type} (s : MultibandSaturator.State 4 R) : List R :=
  Splitter.memoryFields s.splitter ++ List.ofFn s.bandGain ++ [s.masterGain]

def outputFields {R : Type} (o : MultibandSaturator.Output 4 R) : List R :=
  (List.ofFn fun i => [(o.bands i).before,(o.bands i).after]).flatten ++ [o.master.before,o.master.after,o.sub]

def line {R : Type} (bits : R → UInt64) (xs : List R) : String :=
  " ".intercalate (xs.map (fun x => toString (bits x)))

def cases {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 8]
    [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [Trig R] [Tanh R]
    (precision : Nat) (fromFloat : Float → R) (bits : R → UInt64)
    (c : Nat) [NeZero c] [OfNat R c] : IO Unit := do
  let out ← IO.getStdout
  for seed in [0,1,2] do
    let cutoffs := Splitter.spec4 (0.08:Float) 0.007 0.21
    let spec : MultibandSaturator.LogicalSpec 4 (Fin c → R) :=
      ⟨⟨Multiband.mapCutoffs (fun f j => fromFloat (f*(1+Float.ofNat j.val/7))) _ cutoffs.tree⟩,
        fun i j => fromFloat (if i.val == seed then 0 else 0.5+Float.ofNat (i.val+j.val)),
        fun j => fromFloat (0.8+Float.ofNat j.val/4)⟩
    let mut s := Multiband.freeze (MultibandSaturator.computeCoefficients spec
      ⟨Splitter.seeded 4 (fun j => fromFloat (0.03*Float.ofNat (seed+j.val))),fun _ _ => 0,fun _ => 0⟩)
    let snap := (MultibandSaturator.prepare spec).snapshot
    out.putStrLn s!"case {precision} {c} 96"
    out.putStrLn (line bits ((List.finRange c).flatMap fun j => (stateFields s).map (· j)))
    let snapValues := Multiband.snapshotFields _ snap.splitter.tree ++ List.ofFn snap.bandGain ++ [snap.masterGain]
    out.putStrLn (line bits ((List.finRange c).flatMap fun j => snapValues.map (· j)))
    for k in [:96] do
      let mono := k % 31 < 17
      let x := (StateSpace.snapshot c fun j => fromFloat
        (if k % 23 == 0 then -0.0 else 4*(Float.ofNat k*(0.07+Float.ofNat j.val/30)).cos)).values
      let (next,o) := MultibandSaturator.step4Band s ⟨x,mono⟩
      s := Multiband.freeze next
      let f := (StateSpace.snapshot c fun j => fromFloat (Float.ofNat ((k+j.val)%33)/64)).values
      let sine := (StateSpace.snapshot c fun j => fromFloat ((Float.ofNat ((k+j.val)%33)/64)*3.141592653589793).sin).values
      let cosine := (StateSpace.snapshot c fun j => fromFloat ((Float.ofNat ((k+j.val)%33)/64)*3.141592653589793).cos).values
      let power := snap.magnitudeSquared (ResponseFrequency.fromHalfAngle f sine cosine)
      let values := (List.finRange c).flatMap fun j => (stateFields s ++ outputFields o ++ [power]).map (· j)
      let frequencies := (List.finRange c).flatMap fun j => [f j,sine j,cosine j]
      out.putStrLn s!"{if mono then 1 else 0} {line bits (List.ofFn x ++ frequencies)} | {line bits values}"

end DSP.Tests.MultibandCodegen

def main : IO Unit := do
  for c in [1,2,3,4] do
    match c with
    | 1 =>
      DSP.Tests.MultibandCodegen.cases 64 id Float.toBits 1
      DSP.Tests.MultibandCodegen.cases 32 Float.toFloat32 (fun x => x.toBits.toUInt64) 1
    | 2 =>
      DSP.Tests.MultibandCodegen.cases 64 id Float.toBits 2
      DSP.Tests.MultibandCodegen.cases 32 Float.toFloat32 (fun x => x.toBits.toUInt64) 2
    | 3 =>
      DSP.Tests.MultibandCodegen.cases 64 id Float.toBits 3
      DSP.Tests.MultibandCodegen.cases 32 Float.toFloat32 (fun x => x.toBits.toUInt64) 3
    | _ =>
      DSP.Tests.MultibandCodegen.cases 64 id Float.toBits 4
      DSP.Tests.MultibandCodegen.cases 32 Float.toFloat32 (fun x => x.toBits.toUInt64) 4
