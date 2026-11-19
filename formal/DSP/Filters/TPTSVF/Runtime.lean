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

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
def prepareFromSnapshotFloat := prepareFromSnapshot (R := Float)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (s : TPTSVF R) :
    prepareCoefficients spec (save s) = (save (TPTSVF.prepareCoefficients spec s).1, (TPTSVF.prepareCoefficients spec s).2) := rfl

theorem prepareFromSnapshot_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (spec : Snapshot R) (s : TPTSVF R) :
    prepareFromSnapshot spec (save s) =
      (save (TPTSVF.prepareFromSnapshot spec s).1, (TPTSVF.prepareFromSnapshot spec s).2) := rfl

structure LowPassOutput (R : Type) where
  lowPass : R

def processLowPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    : Processor (State R) (Sample R) (LowPassOutput R) :=
  Processor.mapOutput (fun output => ⟨output.lowPass⟩) process

theorem processLowPass_eq {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    (s : TPTSVF R) (x : Sample R) :
    processLowPass (save s) x = (save (TPTSVF.step s x), ⟨(TPTSVF.step s x).lowPass⟩) := rfl

theorem simulationLowPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :
    Processor.Simulation
      (Processor.mapOutput (fun output : Output R => (⟨output.lowPass⟩ : LowPassOutput R)) model)
      processLowPass (fun s m => m = save s) :=
  simulation.mapOutput _

def processLowPassFloat := processLowPass (R := Float)

structure HighPassOutput (R : Type) where
  highPass : R

def processHighPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    : Processor (State R) (Sample R) (HighPassOutput R) :=
  Processor.mapOutput (fun output => ⟨output.highPass⟩) process

theorem processHighPass_eq {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    (s : TPTSVF R) (x : Sample R) :
    processHighPass (save s) x = (save (TPTSVF.step s x), ⟨(TPTSVF.step s x).highPass⟩) := rfl

theorem simulationHighPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :
    Processor.Simulation
      (Processor.mapOutput (fun output : Output R => (⟨output.highPass⟩ : HighPassOutput R)) model)
      processHighPass (fun s m => m = save s) :=
  simulation.mapOutput _

def processHighPassFloat := processHighPass (R := Float)

structure BandPassOutput (R : Type) where
  bandPass : R

def processBandPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    : Processor (State R) (Sample R) (BandPassOutput R) :=
  Processor.mapOutput (fun output => ⟨output.bandPass⟩) process

theorem processBandPass_eq {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    (s : TPTSVF R) (x : Sample R) :
    processBandPass (save s) x = (save (TPTSVF.step s x), ⟨(TPTSVF.step s x).bandPass⟩) := rfl

theorem simulationBandPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :
    Processor.Simulation
      (Processor.mapOutput (fun output : Output R => (⟨output.bandPass⟩ : BandPassOutput R)) model)
      processBandPass (fun s m => m = save s) :=
  simulation.mapOutput _

def processBandPassFloat := processBandPass (R := Float)

structure NotchOutput (R : Type) where
  notch : R

def processNotch {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    : Processor (State R) (Sample R) (NotchOutput R) :=
  Processor.mapOutput (fun output => ⟨output.notch⟩) process

theorem processNotch_eq {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    (s : TPTSVF R) (x : Sample R) :
    processNotch (save s) x = (save (TPTSVF.step s x), ⟨(TPTSVF.step s x).notch⟩) := rfl

theorem simulationNotch {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :
    Processor.Simulation
      (Processor.mapOutput (fun output : Output R => (⟨output.notch⟩ : NotchOutput R)) model)
      processNotch (fun s m => m = save s) :=
  simulation.mapOutput _

def processNotchFloat := processNotch (R := Float)

structure AllPassOutput (R : Type) where
  allPass : R

def processAllPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    : Processor (State R) (Sample R) (AllPassOutput R) :=
  Processor.mapOutput (fun output => ⟨output.allPass⟩) process

theorem processAllPass_eq {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
    (s : TPTSVF R) (x : Sample R) :
    processAllPass (save s) x = (save (TPTSVF.step s x), ⟨(TPTSVF.step s x).allPass⟩) := rfl

theorem simulationAllPass {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] :
    Processor.Simulation
      (Processor.mapOutput (fun output : Output R => (⟨output.allPass⟩ : AllPassOutput R)) model)
      processAllPass (fun s m => m = save s) :=
  simulation.mapOutput _

def processAllPassFloat := processAllPass (R := Float)

end DSP.TPTSVF.Runtime
