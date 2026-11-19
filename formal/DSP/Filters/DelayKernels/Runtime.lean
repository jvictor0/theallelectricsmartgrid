import DSP.Filters.DelayKernels
import DSP.Processor.Compact

namespace DSP.DelayKernels.Runtime

namespace Comb

/-- Only the three recurrence memories survive between samples. -/
structure Memory (R : Type) where
  allpass : R
  ic1 : R
  ic2 : R

structure State (R : Type) where
  coefficients : Coefficients R
  memory : Memory R

structure Output (R : Type) where
  y : R

def save {R : Type} (s : CombState R) : State R :=
  ⟨s.coefficients, ⟨s.memory.allpass, s.memory.ic1, s.memory.ic2⟩⟩

def observe {R : Type} (s : CombState R) : Output R := ⟨s.memory.y⟩

def restore {R : Type} [OfNat R 0] (s : State R) : CombState R :=
  ⟨s.coefficients, ⟨s.memory.allpass, s.memory.ic1, s.memory.ic2, 0⟩⟩

@[simp] theorem save_restore {R : Type} [OfNat R 0] (s : State R) :
    save (restore s) = s := rfl

section Processing
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
  [Add R] [Sub R] [Mul R] [Div R] [IfLess R]

def model : Processor (CombState R) (CombInput R) (Output R) :=
  fun state input => let next := combStep state input; (next, observe next)

def process : Processor (State R) (CombInput R) (Output R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : CombState R) (x : CombInput R) :
    combStep (restore (save s)) x = combStep s x := rfl

theorem process_eq (s : CombState R) (x : CombInput R) :
    process (save s) x = (save (combStep s x), observe (combStep s x)) := rfl

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) :=
  Processor.compact_simulation _ _ _ (fun _ _ => rfl)

theorem run_eq (s : CombState R) (u : Nat → CombInput R) :
    Processor.run model u s = Processor.run process u (save s) :=
  Processor.run_simulation _ _ _ simulation _ _ _ rfl

end Processing
end Comb

namespace AllPass

/-- The delay line is external; this arithmetic core retains only its gain. -/
structure State (R : Type) where
  gain : R

structure Output (R : Type) where
  y : R
  written : R

def save {R : Type} (s : AllPassState R) : State R := ⟨s.gain⟩

def observe {R : Type} (s : AllPassState R) : Output R := ⟨s.y, s.written⟩

def restore {R : Type} [OfNat R 0] (s : State R) : AllPassState R := ⟨s.gain, 0, 0⟩

@[simp] theorem save_restore {R : Type} [OfNat R 0] (s : State R) :
    save (restore s) = s := rfl

section Processing
variable {R : Type} [OfNat R 0] [Neg R] [FMA R]

def model : Processor (AllPassState R) (CombInput R) (Output R) :=
  fun state input => let next := allPassStep state input; (next, observe next)

def process : Processor (State R) (CombInput R) (Output R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : AllPassState R) (x : CombInput R) :
    allPassStep (restore (save s)) x = allPassStep s x := rfl

theorem process_eq (s : AllPassState R) (x : CombInput R) :
    process (save s) x = (save (allPassStep s x), observe (allPassStep s x)) := rfl

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) :=
  Processor.compact_simulation _ _ _ (fun _ _ => rfl)

theorem run_eq (s : AllPassState R) (u : Nat → CombInput R) :
    Processor.run model u s = Processor.run process u (save s) :=
  Processor.run_simulation _ _ _ simulation _ _ _ rfl

end Processing
end AllPass

section Preparation
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Mul R] [Div R] [Neg R] [IfLess R] [Trig R] [SquareRoot R] [DelayMath R]

/-- Preparation is independent of any previous coefficient state. -/
def prepareCoefficients (p : LogicalSpec R) : Coefficients R :=
  prepare ⟨0,0,0,0,0,0,0,0,0,0,0⟩ p

theorem prepare_eq (old : Coefficients R) (p : LogicalSpec R) :
    prepareCoefficients p = prepare old p := rfl

end Preparation
end DSP.DelayKernels.Runtime
