import DSP.Filters.LinkwitzRileySplitter.Prepared
import DSP.Filters.TPTSVF.Runtime

namespace DSP.LinkwitzRileySplitter.Runtime

structure PhaseNode (R L H : Type) where
  svf : TPTSVF.Runtime.State R
  low : L
  high : H

@[reducible] def PhaseState (R : Type) : Topology → Type
  | .leaf => Empty
  | .branch low high => PhaseNode R (PhaseState R low) (PhaseState R high)

structure SplitNode (R L H LP HP : Type) where
  first : TPTSVF.Runtime.State R
  second : TPTSVF.Runtime.State R
  lowPhase : LP
  highPhase : HP
  low : L
  high : H

@[reducible] def TreeState (R : Type) : Topology → Type
  | .leaf => Empty
  | .branch low high => SplitNode R (TreeState R low) (TreeState R high)
      (PhaseState R high) (PhaseState R low)

structure State (numBands : Nat) (R : Type) where
  tree : TreeState R (Topology.balanced numBands)

structure Output (numBands : Nat) (R : Type) where
  bands : Fin numBands → R

variable {R : Type}

def savePhase : (t : Topology) → LinkwitzRileySplitter.PhaseState R t → PhaseState R t
  | .leaf, _ => ⟨⟩
  | .branch low high, s => ⟨TPTSVF.Runtime.save s.svf, savePhase low s.low, savePhase high s.high⟩

def saveTree : (t : Topology) → LinkwitzRileySplitter.TreeState R t → TreeState R t
  | .leaf, _ => ⟨⟩
  | .branch low high, s =>
    ⟨TPTSVF.Runtime.save s.first, TPTSVF.Runtime.save s.second,
      savePhase high s.lowPhase, savePhase low s.highPhase, saveTree low s.low, saveTree high s.high⟩

def save {n : Nat} (s : LinkwitzRileySplitter.State n R) : State n R := ⟨saveTree _ s.tree⟩

def restorePhase [OfNat R 0] : (t : Topology) → PhaseState R t → LinkwitzRileySplitter.PhaseState R t
  | .leaf, _ => ⟨0⟩
  | .branch low high, s =>
    ⟨TPTSVF.Runtime.restore s.svf, restorePhase low s.low, restorePhase high s.high⟩

def restoreTree [OfNat R 0] : (t : Topology) → TreeState R t → LinkwitzRileySplitter.TreeState R t
  | .leaf, _ => ⟨0⟩
  | .branch low high, s =>
    ⟨TPTSVF.Runtime.restore s.first, TPTSVF.Runtime.restore s.second,
      restorePhase high s.lowPhase, restorePhase low s.highPhase,
      restoreTree low s.low, restoreTree high s.high⟩

def restore {n : Nat} [OfNat R 0] (s : State n R) : LinkwitzRileySplitter.State n R :=
  ⟨restoreTree _ s.tree, fun _ => 0⟩

section Processing
variable [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]

@[simp] theorem phaseStep_restore (t : Topology) (s : LinkwitzRileySplitter.PhaseState R t)
    (x : Sample R) : phaseStep t (restorePhase t (savePhase t s)) x = phaseStep t s x := by
  induction t generalizing x with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [restorePhase, savePhase, phaseStep, TPTSVF.Runtime.step_restore, hl, hh]

@[simp] theorem treeStep_restore (t : Topology) (s : LinkwitzRileySplitter.TreeState R t)
    (x : Sample R) : treeStep t (restoreTree t (saveTree t s)) x = treeStep t s x := by
  induction t generalizing x with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [restoreTree, saveTree, treeStep, TPTSVF.Runtime.step_restore, phaseStep_restore, hl, hh]

@[simp] theorem step_restore {n : Nat} (s : LinkwitzRileySplitter.State n R) (x : Sample R) :
    LinkwitzRileySplitter.step (restore (save s)) x = LinkwitzRileySplitter.step s x := by
  simp only [LinkwitzRileySplitter.step, restore, save, treeStep_restore]

def model {n : Nat} : Processor (LinkwitzRileySplitter.State n R) (Sample R) (Output n R) :=
  fun state input => let next := LinkwitzRileySplitter.step state input; (next, ⟨next.bands⟩)

def process {n : Nat} : Processor (State n R) (Sample R) (Output n R) :=
  Processor.compact model save restore

theorem simulation {n : Nat} : Processor.Simulation (model (n := n) (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def computeCoefficients {n : Nat} [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] (spec : LogicalSpec n R) (s : State n R) : State n R :=
  save (LinkwitzRileySplitter.computeCoefficients spec (restore s))

def installPrepared {n : Nat} [OfNat R 0] (spec : PreparedSpec n R) (s : State n R) : State n R :=
  save (LinkwitzRileySplitter.installPrepared spec (restore s))

def reset {n : Nat} [OfNat R 0] (s : State n R) : State n R :=
  save (LinkwitzRileySplitter.reset (restore s))

def process4Band [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :=
  process (n := 4) (R := R)

def computeCoefficients4Band [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] := computeCoefficients (n := 4) (R := R)

end DSP.LinkwitzRileySplitter.Runtime
