import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Residual
import DSP.ResponseMeasurement.FiniteSpectrum

/-! A finite spectral response curve with an explicit error against FULL
harmonic balance. The existence theorem is conditional on the full equation
solver certificate. It does not identify the solution reached from reset.
For drifting responses a free oscillator phase must first be fixed; a fixed-
frequency root solver cannot simply assume away its neutral phase direction. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance
open DSP.Harmonics DSP.ResponseMeasurement Metric UnitAddTorus
open scoped NNReal
variable {d ι : Type*} [Fintype d] [Fintype ι]

/-- Peaks, not half-amplitude Fourier coefficients. Indices can include
input harmonics, an independent oscillator, and their mixed frequencies. -/
def finiteWaveform (peak : ι → ℂ) (mode : ι → d → ℤ) : Waveform d :=
  ∑ i, realMode (peak i) (mode i)

def spectralPeak (peak : ι → ℂ) (mode : ι → d → ℤ) (phase : Phase d) : ι → ℂ :=
  fun i => peak i*mFourier (mode i) phase

def spectralCarrier (mode : ι → d → ℤ) (increment : Phase d) : ι → ℂ :=
  fun i => mFourier (mode i) increment

def responseCurve (peak : ι → ℂ) (mode : ι → d → ℤ)
    (increment phase : Phase d) (target : ℂ) : Prediction :=
  FiniteSpectrum.prediction (spectralPeak peak mode phase) (spectralCarrier mode increment) target

theorem finiteWaveform_samples (peak : ι → ℂ) (mode : ι → d → ℤ)
    (increment phase : Phase d) (n : ℕ) :
    finiteWaveform peak mode (phase+n • increment) =
      FiniteSpectrum.signal (spectralPeak peak mode phase) (spectralCarrier mode increment) n := by
  simp only [finiteWaveform,ContinuousMap.sum_apply,realMode,ContinuousMap.coe_mk,
    character_add,character_nsmul,FiniteSpectrum.signal,spectralPeak,spectralCarrier,mul_assoc]

theorem spectral_amplitudeBound (peak : ι → ℂ) (mode : ι → d → ℤ) (phase : Phase d) :
    FiniteSpectrum.amplitudeBound (spectralPeak peak mode phase) =
      FiniteSpectrum.amplitudeBound peak := by
  simp only [FiniteSpectrum.amplitudeBound,spectralPeak,norm_mul,character_norm,mul_one]

/-- The candidate's final saturated output has its OWN approximation tail.
Approximating four histories does not permit silently dropping output harmonics. -/
theorem response_error (s : NaiveReuppingLadder ℝ) (solution candidate : WaveState d)
    (peak : ι → ℂ) (mode : ι → d → ℤ) (increment phase : Phase d)
    (target : ℂ) (ht : ‖target‖ = 1) (stateError tailError : ℝ)
    (hs : dist candidate solution ≤ stateError)
    (htail : ∀ θ, |output s candidate.orbit θ-finiteWaveform peak mode θ| ≤ tailError) :
    let e := outputLipschitz s*stateError+tailError
    Approximates (fun n => output s solution.orbit (phase+n • increment)) target
      (responseCurve peak mode increment phase target) (2*e)
      (e*(2*FiniteSpectrum.amplitudeBound peak+e)) := by
  have hL : 0 ≤ outputLipschitz s := by unfold outputLipschitz; positivity
  have he (n : ℕ) : |output s solution.orbit (phase+n • increment)-
      FiniteSpectrum.signal (spectralPeak peak mode phase) (spectralCarrier mode increment) n| ≤
        outputLipschitz s*stateError+tailError := by
    rw [← finiteWaveform_samples]
    apply (abs_sub_le _ (output s candidate.orbit (phase+n • increment)) _).trans
    apply add_le_add _ (htail _)
    apply (output_difference_bound s solution candidate _).trans
    rw [dist_comm]
    exact mul_le_mul_of_nonneg_left hs hL
  simpa only [responseCurve,spectral_amplitudeBound] using
    FiniteSpectrum.approximates _ (spectralPeak peak mode phase) (spectralCarrier mode increment)
      (fun i => character_norm _ _) target ht _ he

/-- Apply the bound to the original processor initialized on the exact HB
orbit. The first observation is one phase increment after the initial state,
matching `Step.states`; no reset or attraction premise is smuggled in. -/
theorem trajectory_response_error (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (solution candidate : WaveState d) (increment phase : Phase d) (input : Waveform d)
    (hb : Balance s increment input solution.orbit)
    (peak : ι → ℂ) (mode : ι → d → ℤ) (target : ℂ) (ht : ‖target‖ = 1)
    (stateError tailError : ℝ) (hs : dist candidate solution ≤ stateError)
    (htail : ∀ θ, |output s candidate.orbit θ-finiteWaveform peak mode θ| ≤ tailError) :
    let e := outputLipschitz s*stateError+tailError
    Approximates (fun n => (trajectory s increment input solution.orbit phase (n+1)).output)
      target (responseCurve peak mode increment (phase+increment) target) (2*e)
      (e*(2*FiniteSpectrum.amplitudeBound peak+e)) := by
  have h := response_error s solution candidate peak mode increment (phase+increment) target ht
    stateError tailError hs htail
  have he (n : ℕ) : (trajectory s increment input solution.orbit phase (n+1)).output =
      output s solution.orbit ((phase+increment)+n • increment) := by
    rw [balance_trajectory_output s hd increment input solution.orbit hb]
    congr 1
    rw [add_nsmul,one_nsmul]
    abel
  simpa only [Approximates,he] using h

/-- Full residual -> nearby exact HB solution -> measured response error.
This proves existence near the chosen branch, not reset selection or a
certificate for every knob setting. In particular `hc` is a substantive premise. -/
theorem exists_balance_response (s : NaiveReuppingLadder ℝ) (increment phase : Phase d)
    (input : Waveform d) (inverse : WaveState d →L[ℝ] WaveState d)
    (hi : Function.Injective inverse) (candidate : WaveState d)
    (radius defect : ℝ) (q : ℝ≥0) (hr : 0 ≤ radius) (hq : q < 1)
    (hc : LipschitzOnWith q (ResidualValidation.correction (residual s increment input) inverse)
      (closedBall candidate radius))
    (hd : ‖inverse (residual s increment input candidate)‖ ≤ defect)
    (htrap : defect+q*radius ≤ radius)
    (peak : ι → ℂ) (mode : ι → d → ℤ) (tailError : ℝ)
    (htail : ∀ θ, |output s candidate.orbit θ-finiteWaveform peak mode θ| ≤ tailError)
    (target : ℂ) (ht : ‖target‖ = 1) :
    ∃ solution : WaveState d, Balance s increment input solution.orbit ∧
      let e := outputLipschitz s*(defect/(1-q))+tailError
      Approximates (fun n => output s solution.orbit (phase+n • increment)) target
        (responseCurve peak mode increment phase target) (2*e)
        (e*(2*FiniteSpectrum.amplitudeBound peak+e)) := by
  obtain ⟨solution,_,hb,he⟩ := exists_balance_near s increment input inverse hi candidate
    radius defect q hr hq hc hd htrap
  exact ⟨solution,hb,response_error s solution candidate peak mode increment phase target ht
    (defect/(1-q)) tailError he htail⟩

end DSP.NaiveReuppingLadder.HarmonicBalance
