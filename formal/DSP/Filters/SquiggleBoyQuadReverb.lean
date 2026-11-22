import DSP.Filters.SchroederAllPass.FilterProofs
import DSP.Filters.SchroederAllPass.Wandering
import Mathlib.Data.Fin.VecNotation

namespace DSP

/-- The short and long input diffusion taps in 192 kHz samples. -/
def squiggleBoyReverbIntroAPFsDelayTime : Fin 2 → Fin 4 → Nat :=
  ![![293, 487, 907, 1433], ![4493, 6899, 9811, 14033]]

def squiggleBoyReverbShortCapacities : List Nat := [2048, 2048, 2048, 2048]
def squiggleBoyReverbLongCapacities : List Nat := [8192, 8192, 16384, 16384]

def squiggleBoyReverbIntroAPFCapacity (branch : Fin 2) (stage : Fin 4) : Nat :=
  if branch = 0 then squiggleBoyReverbShortCapacities[stage.val]'stage.isLt
  else squiggleBoyReverbLongCapacities[stage.val]'stage.isLt

abbrev SquiggleBoyQuadReverbIntroAPFs (R : Type) :=
  ParallelSchroederAllPassCascades squiggleBoyReverbShortCapacities squiggleBoyReverbLongCapacities 4 R

def squiggleBoyQuadReverbIntroAPFs {R : Type} [OfNat R 0] (gain : Fin 4 → R) :
    SquiggleBoyQuadReverbIntroAPFs R where
  short :=
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 0) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 1) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 2) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 3) (by decide) (by decide) gain) <|
    .passthrough (fun _ => 0)
  long :=
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 0) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 1) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 2) (by decide) (by decide) gain) <|
    .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 3) (by decide) (by decide) gain) <|
    .passthrough (fun _ => 0)

theorem squiggleBoyQuadReverbIntroAPFs_realizes {K : Type} [Field K] (gain : Fin 4 → K) :
    let bank := squiggleBoyQuadReverbIntroAPFs gain
    Channels.Realizes ParallelSchroederAllPassCascades.step bank ParallelSchroederAllPassCascades.y
      (bank.response Bilinear.const Bilinear.delay) := by
  apply ParallelSchroederAllPassCascades.realizes <;>
    simp [squiggleBoyQuadReverbIntroAPFs, SchroederAllPassCascade.AtRest]

/-- The mean velocity draw is one cycle per 1,920,000 samples. -/
def squiggleBoyReverbLambdaSamples : Nat := 1920000

def squiggleBoyReverbWanderSpec (enabled : Bool) : WanderingSchroederAllPassGeneric.WanderSpec :=
  ⟨if enabled then 128 else 0, Float.ofNat squiggleBoyReverbLambdaSamples⟩

theorem squiggleBoyReverbIntroWanderFits (enabled : Bool) (branch : Fin 2) (stage : Fin 4) :
    DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity branch stage)
      (squiggleBoyReverbIntroAPFsDelayTime branch stage) (squiggleBoyReverbWanderSpec enabled).depth := by
  have all : ∀ (e : Bool) (b : Fin 2) (s : Fin 4),
      DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity b s)
        (squiggleBoyReverbIntroAPFsDelayTime b s) (squiggleBoyReverbWanderSpec e).depth := by decide
  exact all enabled branch stage

def squiggleBoyReverbIntroSeed (branch : Fin 2) (stage : Fin 4) : UInt64 :=
  UInt64.ofNat (branch.val * 4 + stage.val + 1)

def squiggleBoyReverbLoopSeed (stage : Fin 2) : UInt64 :=
  UInt64.ofNat (9 + stage.val)

structure SquiggleBoyWanderingIntroAPFs (R : Type) where
  short : WanderingSchroederAllPassCascade 4 R squiggleBoyReverbShortCapacities
  long : WanderingSchroederAllPassCascade 4 R squiggleBoyReverbLongCapacities

namespace SquiggleBoyWanderingIntroAPFs

def init {R : Type} [OfNat R 0] [SmoothRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (enabled : Bool) (gain : Fin 4 → R) : SquiggleBoyWanderingIntroAPFs R :=
  let spec := squiggleBoyReverbWanderSpec enabled
  { short :=
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 0) gain spec (squiggleBoyReverbIntroSeed 0 0)
        (squiggleBoyReverbIntroWanderFits enabled 0 0)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 1) gain spec (squiggleBoyReverbIntroSeed 0 1)
        (squiggleBoyReverbIntroWanderFits enabled 0 1)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 2) gain spec (squiggleBoyReverbIntroSeed 0 2)
        (squiggleBoyReverbIntroWanderFits enabled 0 2)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 0 3) gain spec (squiggleBoyReverbIntroSeed 0 3)
        (squiggleBoyReverbIntroWanderFits enabled 0 3)) <|
      .passthrough (fun _ => 0)
    long :=
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 0) gain spec (squiggleBoyReverbIntroSeed 1 0)
        (squiggleBoyReverbIntroWanderFits enabled 1 0)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 1) gain spec (squiggleBoyReverbIntroSeed 1 1)
        (squiggleBoyReverbIntroWanderFits enabled 1 1)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 2) gain spec (squiggleBoyReverbIntroSeed 1 2)
        (squiggleBoyReverbIntroWanderFits enabled 1 2)) <|
      .series (.uniform (squiggleBoyReverbIntroAPFsDelayTime 1 3) gain spec (squiggleBoyReverbIntroSeed 1 3)
        (squiggleBoyReverbIntroWanderFits enabled 1 3)) <|
      .passthrough (fun _ => 0) }

def y {R : Type} [Add R] [Div R] [OfNat R 2] (bank : SquiggleBoyWanderingIntroAPFs R) : Fin 4 → R :=
  fun ch => ParallelSchroederAllPassCascades.mixOutputs (bank.short.y ch) (bank.long.y ch)

def step {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [SmoothRandomLFO.SampleValue R]
    (bank : SquiggleBoyWanderingIntroAPFs R) (input : Sample (Fin 4 → R)) :
    SquiggleBoyWanderingIntroAPFs R :=
  ⟨bank.short.step input, bank.long.step input⟩

end SquiggleBoyWanderingIntroAPFs
end DSP
