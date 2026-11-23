import DSP.Filters.NaiveReuppingLadder.NoiseResponse.PopulationDisplay

noncomputable section
namespace NoiseDisplay.MarkovPower
open Filter Finset
open scoped Topology
variable {S : Type*} [Fintype S]
variable (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j) (one : ∀ i, ∑ j, p i j = 1)

def fromReset (i : S) (f : S → ℝ) : ℝ := (population_mean p hp one f).choose i

theorem tends_fromReset (i : S) (f : S → ℝ) :
    Tendsto (fun n : ℕ => (∑ j ∈ range n, atTime (transition p) j f i)/(n:ℝ))
      atTop (𝓝 (fromReset p hp one i f)) := by
  obtain ⟨_,_,_,h⟩ := (population_mean p hp one f).choose_spec
  exact h i

include hp in
private theorem time_mono (n : ℕ) {f g : S → ℝ} (fg : ∀ j, f j ≤ g j) :
    ∀ i, atTime (transition p) n f i ≤ atTime (transition p) n g i := by
  induction n with
  | zero => exact fg
  | succ n ih =>
    intro i
    exact sum_le_sum (fun j _ => mul_le_mul_of_nonneg_left (ih j) (hp i j))

theorem fromReset_mono (i : S) {f g : S → ℝ} (fg : ∀ j, f j ≤ g j) :
    fromReset p hp one i f ≤ fromReset p hp one i g := by
  apply le_of_tendsto_of_tendsto (tends_fromReset p hp one i f) (tends_fromReset p hp one i g)
  filter_upwards with n
  exact div_le_div_of_nonneg_right (sum_le_sum fun j _ => time_mono p hp j fg i) (Nat.cast_nonneg n)

theorem fromReset_add (i : S) (f g : S → ℝ) :
    fromReset p hp one i (f+g) = fromReset p hp one i f + fromReset p hp one i g := by
  have lin (n : ℕ) : atTime (transition p) n (f+g) = atTime (transition p) n f + atTime (transition p) n g := by
    induction n with
    | zero => rfl
    | succ n ih => simp only [atTime,ih,map_add]
  apply tendsto_nhds_unique (tends_fromReset p hp one i _)
  simpa only [lin,Pi.add_apply,sum_add_distrib,add_div] using
    (show Tendsto _ atTop _ from (tends_fromReset p hp one i f).add (tends_fromReset p hp one i g))

theorem fromReset_smul (i : S) (c : ℝ) (f : S → ℝ) :
    fromReset p hp one i (c • f) = c * fromReset p hp one i f := by
  have lin (n : ℕ) : atTime (transition p) n (c • f) = c • atTime (transition p) n f := by
    induction n with
    | zero => rfl
    | succ n ih => simp only [atTime,ih,map_smul]
  apply tendsto_nhds_unique (tends_fromReset p hp one i _)
  simpa only [lin,Pi.smul_apply,smul_eq_mul,←mul_sum,mul_div_assoc] using
    (tends_fromReset p hp one i f).const_mul c

theorem fromReset_const (i : S) (c : ℝ) : fromReset p hp one i (fun _ => c)=c := by
  have lin (n : ℕ) : atTime (transition p) n (fun _ => c) = fun _ => c := by
    induction n with
    | zero => rfl
    | succ n ih =>
      rw [atTime,ih]
      ext j
      change ∑ k, p j k*c = c
      rw [←sum_mul,one,one_mul]
  apply tendsto_nhds_unique (tends_fromReset p hp one i _)
  apply tendsto_const_nhds.congr'
  filter_upwards [eventually_ge_atTop 1] with n hn
  simp [lin,mul_div_cancel_left₀,show (n:ℝ) ≠ 0 by exact_mod_cast (show n≠0 by omega)]

theorem fromReset_boundary (i : S) (v : S → ℝ) :
    fromReset p hp one i (v-transition p v)=0 := by
  let b := ∑ j, |v j|
  have hv (j : S) : |v j| ≤ b := single_le_sum (fun k _ => abs_nonneg (v k)) (mem_univ j)
  have lim := (tends_fromReset p hp one i (v-transition p v)).abs
  have zero := tendsto_const_div_atTop_nhds_zero_nat (2*b : ℝ)
  apply abs_nonpos_iff.mp
  apply le_of_tendsto_of_tendsto lim zero
  filter_upwards [eventually_ge_atTop 1] with n hn
  simpa only [Pi.zero_apply,sub_zero] using mean_error (transition p) (h:=0)
    (r:=v-transition p v) (stochastic_bounded p hp one) (map_zero _) (by simp) (by omega : 0<n) i hv

/-- Only the occupancy from the selected reset matters. A residual excess bounded
    by M on a group B of rare states costs M times the expected long-run fraction
    of time spent there. Multiple recurrent classes and transients are allowed. -/
theorem occupancy_residual [DecidableEq S] (i : S) (r v : S → ℝ) (B : Finset S)
    {c δ M : ℝ} (res : ∀ j,
      |r j - (c + (v-transition p v) j)| ≤ δ + M*(if j ∈ B then (1:ℝ) else 0)) :
    |fromReset p hp one i r - c| ≤
      δ + M*fromReset p hp one i (fun j => if j ∈ B then 1 else 0) := by
  let boundary := v-transition p v
  let flag : S → ℝ := fun j => if j ∈ B then 1 else 0
  let up := ((fun _ : S => c+δ) + M • flag) + boundary
  let lo := ((fun _ : S => c-δ) + (-M) • flag) + boundary
  have upper : ∀ j, r j ≤ up j := by
    intro j
    change r j ≤ (c+δ+M*flag j)+boundary j
    have h : |r j-(c+boundary j)| ≤ δ+M*flag j := res j
    linarith [(abs_le.mp h).2]
  have lower : ∀ j, lo j ≤ r j := by
    intro j
    change (c-δ+(-M)*flag j)+boundary j ≤ r j
    have h : |r j-(c+boundary j)| ≤ δ+M*flag j := res j
    linarith [(abs_le.mp h).1]
  have u := fromReset_mono p hp one i upper
  have l := fromReset_mono p hp one i lower
  simp only [up,lo,boundary,fromReset_add,fromReset_const,fromReset_smul,fromReset_boundary] at u l
  change |fromReset p hp one i r-c| ≤ δ+M*fromReset p hp one i flag
  exact abs_le.mpr ⟨by linarith,by linarith⟩

end NoiseDisplay.MarkovPower
