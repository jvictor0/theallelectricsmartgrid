import DSP.ResponseMeasurement.ErrorBounds
import DSP.Analysis.Coboundary

/-! A response approximation means that every sufficiently long measurement
lies within an explicit error budget, with arbitrary positive slack. It does
not require a limiting trajectory or a limiting measured value. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter
open scoped Topology

def EventuallyNear {E : Type*} [NormedAddCommGroup E]
    (measurement : ℕ → E) (prediction : E) (error : ℝ) : Prop :=
  ∀ slack > 0, ∀ᶠ N in atTop, ‖measurement N-prediction‖ ≤ error+slack

structure Prediction where
  coherentPeak : ℂ
  totalPower : ℝ

def Prediction.coherentPower (p : Prediction) : ℝ := ‖p.coherentPeak‖^2/2
def Prediction.coherentGainSquared (p : Prediction) (amplitude : ℝ) : ℝ :=
  ‖p.coherentPeak‖^2/amplitude^2
def Prediction.totalGainSquared (p : Prediction) (amplitude : ℝ) : ℝ :=
  2*p.totalPower/amplitude^2

/-- Explicit phasor and total-power budgets, before input normalization. -/
def Approximates (signal : ℕ → ℝ) (carrier : ℂ) (prediction : Prediction)
    (coherentError powerError : ℝ) : Prop :=
  EventuallyNear (coherent carrier signal) prediction.coherentPeak coherentError ∧
  EventuallyNear (meanSquare signal) prediction.totalPower powerError

theorem eventuallyNear_of_bound {E : Type*} [NormedAddCommGroup E]
    (measurement : ℕ → E) (prediction : E) (error : ℝ) (tail : ℕ → ℝ)
    (hb : ∀ᶠ N in atTop, ‖measurement N-prediction‖ ≤ error+tail N)
    (ht : Tendsto tail atTop (𝓝 0)) : EventuallyNear measurement prediction error := by
  intro slack hslack
  filter_upwards [hb,ht.eventually (gt_mem_nhds hslack)] with N hN htail
  exact hN.trans (by linarith)

theorem eventuallyNear_of_coboundary {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]
    (signal potential : ℕ → E) (prediction : E) (error M : ℝ)
    (he : ∀ n, ‖signal n-prediction-(potential n-potential (n+1))‖ ≤ error)
    (hp : ∀ n, ‖potential n‖ ≤ M) :
    EventuallyNear (DSP.Coboundary.average signal) prediction error := by
  intro slack hslack
  filter_upwards [DSP.Coboundary.eventually_bound signal potential prediction error M he hp slack hslack]
    with N hN
  exact hN.le

/-- Transfer an exact reference response to an imperfect waveform. The
actual waveform need not converge, even though the finite reference does. -/
theorem approximates_of_uniform_waveform_error (signal reference : ℕ → ℝ)
    (carrier : ℂ) (hc : ‖carrier‖ = 1) (prediction : Prediction) (error B : ℝ)
    (he : ∀ n, |signal n-reference n| ≤ error) (hB : ∀ n, |reference n| ≤ B)
    (hcoh : HasCoherentResponse carrier reference prediction.coherentPeak)
    (hpower : HasMeanSquare reference prediction.totalPower) :
    Approximates signal carrier prediction (2*error) (error*(2*B+error)) := by
  have he0 : 0 ≤ error := (abs_nonneg _).trans (he 0)
  constructor
  · apply eventuallyNear_of_bound _ _ _ (fun N => ‖coherent carrier reference N-prediction.coherentPeak‖)
    · filter_upwards [eventually_gt_atTop 0] with N hN
      have h := coherent_error_bound carrier hc signal reference (fun _ => error) N (fun n _ => he n)
      have hn : (N:ℝ) ≠ 0 := by positivity
      have hs : 2*(N:ℝ)⁻¹*(∑ _n ∈ Finset.range N, error) = 2*error := by
        simp only [Finset.sum_const,Finset.card_range,nsmul_eq_mul]
        field_simp
      rw [hs] at h
      exact (norm_sub_le_norm_sub_add_norm_sub _ (coherent carrier reference N) _).trans (add_le_add h le_rfl)
    · simpa using (hcoh.sub_const prediction.coherentPeak).norm
  · apply eventuallyNear_of_bound _ _ _ (fun N => |meanSquare reference N-prediction.totalPower|)
    · filter_upwards [eventually_gt_atTop 0] with N hN
      have h := meanSquare_error_bound signal reference (fun _ => error) N (fun n _ => he n)
      have hn : (N:ℝ) ≠ 0 := by positivity
      have hs : (N:ℝ)⁻¹*(∑ n ∈ Finset.range N, error*(2*|reference n|+error)) ≤ error*(2*B+error) := by
        calc
          _ ≤ (N:ℝ)⁻¹*(∑ _n ∈ Finset.range N, error*(2*B+error)) := by
            apply mul_le_mul_of_nonneg_left _ (inv_nonneg.mpr (Nat.cast_nonneg N))
            exact Finset.sum_le_sum (fun n _ => mul_le_mul_of_nonneg_left (by linarith [hB n]) he0)
          _ = _ := by simp only [Finset.sum_const,Finset.card_range,nsmul_eq_mul]; field_simp
      exact (norm_sub_le_norm_sub_add_norm_sub _ (meanSquare reference N) _).trans (add_le_add (h.trans hs) le_rfl)
    · simpa only [Real.norm_eq_abs,sub_self,abs_zero] using (hpower.sub_const prediction.totalPower).norm

end DSP.ResponseMeasurement
