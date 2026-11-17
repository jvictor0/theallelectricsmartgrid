import DSP.Filters.Tuning
import Mathlib.Data.Nat.Basic

/-! A compile-time crossover tree. Specializing the band count reduces the
state to nested records; no topology or collection of filter records survives
as a runtime parameter. All signal histories belong to the shared TPTSVF. -/
namespace DSP.LinkwitzRileySplitter

inductive Topology where
  | leaf
  | branch (low high : Topology)
  deriving DecidableEq

def Topology.bands : Topology → Nat
  | .leaf => 1
  | .branch low high => low.bands + high.bands

/-- Zero is mapped to the identity topology; the public splitter contract
requires a positive band count. Odd counts split into floor/ceiling halves. -/
def Topology.balancedAux : Nat → Nat → Topology
  | 0, _ => .leaf
  | _ + 1, 0 => .leaf
  | _ + 1, 1 => .leaf
  | fuel + 1, n + 2 =>
    .branch (balancedAux fuel ((n + 2) / 2)) (balancedAux fuel ((n + 2) - (n + 2) / 2))

def Topology.balanced (n : Nat) : Topology := balancedAux n n

theorem Topology.bands_pos (t : Topology) : 0 < t.bands := by
  induction t with
  | leaf => exact Nat.zero_lt_one
  | branch low high hl hh => simp only [bands]; omega

theorem Topology.balancedAux_bands (fuel n : Nat) (h : 0 < n) (hf : n ≤ fuel + 1) :
    (balancedAux fuel n).bands = n := by
  induction fuel generalizing n with
  | zero =>
    have hn : n = 1 := by omega
    subst n
    rfl
  | succ fuel ih =>
    match n with
    | 0 => omega
    | 1 => rfl
    | n + 2 =>
      rw [balancedAux, bands, ih _ (by omega) (by omega), ih _ (by omega) (by omega)]
      omega

theorem Topology.balanced_bands (n : Nat) (h : 0 < n) : (balanced n).bands = n :=
  balancedAux_bands n n h (by omega)

structure Empty where

structure BranchData (V L H : Type) where
  value : V
  low : L
  high : H

@[reducible] def TreeData (V : Type) : Topology → Type
  | .leaf => Empty
  | .branch low high => BranchData V (TreeData V low) (TreeData V high)

structure PhaseNode (R L H : Type) where
  svf : TPTSVF R
  low : L
  high : H

@[reducible] def PhaseState (R : Type) : Topology → Type
  | .leaf => Sample R
  | .branch low high => PhaseNode R (PhaseState R low) (PhaseState R high)

structure SplitNode (R L H LP HP : Type) where
  first : TPTSVF R
  second : TPTSVF R
  lowPhase : LP
  highPhase : HP
  low : L
  high : H

@[reducible] def TreeState (R : Type) : Topology → Type
  | .leaf => Sample R
  | .branch low high => SplitNode R (TreeState R low) (TreeState R high)
      (PhaseState R high) (PhaseState R low)

structure State (numBands : Nat) (R : Type) where
  tree : TreeState R (Topology.balanced numBands)
  bands : Fin numBands → R

structure LogicalSpec (numBands : Nat) (R : Type) where
  tree : TreeData R (Topology.balanced numBands)

structure Snapshot (numBands : Nat) (R : Type) where
  tree : TreeData (TPTSVF.Snapshot R) (Topology.balanced numBands)

variable {R : Type}

def phaseOutput : (t : Topology) → PhaseState R t → R
  | .leaf, f => f.x
  | .branch _ high, f => phaseOutput high f.high

section Processing
variable [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2]

/-- An independent cascade of the all-pass tap at every node of a subtree. -/
def phaseStep : (t : Topology) → PhaseState R t → Sample R → PhaseState R t
  | .leaf, _, input => input
  | .branch low high, f, input =>
    let svf := f.svf.step input
    let lo := phaseStep low f.low ⟨svf.allPass⟩
    let hi := phaseStep high f.high ⟨phaseOutput low lo⟩
    ⟨svf, lo, hi⟩

/-- Each child gets the phase of its sibling's complete subtree. The two
root sections produce LP4 and AP-LP4; the LR4 damping makes the latter HP4. -/
def treeStep : (t : Topology) → TreeState R t → Sample R → TreeState R t
  | .leaf, _, input => input
  | .branch low high, f, input =>
    let first := f.first.step input
    let second := f.second.step ⟨first.lowPass⟩
    let lowPhase := phaseStep high f.lowPhase ⟨second.lowPass⟩
    let highPhase := phaseStep low f.highPhase ⟨first.allPass - second.lowPass⟩
    ⟨first, second, lowPhase, highPhase,
      treeStep low f.low ⟨phaseOutput high lowPhase⟩,
      treeStep high f.high ⟨phaseOutput low highPhase⟩⟩

end Processing

/-- Bands are enumerated from low to high. Callers use indices below bands. -/
def treeBand : (t : Topology) → TreeState R t → Nat → R
  | .leaf, f, _ => f.x
  | .branch low high, f, i =>
    if i < low.bands then treeBand low f.low i else treeBand high f.high (i - low.bands)

def step {numBands : Nat} [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2]
    (f : State numBands R) (input : Sample R) : State numBands R :=
  let tree := treeStep _ f.tree input
  ⟨tree, fun i => treeBand _ tree i.val⟩

section Tuning
variable [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R]

/-- Q is a property of the splitter, not an independent user parameter. -/
def sectionSpec (cutoff : R) : TPTSVF.LogicalSpec R := ⟨cutoff, Trig.cos2pi (1 / 8)⟩

def tunePhase : (t : Topology) → TreeData R t → PhaseState R t → PhaseState R t
  | .leaf, _, f => f
  | .branch low high, spec, f =>
    ⟨TPTSVF.computeCoefficients (sectionSpec spec.value) f.svf,
      tunePhase low spec.low f.low, tunePhase high spec.high f.high⟩

def tuneTree : (t : Topology) → TreeData R t → TreeState R t → TreeState R t
  | .leaf, _, f => f
  | .branch low high, spec, f =>
    ⟨TPTSVF.computeCoefficients (sectionSpec spec.value) f.first,
      TPTSVF.computeCoefficients (sectionSpec spec.value) f.second,
      tunePhase high spec.high f.lowPhase, tunePhase low spec.low f.highPhase,
      tuneTree low spec.low f.low, tuneTree high spec.high f.high⟩

def computeCoefficients {numBands : Nat} (spec : LogicalSpec numBands R)
    (state : State numBands R) : State numBands R :=
  { state with tree := tuneTree _ spec.tree state.tree }

/-- Capture logical integrator gains directly, without dividing rounded solved
coefficients. The tuning proof relates this snapshot to every runtime section. -/
def snapshotTree : (t : Topology) → TreeData R t → TreeData (TPTSVF.Snapshot R) t
  | .leaf, _ => ⟨⟩
  | .branch low high, spec =>
    ⟨⟨Trig.tanPi spec.value, 1 / (sectionSpec spec.value).q⟩,
      snapshotTree low spec.low, snapshotTree high spec.high⟩

def makeSnapshot {numBands : Nat} (spec : LogicalSpec numBands R) : Snapshot numBands R :=
  ⟨snapshotTree _ spec.tree⟩

end Tuning

/-- Only arithmetic type and channel width remain generic in the emitted code. -/
def step4Band [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2] :
    State 4 R → Sample R → State 4 R := step

def computeCoefficients4Band [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R] :
    LogicalSpec 4 R → State 4 R → State 4 R := computeCoefficients

def makeSnapshot4Band [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R] :
    LogicalSpec 4 R → Snapshot 4 R := makeSnapshot

end DSP.LinkwitzRileySplitter
