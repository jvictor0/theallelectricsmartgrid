import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Capture

/-! Prepare each cutoff once, then install its coefficients in every split and
phase-compensation section that uses it. Preparation is generic in arithmetic:
its lanes can represent independent cutoffs, separately from the audio lanes.
The existing TPTSVF tuner remains the only source of the section arithmetic. -/
namespace DSP.LinkwitzRileySplitter

variable {R : Type}

def zeroSection [OfNat R 0] : TPTSVF R :=
  ⟨0,0,0,0,0,0,0,0,0,0,0⟩

def resetSection [OfNat R 0] (s : TPTSVF R) : TPTSVF R :=
  {s with
    ic1 := 0, ic2 := 0, lowPass := 0, bandPass := 0, highPass := 0,
    notch := 0, allPass := 0}

def zeroPhase [OfNat R 0] : (t : Topology) → PhaseState R t
  | .leaf => ⟨0⟩
  | .branch low high => ⟨zeroSection, zeroPhase low, zeroPhase high⟩

def zeroTree [OfNat R 0] : (t : Topology) → TreeState R t
  | .leaf => ⟨0⟩
  | .branch low high =>
    ⟨zeroSection, zeroSection, zeroPhase high, zeroPhase low, zeroTree low, zeroTree high⟩

/-- An untuned state. Install coefficients before processing. -/
def zero [OfNat R 0] (numBands : Nat) : State numBands R :=
  ⟨zeroTree _, fun _ => 0⟩

def resetPhase [OfNat R 0] : (t : Topology) → PhaseState R t → PhaseState R t
  | .leaf, _ => ⟨0⟩
  | .branch low high, s => ⟨resetSection s.svf, resetPhase low s.low, resetPhase high s.high⟩

def resetTree [OfNat R 0] : (t : Topology) → TreeState R t → TreeState R t
  | .leaf, _ => ⟨0⟩
  | .branch low high, s =>
    ⟨resetSection s.first, resetSection s.second,
      resetPhase high s.lowPhase, resetPhase low s.highPhase,
      resetTree low s.low, resetTree high s.high⟩

/-- Clear histories and stored outputs, retaining all section coefficients. -/
def reset {n : Nat} [OfNat R 0] (s : State n R) : State n R :=
  ⟨resetTree _ s.tree, fun _ => 0⟩

structure PreparedSection (R : Type) where
  snapshot : TPTSVF.Snapshot R
  a1 : R
  a2 : R
  a3 : R

structure PreparedSpec (numBands : Nat) (R : Type) where
  tree : TreeData (PreparedSection R) (Topology.balanced numBands)

/-- All fields are independent between arithmetic lanes. A caller may prepare
a packed bank of cutoffs and broadcast each result to its audio channels. -/
def prepareSection [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] (cutoff : R) : PreparedSection R :=
  let prepared := TPTSVF.prepareCoefficients (sectionSpec cutoff) zeroSection
  ⟨prepared.2, prepared.1.a1, prepared.1.a2, prepared.1.a3⟩

def prepareTree [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] :
    (t : Topology) → TreeData R t → TreeData (PreparedSection R) t
  | .leaf, _ => ⟨⟩
  | .branch low high, s =>
    ⟨prepareSection s.value, prepareTree low s.low, prepareTree high s.high⟩

def prepare {n : Nat} [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] (spec : LogicalSpec n R) : PreparedSpec n R :=
  ⟨prepareTree _ spec.tree⟩

def installSection (c : PreparedSection R) (s : TPTSVF R) : TPTSVF R :=
  {s with k := c.snapshot.k, a1 := c.a1, a2 := c.a2, a3 := c.a3}

def installPhase : (t : Topology) → TreeData (PreparedSection R) t → PhaseState R t → PhaseState R t
  | .leaf, _, s => s
  | .branch low high, c, s =>
    ⟨installSection c.value s.svf, installPhase low c.low s.low, installPhase high c.high s.high⟩

def installTree : (t : Topology) → TreeData (PreparedSection R) t → TreeState R t → TreeState R t
  | .leaf, _, s => s
  | .branch low high, c, s =>
    ⟨installSection c.value s.first, installSection c.value s.second,
      installPhase high c.high s.lowPhase, installPhase low c.low s.highPhase,
      installTree low c.low s.low, installTree high c.high s.high⟩

/-- Installation only copies coefficients. It performs no arithmetic or trig. -/
def installPrepared {n : Nat} (c : PreparedSpec n R) (s : State n R) : State n R :=
  {s with tree := installTree _ c.tree s.tree}

def preparedSnapshotTree :
    (t : Topology) → TreeData (PreparedSection R) t → TreeData (TPTSVF.Snapshot R) t
  | .leaf, _ => ⟨⟩
  | .branch low high, c =>
    ⟨c.value.snapshot, preparedSnapshotTree low c.low, preparedSnapshotTree high c.high⟩

def PreparedSpec.snapshot {n : Nat} (c : PreparedSpec n R) : Snapshot n R :=
  ⟨preparedSnapshotTree _ c.tree⟩

end DSP.LinkwitzRileySplitter
