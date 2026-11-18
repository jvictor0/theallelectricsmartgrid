import DSP.Filters.NaiveReuppingLadder.FloorFreeScaling
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp
import Mathlib.Analysis.Calculus.Deriv.Slope

/-! Exact amplitude normalization of the floor-free ladder. The only remaining
amplitude dependence is in tanh(a*x)/a. Its continuous limit is the saturation
slope, while the adaptive controller remains. This does not exchange the
infinite-time and zero-amplitude limits. -/

noncomputable section
namespace DSP.NormalizedTanh
open Filter
open scoped Topology

theorem hasDerivAt_zero (c : Coefficients ℝ) : HasDerivAt (apply c) (slopeZero c) 0 := by
  change HasDerivAt (fun x : ℝ => Real.tanh (x * c.gain) / c.tanhGain) (c.gain / c.tanhGain) 0
  have ht : HasDerivAt Real.tanh 1 0 := by
    have h := (Real.hasDerivAt_sinh 0).div (Real.hasDerivAt_cosh 0) (by simp)
    have he : Real.tanh = Real.sinh / Real.cosh := funext Real.tanh_eq_sinh_div_cosh
    rw [he]
    simpa using h
  have hm : HasDerivAt (fun x : ℝ => x * c.gain) c.gain 0 := by
    simpa using (hasDerivAt_id (0 : ℝ)).mul_const c.gain
  have h := (show HasDerivAt Real.tanh 1 ((0 : ℝ) * c.gain) by simpa using ht).comp 0 hm
  simpa using h.div_const c.tanhGain

def divided (c : Coefficients ℝ) : ℝ → ℝ :=
  Function.update (fun x => apply c x / x) 0 (slopeZero c)

theorem divided_continuousAt_zero (c : Coefficients ℝ) : ContinuousAt (divided c) 0 := by
  simpa [divided, apply_zero] using (hasDerivAt_zero c).continuousAt_div

/-- Zero amplitude is filled in with the proved derivative. -/
def normalized (c : Coefficients ℝ) (amplitude x : ℝ) : ℝ :=
  if amplitude = 0 then slopeZero c * x else apply c (amplitude * x) / amplitude

theorem normalized_eq (c : Coefficients ℝ) (amplitude x : ℝ) :
    normalized c amplitude x = x * divided c (amplitude * x) := by
  by_cases ha : amplitude = 0
  · simp [normalized, divided, ha, mul_comm]
  · by_cases hx : x = 0
    · simp [normalized, ha, hx, apply_zero]
    · simp only [normalized, if_neg ha, divided,
        Function.update_of_ne (mul_ne_zero ha hx)]
      field_simp

/-- Joint limit: the normalized state may itself vary with amplitude. -/
theorem normalized_tendsto {ι : Type} {l : Filter ι} (c : Coefficients ℝ)
    (amplitude signal : ι → ℝ) (x : ℝ)
    (ha : Tendsto amplitude l (𝓝 0)) (hx : Tendsto signal l (𝓝 x)) :
    Tendsto (fun i => normalized c (amplitude i) (signal i)) l (𝓝 (slopeZero c * x)) := by
  have hp : Tendsto (fun i => amplitude i * signal i) l (𝓝 0) := by
    simpa only [zero_mul] using ha.mul hx
  have hd := (divided_continuousAt_zero c).tendsto.comp hp
  have h := hx.mul hd
  simp only [Function.comp_def] at h
  simp only [normalized_eq]
  simpa only [divided, Function.update_self, mul_comm x] using h

end DSP.NormalizedTanh

namespace DSP.NaiveReuppingLadder.FloorFree.NormalizedModel

def step (amplitude : ℝ) (s : NaiveReuppingLadder ℝ) (input : Sample ℝ) : NaiveReuppingLadder ℝ :=
  advanceWith (NormalizedTanh.normalized s.saturation amplitude) s (effectiveFeedback s) input

abbrev trajectory (amplitude : ℝ) (s : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) :=
  Step.states (step amplitude) (fun n => ⟨input n⟩) s

def output (amplitude : ℝ) (s : NaiveReuppingLadder ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory amplitude s input (n + 1)).output

theorem zero_step (s : NaiveReuppingLadder ℝ) (input : Sample ℝ) :
    step 0 s input = SlopeModel.step s input := by
  have h : NormalizedTanh.normalized s.saturation 0 =
      fun x => NormalizedTanh.slopeZero s.saturation * x := by
    funext x
    simp [NormalizedTanh.normalized]
  rw [step, h]
  rfl

/-- Exact conjugacy of the full nonlinear step, including both detectors and
the previous-sample feedback. It is not a small-amplitude approximation. -/
theorem actual_step_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ) (input : ℝ) :
    FloorFree.step (scaleHistory a s) ⟨a * input⟩ = scaleHistory a (step a s ⟨input⟩) := by
  unfold FloorFree.step advance step
  rw [effectiveFeedback_scale a ha]
  apply advanceWith_scale
  intro x
  simp only [scaleHistory, NormalizedTanh.normalized, if_neg ha]
  field_simp

theorem actual_trajectory_scale (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ)
    (input : ℕ → ℝ) (n : ℕ) :
    FloorFree.trajectory (scaleHistory a s) (fun n => a * input n) n =
      scaleHistory a (trajectory a s input n) := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change FloorFree.step (FloorFree.trajectory (scaleHistory a s) (fun n => a * input n) n)
      ⟨a * input n⟩ = _
    rw [ih, actual_step_scale a ha]
    rfl

/-- The response experiment can be studied entirely at unit stimulus amplitude
by putting epsilon into the saturator. Valid at every finite sample count. -/
theorem actual_output_normalized (a : ℝ) (ha : a ≠ 0) (s : NaiveReuppingLadder ℝ)
    (input : ℕ → ℝ) (n : ℕ) :
    FloorFree.output (NaiveReuppingLadder.reset s) (fun n => a * input n) n / a =
      output a (NaiveReuppingLadder.reset s) input n := by
  have h := actual_trajectory_scale a ha (NaiveReuppingLadder.reset s) input (n + 1)
  rw [scaleHistory_reset] at h
  unfold FloorFree.output output
  rw [h]
  simp only [scaleHistory]
  field_simp

end DSP.NaiveReuppingLadder.FloorFree.NormalizedModel
