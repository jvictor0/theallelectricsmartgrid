import DSP.Filters.TPTSVF.Runtime

/-! Compact representations of the existing proved filter steps. Only memories
needed by later samples are persistent; observations are projected at export.
Every recurrence and tuner is reused from its original model. -/
namespace DSP

namespace OnePoleTPTLowPass.Runtime
structure State (R : Type) where
  G : R
  s : R

def save {R : Type} (s : OnePoleTPTLowPass R) : State R := ⟨s.G,s.s⟩

def restore {R : Type} [OfNat R 0] (s : State R) : OnePoleTPTLowPass R := ⟨s.G,s.s,0⟩

def observe {R : Type} (s : OnePoleTPTLowPass R) : Sample R := ⟨s.y⟩

section Processing
variable {R : Type} [OfNat R 0] [Sub R] [FMA R]

def model : Processor (OnePoleTPTLowPass R) (Sample R) (Sample R) :=
  fun state input => let next := OnePoleTPTLowPass.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : OnePoleTPTLowPass R) (x : Sample R) :
    OnePoleTPTLowPass.step (restore (save s)) x = OnePoleTPTLowPass.step s x := by
  rfl

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : OnePoleTPTLowPass.LogicalSpec R) (s : State R) : State R × OnePoleTPTLowPass.Snapshot R :=
  let prepared := OnePoleTPTLowPass.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : OnePoleTPTLowPass.LogicalSpec R) (s : OnePoleTPTLowPass R) :
    prepareCoefficients spec (save s) =
      (save (OnePoleTPTLowPass.prepareCoefficients spec s).1, (OnePoleTPTLowPass.prepareCoefficients spec s).2) := rfl

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end OnePoleTPTLowPass.Runtime

namespace OnePoleTPTHighPass.Runtime
structure State (R : Type) where
  lowPass : OnePoleTPTLowPass.Runtime.State R

def save {R : Type} (s : OnePoleTPTHighPass R) : State R := ⟨OnePoleTPTLowPass.Runtime.save s.lowPass⟩

def restore {R : Type} [OfNat R 0] (s : State R) : OnePoleTPTHighPass R := ⟨OnePoleTPTLowPass.Runtime.restore s.lowPass,0⟩

def observe {R : Type} (s : OnePoleTPTHighPass R) : Sample R := ⟨s.y⟩

section Processing
variable {R : Type} [OfNat R 0] [Sub R] [FMA R]

def model : Processor (OnePoleTPTHighPass R) (Sample R) (Sample R) :=
  fun state input => let next := OnePoleTPTHighPass.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : OnePoleTPTHighPass R) (x : Sample R) :
    OnePoleTPTHighPass.step (restore (save s)) x = OnePoleTPTHighPass.step s x := by
  simp only [save, restore, OnePoleTPTHighPass.step, OnePoleTPTLowPass.Runtime.step_restore]

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : OnePoleTPTHighPass.LogicalSpec R) (s : State R) : State R × OnePoleTPTHighPass.Snapshot R :=
  let prepared := OnePoleTPTHighPass.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : OnePoleTPTHighPass.LogicalSpec R) (s : OnePoleTPTHighPass R) :
    prepareCoefficients spec (save s) =
      (save (OnePoleTPTHighPass.prepareCoefficients spec s).1, (OnePoleTPTHighPass.prepareCoefficients spec s).2) := rfl

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end OnePoleTPTHighPass.Runtime

namespace TPTBaseWidth.Runtime
structure State (R : Type) where
  highPass : OnePoleTPTHighPass.Runtime.State R
  lowPass : OnePoleTPTLowPass.Runtime.State R

def save {R : Type} (s : TPTBaseWidth R) : State R := ⟨OnePoleTPTHighPass.Runtime.save s.highPass,OnePoleTPTLowPass.Runtime.save s.lowPass⟩

def restore {R : Type} [OfNat R 0] (s : State R) : TPTBaseWidth R := ⟨OnePoleTPTHighPass.Runtime.restore s.highPass,OnePoleTPTLowPass.Runtime.restore s.lowPass⟩

def observe {R : Type} (s : TPTBaseWidth R) : Sample R := ⟨s.lowPass.y⟩

section Processing
variable {R : Type} [OfNat R 0] [Sub R] [FMA R]

def model : Processor (TPTBaseWidth R) (Sample R) (Sample R) :=
  fun state input => let next := TPTBaseWidth.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : TPTBaseWidth R) (x : Sample R) :
    TPTBaseWidth.step (restore (save s)) x = TPTBaseWidth.step s x := by
  simp only [save, restore, TPTBaseWidth.step, OnePoleTPTHighPass.Runtime.step_restore, OnePoleTPTLowPass.Runtime.step_restore]

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : TPTBaseWidth.LogicalSpec R) (s : State R) : State R × TPTBaseWidth.Snapshot R :=
  let prepared := TPTBaseWidth.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : TPTBaseWidth.LogicalSpec R) (s : TPTBaseWidth R) :
    prepareCoefficients spec (save s) =
      (save (TPTBaseWidth.prepareCoefficients spec s).1, (TPTBaseWidth.prepareCoefficients spec s).2) := rfl

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTBaseWidth.Runtime

namespace MorphingTPTSVF.Runtime
structure State (R : Type) where
  svf : TPTSVF.Runtime.State R
  cosine : R
  sine : R
  blend : R

def save {R : Type} (s : MorphingTPTSVF R) : State R := ⟨TPTSVF.Runtime.save s.svf,s.cosine,s.sine,s.blend⟩

def restore {R : Type} [OfNat R 0] (s : State R) : MorphingTPTSVF R := ⟨TPTSVF.Runtime.restore s.svf,s.cosine,s.sine,s.blend,0⟩

def observe {R : Type} (s : MorphingTPTSVF R) : Sample R := ⟨s.output⟩

section Processing
variable {R : Type} [OfNat R 0] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [IfLess R]

def model : Processor (MorphingTPTSVF R) (Sample R) (Sample R) :=
  fun state input => let next := MorphingTPTSVF.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : MorphingTPTSVF R) (x : Sample R) :
    MorphingTPTSVF.step (restore (save s)) x = MorphingTPTSVF.step s x := by
  simp only [save, restore, MorphingTPTSVF.step, TPTSVF.Runtime.step_restore]

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : MorphingTPTSVF.LogicalSpec R) (s : State R) : State R × MorphingTPTSVF.Snapshot R :=
  let prepared := MorphingTPTSVF.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : MorphingTPTSVF.LogicalSpec R) (s : MorphingTPTSVF R) :
    prepareCoefficients spec (save s) =
      (save (MorphingTPTSVF.prepareCoefficients spec s).1, (MorphingTPTSVF.prepareCoefficients spec s).2) := rfl

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end MorphingTPTSVF.Runtime

namespace TPTLadder.Runtime
structure State (R : Type) where
  k : R
  stage1 : OnePoleTPTLowPass.Runtime.State R
  stage2 : OnePoleTPTLowPass.Runtime.State R
  stage3 : OnePoleTPTLowPass.Runtime.State R
  stage4 : OnePoleTPTLowPass.Runtime.State R

def save {R : Type} (s : TPTLadder R) : State R := ⟨s.k,OnePoleTPTLowPass.Runtime.save s.stage1,OnePoleTPTLowPass.Runtime.save s.stage2,OnePoleTPTLowPass.Runtime.save s.stage3,OnePoleTPTLowPass.Runtime.save s.stage4⟩

def restore {R : Type} [OfNat R 0] (s : State R) : TPTLadder R := ⟨s.k,0,OnePoleTPTLowPass.Runtime.restore s.stage1,OnePoleTPTLowPass.Runtime.restore s.stage2,OnePoleTPTLowPass.Runtime.restore s.stage3,OnePoleTPTLowPass.Runtime.restore s.stage4⟩

def observe {R : Type} (s : TPTLadder R) : Sample R := ⟨s.stage4.y⟩

section Processing
variable {R : Type} [OfNat R 0] [OfNat R 1] [Sub R] [Neg R] [Mul R] [Div R] [FMA R]

def model : Processor (TPTLadder R) (Sample R) (Sample R) :=
  fun state input => let next := TPTLadder.step state input; (next, observe next)

def process : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact model save restore

@[simp] theorem step_restore (s : TPTLadder R) (x : Sample R) :
    TPTLadder.step (restore (save s)) x = TPTLadder.step s x := by
  simp only [save, restore, TPTLadder.step, OnePoleTPTLowPass.Runtime.step_restore]
  rfl

theorem simulation : Processor.Simulation (model (R := R)) process (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [model, step_restore]

end Processing

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : TPTLadder.LogicalSpec R) (s : State R) : State R × TPTLadder.Snapshot R :=
  let prepared := TPTLadder.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)

theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : TPTLadder.LogicalSpec R) (s : TPTLadder R) :
    prepareCoefficients spec (save s) =
      (save (TPTLadder.prepareCoefficients spec s).1, (TPTLadder.prepareCoefficients spec s).2) := rfl

def processFloat := process (R := Float)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTLadder.Runtime

namespace TPTSVF4Pole.Runtime
structure State (R : Type) where
  stage1 : TPTSVF.Runtime.State R
  stage2 : TPTSVF.Runtime.State R

def save {R : Type} (s : TPTSVF4Pole R) : State R :=
  ⟨TPTSVF.Runtime.save s.stage1,TPTSVF.Runtime.save s.stage2⟩
def restore {R : Type} [OfNat R 0] (s : State R) : TPTSVF4Pole R :=
  ⟨TPTSVF.Runtime.restore s.stage1,TPTSVF.Runtime.restore s.stage2⟩

section
variable {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
def modelLowPass : Processor (TPTSVF4Pole R) (Sample R) (Sample R) :=
  fun state input => let next := TPTSVF4Pole.stepLowPass state input; (next, ⟨next.stage2.lowPass⟩)
def processLowPass : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact modelLowPass save restore

theorem simulationLowPass : Processor.Simulation (modelLowPass (R := R)) processLowPass (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [modelLowPass, TPTSVF4Pole.stepLowPass, restore, save, TPTSVF.Runtime.step_restore]
end

def processLowPassFloat := processLowPass (R := Float)

section
variable {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
def modelHighPass : Processor (TPTSVF4Pole R) (Sample R) (Sample R) :=
  fun state input => let next := TPTSVF4Pole.stepHighPass state input; (next, ⟨next.stage2.highPass⟩)
def processHighPass : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact modelHighPass save restore

theorem simulationHighPass : Processor.Simulation (modelHighPass (R := R)) processHighPass (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [modelHighPass, TPTSVF4Pole.stepHighPass, restore, save, TPTSVF.Runtime.step_restore]
end

def processHighPassFloat := processHighPass (R := Float)

section
variable {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
def modelBandPass : Processor (TPTSVF4Pole R) (Sample R) (Sample R) :=
  fun state input => let next := TPTSVF4Pole.stepBandPass state input; (next, ⟨next.stage2.bandPass⟩)
def processBandPass : Processor (State R) (Sample R) (Sample R) :=
  Processor.compact modelBandPass save restore

theorem simulationBandPass : Processor.Simulation (modelBandPass (R := R)) processBandPass (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [modelBandPass, TPTSVF4Pole.stepBandPass, restore, save, TPTSVF.Runtime.step_restore]
end

def processBandPassFloat := processBandPass (R := Float)

def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : TPTSVF4Pole.LogicalSpec R) (s : State R) : State R × TPTSVF4Pole.Snapshot R :=
  let prepared := TPTSVF4Pole.prepareCoefficients spec (restore s)
  (save prepared.1,prepared.2)
theorem prepare_save {R : Type} [OfNat R 0] [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : TPTSVF4Pole.LogicalSpec R) (s : TPTSVF4Pole R) :
    prepareCoefficients spec (save s) =
      (save (TPTSVF4Pole.prepareCoefficients spec s).1, (TPTSVF4Pole.prepareCoefficients spec s).2) := rfl

def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTSVF4Pole.Runtime

end DSP
