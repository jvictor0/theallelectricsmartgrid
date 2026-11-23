import DSP.Filters.NaiveReuppingLadder.Contracts
import DSP.ResponseMeasurement.Approximation

/-! The chosen response target: drive the reset filter by a positive-amplitude
cosine, with input phase zero. Parameters are fixed throughout the measurement.
This defines the detector-disabled exact-tanh mathematical model only; it does
not change production processing or assert that an HB branch is selected. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.ResponseGoal
open DSP.ResponseMeasurement

def carrier (frequency : ℝ) : ℂ := Complex.exp (Complex.I*(2*Real.pi*frequency))

def input (amplitude frequency : ℝ) (n : ℕ) : Sample ℝ :=
  ⟨amplitude*(carrier frequency^n).re⟩

/-- The sample at index n is observed AFTER processing input n. -/
def resetOutput (prepared : NaiveReuppingLadder ℝ) (amplitude frequency : ℝ) (n : ℕ) : ℝ :=
  (Step.states step (input amplitude frequency)
    (reset {prepared with dampingScale := 0}) (n+1)).output

/-- For each positive slack, ALL sufficiently long measurements must fit
the stated band. Neither pointwise settling nor existence of a unique long-run
average is required. DC and Nyquist are excluded for the sinusoid normalization. -/
def Meets (prepared : NaiveReuppingLadder ℝ) (amplitude frequency : ℝ)
    (prediction : Prediction) (coherentError powerError : ℝ) : Prop :=
  0 < amplitude ∧ 0 < frequency ∧ frequency < 1/2 ∧
    0 ≤ coherentError ∧ 0 ≤ powerError ∧
    Approximates (resetOutput prepared amplitude frequency) (carrier frequency)
      prediction coherentError powerError

/-- A direct route from reset to the requested goal. The correction sequences
may oscillate forever; their boundedness makes their boundary contributions
vanish in the average. Uniform residual bounds still need to be established
along this reset trajectory (for example by proving a reachable-state region). -/
theorem meets_of_coboundaries (prepared : NaiveReuppingLadder ℝ)
    (amplitude frequency : ℝ) (ha : 0 < amplitude) (hf : 0 < frequency) (hf1 : frequency < 1/2)
    (prediction : Prediction) (coherentError powerError : ℝ)
    (he : 0 ≤ coherentError) (hp : 0 ≤ powerError)
    (coherentCorrection : ℕ → ℂ) (powerCorrection : ℕ → ℝ) (coherentBound powerBound : ℝ)
    (hcoh : ∀ n, ‖2*((resetOutput prepared amplitude frequency n : ℂ)*star (carrier frequency)^n)-
      prediction.coherentPeak-(coherentCorrection n-coherentCorrection (n+1))‖ ≤ coherentError)
    (hpow : ∀ n, |resetOutput prepared amplitude frequency n^2-prediction.totalPower-
      (powerCorrection n-powerCorrection (n+1))| ≤ powerError)
    (hbc : ∀ n, ‖coherentCorrection n‖ ≤ coherentBound)
    (hbp : ∀ n, |powerCorrection n| ≤ powerBound) :
    Meets prepared amplitude frequency prediction coherentError powerError := by
  refine ⟨ha,hf,hf1,he,hp,?_,?_⟩
  · have h := eventuallyNear_of_coboundary _ coherentCorrection prediction.coherentPeak
      coherentError coherentBound hcoh hbc
    have heq (N : ℕ) : DSP.Coboundary.average
        (fun n => 2*((resetOutput prepared amplitude frequency n : ℂ)*star (carrier frequency)^n)) N =
        coherent (carrier frequency) (resetOutput prepared amplitude frequency) N := by
      simp only [DSP.Coboundary.average,coherent,average,← Finset.mul_sum,Complex.real_smul]
      ring
    change EventuallyNear (fun N => DSP.Coboundary.average _ N) _ _ at h
    simpa only [heq] using h
  · exact eventuallyNear_of_coboundary _ powerCorrection prediction.totalPower
      powerError powerBound hpow hbp

end DSP.NaiveReuppingLadder.ResponseGoal
