import DSP.Filters.MultibandSaturator.Exports
import DSP.Filters.LinkwitzRileySplitter.Runtime

namespace DSP.MultibandSaturator.Runtime

structure State (numBands : Nat) (R : Type) where
  splitter : LinkwitzRileySplitter.Runtime.State numBands R
  bandGain : Fin numBands → R
  masterGain : R

def save {n : Nat} {R : Type} (s : MultibandSaturator.State n R) : State n R :=
  ⟨LinkwitzRileySplitter.Runtime.save s.splitter, s.bandGain, s.masterGain⟩

def restore {n : Nat} {R : Type} [OfNat R 0] (s : State n R) : MultibandSaturator.State n R :=
  ⟨LinkwitzRileySplitter.Runtime.restore s.splitter, s.bandGain, s.masterGain⟩

section Processing
variable {R : Type} [OfNat R 0] [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [FMA R]
  [Tanh R] [ChannelMean R]

def process4Band : Processor (State 4 R) (Input R) (Output 4 R) :=
  Processor.compact step4Band save restore

theorem simulation : Processor.Simulation (step4Band (R := R)) process4Band (fun s m => m = save s) := by
  apply Processor.compact_simulation
  intro s x
  simp only [step4Band, stepWith, restore, save, LinkwitzRileySplitter.Runtime.step_restore]

/-- Every audio and meter observation matches the proved model for any input stream. -/
theorem output_stream (u : Nat → Input R) (s : MultibandSaturator.State 4 R) :
    Processor.run step4Band u s = Processor.run process4Band u (save s) :=
  Processor.run_simulation _ _ _ simulation u s (save s) rfl

end Processing

def prepare4Band {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec 4 R) (s : State 4 R) : State 4 R × Snapshot 4 R :=
  let result := MultibandSaturator.prepare4Band spec (restore s)
  (save result.1, result.2)

def install4Band {R : Type} [OfNat R 0] (spec : PreparedSpec 4 R) (s : State 4 R) : State 4 R × Snapshot 4 R :=
  (save (MultibandSaturator.installPrepared spec (restore s)), spec.snapshot)

def reset4Band {R : Type} [OfNat R 0] (s : State 4 R) : State 4 R :=
  save (MultibandSaturator.reset (restore s))

end DSP.MultibandSaturator.Runtime
