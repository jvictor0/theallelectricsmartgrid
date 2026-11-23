import DSP.Filters.NaiveReuppingLadder.NoiseResponse.Display
import DSP.Filters.NaiveReuppingLadder.NoiseResponse.MarkovPower

noncomputable section
namespace NoiseDisplay
open Finset Filter MarkovPower
open scoped Topology

/-- From a bound on one expected block to a bound on the displayed stationary band.
    The candidate c is in wet power. 'fit' is the model's proposed ordinate in dB.
    When both candidate endpoints draw below the floor the right side is zero
    for any fit at or below the floor. -/
theorem population_drawn_error {S : Type*} [Fintype S]
    (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j) (one : ∀ i, ∑ j, p i j = 1)
    (r v : S → ℝ) (i : S) {b δ c dry : ℝ} (hd : 0 < dry)
    (hv : ∀ s, |v s| ≤ b)
    (res : ∀ s, |r s - (c + (v s - transition p v s))| ≤ δ)
    (fit top height : ℝ) (hh : 0 ≤ height) :
    ∃ wet : ℝ,
      Tendsto (fun n : ℕ => (∑ j ∈ range n, atTime (transition p) j r i)/(n:ℝ)) atTop (𝓝 wet) ∧
      |screenY top height (powerToDb (wet/dry)) - screenY top height fit| ≤
        height/120 * max |db ((c-δ)/dry)-clampDb fit| |db ((c+δ)/dry)-clampDb fit| := by
  obtain ⟨h,_,_,_,limit⟩ := population_mean p hp one r
  let P := transition p
  let proposed := fun _s : S => c
  have fixed : P proposed = proposed := by
    ext s
    change ∑ j, p s j*c = c
    rw [←sum_mul,one,one_mul]
  have bound := stochastic_bounded p hp one
  have res' : ∀ s, |(r-(proposed+(v-P v))) s| ≤ δ := res
  have band := ((limit i).sub_const c).abs
  have zero := (tendsto_const_div_atTop_nhds_zero_nat (2*b : ℝ))
  have rhs := (tendsto_const_nhds (x:=δ) (f:=atTop (α:=ℕ))).add zero
  rw [add_zero] at rhs
  have dist : |h i-c| ≤ δ := le_of_tendsto_of_tendsto band rhs (by
    filter_upwards [eventually_ge_atTop 1] with n hn
    exact approximate_mean P bound fixed hv res' (by omega) i)
  obtain ⟨lo,hi⟩ := abs_le.mp dist
  refine ⟨h i,limit i,?_⟩
  rw [drawn_error _ _ _ _ hh]
  unfold displayError
  rw [path_floor]
  apply mul_le_mul_of_nonneg_left _ (by positivity)
  exact band_bracket hd (by linarith : c-δ ≤ h i) (by linarith : h i ≤ c+δ)

end NoiseDisplay
