import DSP.DelayBuffer.Cubic
import DSP.Filters.NormalizedTanh
import DSP.Filters.SquiggleBoyQuadReverb
import DSP.Filters.SquiggleBoyQuadReverb.Mixing
import DSP.Filters.Tuning

namespace DSP

/-- Feedback-chain taps in 192 kHz samples, from the 30× and 20× targets. -/
def squiggleBoyReverbLoopAPFsDelayTime : Fin 2 → Fin 4 → Nat :=
  ![![2039, 3719, 6359, 2753], ![11923, 16879, 9041, 23447]]

def squiggleBoyReverbDelayCapacity : Nat := 262144
def squiggleBoyReverbFirstCapacity : Nat := 8192
def squiggleBoyReverbSecondCapacity : Nat := 32768

def squiggleBoyReverbLoopAPFCapacity (stage : Fin 2) : Nat :=
  if stage = 0 then squiggleBoyReverbFirstCapacity else squiggleBoyReverbSecondCapacity

theorem squiggleBoyReverbLoopWanderFits (enabled : Bool) (stage : Fin 2) (channel : Fin 4) :
    DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity stage)
      (squiggleBoyReverbLoopAPFsDelayTime stage channel) (squiggleBoyReverbWanderSpec enabled).depth := by
  have all : ∀ (e : Bool) (s : Fin 2) (ch : Fin 4),
      DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity s)
        (squiggleBoyReverbLoopAPFsDelayTime s ch) (squiggleBoyReverbWanderSpec e).depth := by decide
  exact all enabled stage channel

structure SquiggleBoyQuadReverb (R : Type) where
  private mk ::
  private diffusion : SquiggleBoyWanderingIntroAPFs R
  private delay : DelayBuffer squiggleBoyReverbDelayCapacity (Fin 4 → R)
  private delaySamples : Fin 4 → R
  private feedback : Fin 4 → R
  private saturator : NormalizedTanh.Coefficients (Fin 4 → R)
  private firstAllPass : WanderingSchroederAllPassGeneric squiggleBoyReverbFirstCapacity 4 R
  private damping : TPTBaseWidth (Fin 4 → R)
  private secondAllPass : WanderingSchroederAllPassGeneric squiggleBoyReverbSecondCapacity 4 R

namespace SquiggleBoyQuadReverb

structure LogicalSpec (R : Type) where
  delaySamples : Fin 4 → R
  feedback : Fin 4 → R
  dampingBase : Fin 4 → R
  dampingWidth : Fin 4 → R

def dampingCutoffs {R : Type} [OfScientific R] [Mul R] [IfLess R]
    (base width : R) : R × R :=
  (minValue 0.499 base, minValue 0.499 (base * width))

def tuneDamping {R : Type} [OfNat R 1] [OfScientific R] [Add R] [Mul R] [Div R]
    [Trig R] [IfLess R] (damping : TPTBaseWidth R) (base width : R) : TPTBaseWidth R :=
  let cutoffs := dampingCutoffs base width
  TPTBaseWidth.computeCoefficients ⟨⟨cutoffs.1⟩, ⟨cutoffs.2⟩⟩ damping

/-- Install the current physical settings while retaining the audio histories. -/
def computeCoefficients {R : Type} [OfNat R 1] [OfScientific R] [Add R] [Mul R] [Div R]
    [Trig R] [IfLess R] (spec : LogicalSpec R) (state : SquiggleBoyQuadReverb R) :
    SquiggleBoyQuadReverb R :=
  let damping := tuneDamping state.damping spec.dampingBase spec.dampingWidth
  { state with delaySamples := spec.delaySamples, feedback := spec.feedback, damping }

def allPassGain {R : Type} [OfScientific R] : R := 0.6
def saturatorSpec {R : Type} [OfScientific R] : NormalizedTanh.LogicalSpec R := ⟨0.5⟩

def drive {R : Type} [Add R] [Mul R] [Div R] [Tanh R] (saturator : NormalizedTanh.Coefficients R)
    (input previous feedback : R) : R :=
  NormalizedTanh.apply saturator (input + previous * feedback)

def pushDelay {capacity : Nat} {R : Type} (delay : DelayBuffer capacity (Fin 4 → R))
    (sample : Fin 4 → R) : DelayBuffer capacity (Fin 4 → R) :=
  let channels := Vector.ofFn sample
  delay.push (fun ch => channels[ch])

def stepDamping {R : Type} [Sub R] [FMA R] (state : TPTBaseWidth (Fin 4 → R))
    (sample : Sample (Fin 4 → R)) : TPTBaseWidth (Fin 4 → R) :=
  let next := state.step sample
  let store := fun (f : OnePoleTPTLowPass (Fin 4 → R)) =>
    let history := Vector.ofFn f.s
    let output := Vector.ofFn f.y
    { f with s := fun ch => history[ch], y := fun ch => output[ch] }
  let highPass := Vector.ofFn next.highPass.y
  { highPass := ⟨store next.highPass.lowPass, fun ch => highPass[ch]⟩
    lowPass := store next.lowPass }

def dampedSample {R : Type} (damping : TPTBaseWidth R) : R := damping.lowPass.y

def init {R : Type} [OfNat R 0] [OfNat R 1] [OfScientific R] [Add R] [Mul R] [Div R]
    [Trig R] [IfLess R] [Tanh R] [SmoothRandomLFO.SampleValue R]
    [Sub R] [FMA R] (spec : LogicalSpec R)
    (wandering : Bool := true) : SquiggleBoyQuadReverb R :=
  let gain : Fin 4 → R := allPassGain
  let zero : Fin 4 → R := fun _ => 0
  let wander := squiggleBoyReverbWanderSpec wandering
  computeCoefficients spec {
    diffusion := SquiggleBoyWanderingIntroAPFs.init wandering gain
    delay := DelayBuffer.empty (by decide)
    delaySamples := zero
    feedback := zero
    saturator := NormalizedTanh.prepare saturatorSpec
    firstAllPass := WanderingSchroederAllPassGeneric.init (squiggleBoyReverbLoopAPFsDelayTime 0)
      gain wander (squiggleBoyReverbLoopSeed 0) (squiggleBoyReverbLoopWanderFits wandering 0) (by decide)
    damping := ⟨⟨⟨zero, zero, zero⟩, zero⟩, ⟨zero, zero, zero⟩⟩
    secondAllPass := WanderingSchroederAllPassGeneric.init (squiggleBoyReverbLoopAPFsDelayTime 1)
      gain wander (squiggleBoyReverbLoopSeed 1) (squiggleBoyReverbLoopWanderFits wandering 1) (by decide) }

def y {R : Type} (state : SquiggleBoyQuadReverb R) : Fin 4 → R := state.secondAllPass.y

def step {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [Tanh R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [SmoothRandomLFO.SampleValue R]
    (state : SquiggleBoyQuadReverb R) (input : Sample (Fin 4 → R)) : SquiggleBoyQuadReverb R :=
  let diffusion := state.diffusion.step input
  let delay := pushDelay state.delay (drive state.saturator diffusion.y state.secondAllPass.y state.feedback)
  let mixed := squiggleBoyReverbMix (delay.readCubic state.delaySamples)
  let firstAllPass := state.firstAllPass.step ⟨mixed⟩
  let damping := stepDamping state.damping ⟨firstAllPass.y⟩
  let secondAllPass := state.secondAllPass.step ⟨dampedSample damping⟩
  { state with diffusion, delay, firstAllPass, damping, secondAllPass }

def process {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfNat R 3] [OfNat R 4] [OfNat R 5]
    [OfScientific R] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [Tanh R] [IfLess R]
    [DelayMath R] [DelayBuffer.SampleIndex R] [SmoothRandomLFO.SampleValue R]
    (state : SquiggleBoyQuadReverb R) (input : Sample (Fin 4 → R)) :
    SquiggleBoyQuadReverb R × (Fin 4 → R) :=
  let next := state.step input
  (next, next.y)

end SquiggleBoyQuadReverb
end DSP
