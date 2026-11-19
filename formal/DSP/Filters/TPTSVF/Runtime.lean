import DSP.Filters.Capture
import DSP.Processor.Compact

namespace DSP.TPTSVF.Runtime

/-- Coefficients and the two actual integrator memories. Taps are observations. -/
structure State (R : Type) where
  k : R
  a1 : R
  a2 : R
  a3 : R
  ic1 : R
  ic2 : R

structure Output (R : Type) where
  lowPass : R
  bandPass : R
  highPass : R
  notch : R
  allPass : R

def save {R : Type} (s : TPTSVF R) : State R :=
  ⟨s.k,s.a1,s.a2,s.a3,s.ic1,s.ic2⟩

def observe {R : Type} (s : TPTSVF R) : Output R :=
  ⟨s.lowPass,s.bandPass,s.highPass,s.notch,s.allPass⟩

/-- Omitted taps are never read by the recurrence. These zeros vanish on export. -/
def restore {R : Type} [OfNat R 0] (s : State R) : TPTSVF R :=
  ⟨s.k,s.a1,s.a2,s.a3,s.ic1,s.ic2,0,0,0,0,0⟩

@[simp] theorem save_restore {R : Type} [OfNat R 0] (s : State R) : save (restore s) = s := rfl

section Processing
variable {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]

def model : Processor (TPTSVF R) (Sample R) (Output R) :=
  fun state input => let next := TPTSVF.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Output R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : TPTSVF R) (x : Sample R) :
    TPTSVF.step (restore (save s)) x = TPTSVF.step s x := rfl

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) :=
  Processor.compact_simulation _ _ _ (fun _ _ => rfl)

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (s : State R) : State R × Snapshot R :=
  let result := TPTSVF.prepareCoefficients spec (restore s)
  (save result.1, result.2)

def prepareFromSnapshot {R : Type} [OfNat R 0] [OfNat R 1]
    [Add R] [Mul R] [Div R] [FMA R]
    (spec : Snapshot R) (s : State R) : State R × Snapshot R :=
  let result := TPTSVF.prepareFromSnapshot spec (restore s)
  (save result.1, result.2)

end DSP.TPTSVF.Runtime
