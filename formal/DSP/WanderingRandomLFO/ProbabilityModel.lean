import DSP.SmoothRandomLFO.Proofs
import DSP.IfLess.Real
import DSP.WanderingRandomLFO
import Mathlib.MeasureTheory.Constructions.Pi
import Mathlib.MeasureTheory.Integral.IntervalIntegral.Periodic
import Mathlib.Probability.Distributions.Exponential

namespace DSP.WanderingRandomLFO

open SmoothRandomLFO MeasureTheory ProbabilityTheory Set
open scoped ENNReal

noncomputable section

abbrev Phase := AddCircle (1 : ℝ)

instance : Fact (0 < (1 : ℝ)) := ⟨zero_lt_one⟩

instance : IsProbabilityMeasure (volume : Measure Phase) :=
  ⟨by rw [AddCircle.measure_univ]; norm_num⟩

/-- Each knot and the stationary fractional phase have this uniform law. -/
def unitUniform : Measure ℝ := volume.restrict (Ico 0 1)

instance : IsProbabilityMeasure unitUniform :=
  ⟨by simp [unitUniform, Real.volume_Ico]⟩

def knotUniforms : Measure (Fin 4 → ℝ) := Measure.pi (fun _ : Fin 4 => unitUniform)

instance : IsProbabilityMeasure knotUniforms := by
  unfold knotUniforms
  infer_instance

def fractionalPhase (p : Phase) : ℝ :=
  (AddCircle.measurableEquivIco 1 0 p).val

theorem measurable_fractionalPhase : Measurable fractionalPhase :=
  measurable_subtype_coe.comp (AddCircle.measurableEquivIco 1 0).measurable

/-- Spline on uniform knots, with the same coefficient preparation used by
both generated processors. -/
def normalizedSpline (t : ℝ) (p : Fin 4 → ℝ) : ℝ :=
  evaluate (prepare (unitSegment (p 0) (p 1) (p 2) (p 3))) t

theorem normalizedSpline_eq (t : ℝ) (p : Fin 4 → ℝ) :
    normalizedSpline t p = ((1 - t)^3 * p 0 + (4 - 6*t^2 + 3*t^3) * p 1 +
      (1 + 3*t + 3*t^2 - 3*t^3) * p 2 + t^3 * p 3) / 6 := by
  norm_num [normalizedSpline, unitSegment, evaluate, prepare, Segment.leftSlope, Segment.slope,
    Segment.rightSlope, Segment.leftCurve, Segment.rightCurve]
  ring

def bipolarSample (x : Phase × (Fin 4 → ℝ)) : ℝ :=
  2 * normalizedSpline (fractionalPhase x.1) x.2 - 1

theorem measurable_bipolarSample : Measurable bipolarSample := by
  unfold bipolarSample
  simp_rw [normalizedSpline_eq]
  have hp := measurable_fractionalPhase.comp (measurable_fst :
    Measurable (Prod.fst : Phase × (Fin 4 → ℝ) → Phase))
  fun_prop

/-- A one-sample law is a product with four fresh uniform coordinates. Given
the clock's knot index, the four selected entries of an independent iid knot
stream still have this product law. -/
def amplitudeLaw (phaseLaw : Measure Phase) : Measure ℝ :=
  (phaseLaw.prod knotUniforms).map bipolarSample

instance (phaseLaw : Measure Phase) [IsProbabilityMeasure phaseLaw] :
    IsProbabilityMeasure (amplitudeLaw phaseLaw) :=
  Measure.isProbabilityMeasure_map measurable_bipolarSample.aemeasurable

def stationaryAmplitudeLaw : Measure ℝ := amplitudeLaw volume

/-- The exact real smoothing constant, using the production frame length. -/
def exactAlpha : ℝ := Real.exp (-(frameSamples : ℝ) / (speedSmoothingSamples : ℝ))

theorem exactAlpha_pos : 0 < exactAlpha := Real.exp_pos _

theorem exactAlpha_lt_one : exactAlpha < 1 :=
  Real.exp_lt_one_iff.mpr (by norm_num [frameSamples, speedSmoothingSamples])

/-- Clock speed after n updates. The inputs are uncapped exponential draws. -/
def exactSpeed (lambda alpha : ℝ) (draw : ℕ → ℝ) : ℕ → ℝ
  | 0 => 1 / max 1 lambda
  | n+1 => smoothVelocity alpha (exactSpeed lambda alpha draw n) (min 1 (draw n))

/-- Unwrapped phase at the start of a frame. -/
def exactFramePhase (lambda alpha : ℝ) (draw : ℕ → ℝ) : ℕ → ℝ
  | 0 => 0
  | n+1 => phaseAfter (exactFramePhase lambda alpha draw n) (exactSpeed lambda alpha draw (n+1))
      (frameSamples : ℝ)

def exactSamplePhase (lambda alpha : ℝ) (draw : ℕ → ℝ) (n : ℕ) (i : ℝ) : ℝ :=
  phaseAfter (exactFramePhase lambda alpha draw n) (exactSpeed lambda alpha draw (n+1)) i

/-- A finite prefix of the iid exponential clock suffices for the sample. -/
def phaseDraws (lambda : ℝ) (n : ℕ) : Measure (Fin (n+1) → ℝ) :=
  Measure.pi (fun _ => expMeasure (max 1 lambda))

instance (lambda : ℝ) (n : ℕ) : IsProbabilityMeasure (phaseDraws lambda n) := by
  have h : 0 < max 1 lambda := zero_lt_one.trans_le (le_max_left _ _)
  letI : IsProbabilityMeasure (expMeasure (max 1 lambda)) := isProbabilityMeasure_expMeasure h
  unfold phaseDraws
  infer_instance

def samplePhaseTime (lambda alpha : ℝ) (n : ℕ) (i : ℝ) (draw : Fin (n+1) → ℝ) : ℝ :=
  exactSamplePhase lambda alpha (fun j => draw (Fin.ofNat (n+1) j)) n i

def samplePhaseAngle (lambda alpha : ℝ) (n : ℕ) (i : ℝ) (draw : Fin (n+1) → ℝ) : Phase :=
  (samplePhaseTime lambda alpha n i draw : Phase)

theorem measurable_exactSpeed (lambda alpha : ℝ) {Ω : Type*} [MeasurableSpace Ω]
    (draw : ℕ → Ω → ℝ) (hd : ∀ j, Measurable (draw j)) (n : ℕ) :
    Measurable (fun x => exactSpeed lambda alpha (fun j => draw j x) n) := by
  induction n with
  | zero => exact measurable_const
  | succ n ih =>
    simp only [exactSpeed, smoothVelocity, fma_eq]
    fun_prop

theorem measurable_exactFramePhase (lambda alpha : ℝ) {Ω : Type*} [MeasurableSpace Ω]
    (draw : ℕ → Ω → ℝ) (hd : ∀ j, Measurable (draw j)) (n : ℕ) :
    Measurable (fun x => exactFramePhase lambda alpha (fun j => draw j x) n) := by
  induction n with
  | zero => exact measurable_const
  | succ n ih =>
    simp only [exactFramePhase, phaseAfter]
    have hv := measurable_exactSpeed lambda alpha draw hd (n+1)
    fun_prop

theorem measurable_samplePhaseTime (lambda alpha : ℝ) (n : ℕ) (i : ℝ) :
    Measurable (samplePhaseTime lambda alpha n i) := by
  let draw : ℕ → (Fin (n+1) → ℝ) → ℝ := fun j x => x (Fin.ofNat (n+1) j)
  have hd (j : ℕ) : Measurable (draw j) := measurable_pi_apply _
  have hp := measurable_exactFramePhase lambda alpha draw hd n
  have hv := measurable_exactSpeed lambda alpha draw hd (n+1)
  unfold samplePhaseTime exactSamplePhase phaseAfter
  exact hp.add (hv.mul_const i)

theorem measurable_samplePhaseAngle (lambda alpha : ℝ) (n : ℕ) (i : ℝ) :
    Measurable (samplePhaseAngle lambda alpha n i) :=
  AddCircle.measurable_mk'.comp (measurable_samplePhaseTime lambda alpha n i)

def samplePhaseLaw (lambda alpha : ℝ) (n : ℕ) (i : ℝ) : Measure Phase :=
  (phaseDraws lambda n).map (samplePhaseAngle lambda alpha n i)

instance (lambda alpha : ℝ) (n : ℕ) (i : ℝ) :
    IsProbabilityMeasure (samplePhaseLaw lambda alpha n i) :=
  Measure.isProbabilityMeasure_map (measurable_samplePhaseAngle lambda alpha n i).aemeasurable

/-- Time is counted in engine samples, using the frame offset for each sample. -/
def everySampleLaw (lambda : ℝ) (sample : ℕ) : Measure ℝ :=
  amplitudeLaw (samplePhaseLaw lambda exactAlpha (sample / frameSamples)
    ((sample % frameSamples : ℕ) : ℝ))

end

end DSP.WanderingRandomLFO
