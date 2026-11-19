import DSP.Filters.LinkwitzRileySplitter

namespace DSP.Tests.Splitter
open DSP.LinkwitzRileySplitter

variable {R : Type}

def phaseFields (svf : TPTSVF R → List R) : (t : Topology) → PhaseState R t → List R
  | .leaf, f => [f.x]
  | .branch low high, f => svf f.svf ++ phaseFields svf low f.low ++ phaseFields svf high f.high

def treeFields (svf : TPTSVF R → List R) : (t : Topology) → TreeState R t → List R
  | .leaf, f => [f.x]
  | .branch low high, f => svf f.first ++ svf f.second ++
      phaseFields svf high f.lowPhase ++ phaseFields svf low f.highPhase ++
      treeFields svf low f.low ++ treeFields svf high f.high

def fields {n : Nat} (svf : TPTSVF R → List R) (f : State n R) : List R :=
  treeFields svf _ f.tree ++ List.ofFn f.bands

/-- Serialize only recurrence memories and coefficients from the original model. -/
def sectionMemory {R : Type} (s : TPTSVF R) : List R :=
  [s.k,s.a1,s.a2,s.a3,s.ic1,s.ic2]

def phaseMemory : (t : Topology) → PhaseState R t → List R
  | .leaf, _ => []
  | .branch low high, s => sectionMemory s.svf ++ phaseMemory low s.low ++ phaseMemory high s.high

def treeMemory : (t : Topology) → TreeState R t → List R
  | .leaf, _ => []
  | .branch low high, s => sectionMemory s.first ++ sectionMemory s.second ++
      phaseMemory high s.lowPhase ++ phaseMemory low s.highPhase ++ treeMemory low s.low ++ treeMemory high s.high

def memoryFields {n : Nat} (s : State n R) : List R := treeMemory _ s.tree

def compactFields {n : Nat} (s : State n R) : List R := memoryFields s ++ List.ofFn s.bands

section Seed
variable [OfNat R 0] [OfNat R 1] [Add R] [Neg R]

def seedSVF (seed : R) : TPTSVF R := ⟨0,0,0,0,seed,-seed,seed+1,seed+1,-seed,seed,seed⟩

def seedPhase : (t : Topology) → R → PhaseState R t
  | .leaf, v => ⟨v⟩
  | .branch low high, v => ⟨seedSVF v, seedPhase low (-v), seedPhase high (v+1)⟩

def seedTree : (t : Topology) → R → TreeState R t
  | .leaf, v => ⟨v⟩
  | .branch low high, v => ⟨seedSVF v, seedSVF (-v), seedPhase high (v+1), seedPhase low (-v),
      seedTree low (v+1), seedTree high (-v)⟩

def seeded (n : Nat) (v : R) : State n R := ⟨seedTree _ v, fun _ => v⟩
end Seed

def spec4 (middle low high : R) : LogicalSpec 4 R := ⟨⟨middle,⟨low,⟨⟩,⟨⟩⟩,⟨high,⟨⟩,⟨⟩⟩⟩⟩

def zeroPhase : (t : Topology) → PhaseState Float t
  | .leaf => ⟨0⟩
  | .branch low high => ⟨seedSVF 0, zeroPhase low, zeroPhase high⟩

def zeroTree : (t : Topology) → TreeState Float t
  | .leaf => ⟨0⟩
  | .branch low high => ⟨seedSVF 0, seedSVF 0, zeroPhase high, zeroPhase low, zeroTree low, zeroTree high⟩

def cutoffTree (n : Nat) : (t : Topology) → Nat → TreeData Float t
  | .leaf, _ => ⟨⟩
  | .branch low high, start =>
    ⟨0.01 + 0.42 * Float.ofNat (start+low.bands) / Float.ofNat n,
      cutoffTree n low start, cutoffTree n high (start+low.bands)⟩

def cutoffList : (t : Topology) → TreeData Float t → List Float
  | .leaf, _ => []
  | .branch low high, s => [s.value] ++ cutoffList low s.low ++ cutoffList high s.high

/-- Independent direct-form allpass reference, with no shared SVF recurrence. -/
structure DirectAP where
  b0 : Float
  b1 : Float
  a1 : Float
  a2 : Float
  x1 : Float := 0
  x2 : Float := 0
  y1 : Float := 0
  y2 : Float := 0

def direct (fc : Float) : DirectAP :=
  let g := Trig.tanPi fc
  let k := 1 / (Trig.cos2pi (1 / 8) : Float)
  let d := 1+g*k+g*g
  ⟨(1-g*k+g*g)/d, 2*(g*g-1)/d, 2*(g*g-1)/d, (1-g*k+g*g)/d, 0,0,0,0⟩

def DirectAP.step (s : DirectAP) (x : Float) : DirectAP × Float :=
  let y := s.b0*x+s.b1*s.x1+s.x2-s.a1*s.y1-s.a2*s.y2
  ({s with x1 := x, x2 := s.x1, y1 := y, y2 := s.y1}, y)

def cascade : List DirectAP → Float → List DirectAP × Float
  | [], x => ([],x)
  | s::ss, x =>
    let (s,y) := s.step x
    let (ss,z) := cascade ss y
    (s::ss,z)

def check : IO Unit := do
  for n in [1,2,3,4,5,7,8] do
    let topology := Topology.balanced n
    unless topology.bands == n do throw (IO.userError s!"splitter {n}: incorrect leaf count")
    let spec : LogicalSpec n Float := ⟨cutoffTree n topology 0⟩
    let mut state := computeCoefficients spec (⟨zeroTree topology,fun _ => 0⟩ : State n Float)
    let mut reference := (cutoffList topology spec.tree).map direct
    for sample in [:4096] do
      let x := if sample == 0 then 1.0 else if sample < 2048 then 0.0
        else (0.13*Float.ofNat sample).sin
      state := step state ⟨x⟩
      let (next,y) := cascade reference x
      reference := next
      let sum := (List.ofFn state.bands).foldl (·+·) 0
      unless (sum-y).abs < 1e-10 do
        throw (IO.userError s!"splitter {n} sample {sample}: reconstructed {sum}, allpass reference {y}")
  IO.println "Splitter: 1,2,3,4,5,7,8 bands reconstruct an independent allpass cascade."

end DSP.Tests.Splitter
