import DSP.Filters.NaiveReuppingLadder.DetectorDynamics

noncomputable section
namespace DSP.NaiveReuppingLadder.DetectorDynamics

/-- Uniform in the wet power, including when either trajectory crosses the
controller knee. Thus an independently vanishing dry-power error vanishes
from the closed-loop gain estimate. -/
theorem reduction_dry_lipschitz (K D R p t q : ℝ) (hK : 0 ≤ K) (hD : 0 ≤ D)
    (hp : 0 ≤ p) (ht : 0 ≤ t) :
    |reduction K D R p q - reduction K D R t q| ≤
      (K/(powerFloor : ℝ))*(1+D*|R|)*|p-t| := by
  let m := fun x => max 0 (q-R*x)
  let v := fun x => x+(powerFloor : ℝ)
  let d := fun x => v x+D*m x
  have hv (x : ℝ) (h : 0 ≤ x) : 0 < v x := add_pos_of_nonneg_of_pos h floor_pos
  have hmd (x : ℝ) : 0 ≤ D*m x := mul_nonneg hD (le_max_left _ _)
  have hd (x : ℝ) (h : 0 ≤ x) : 0 < d x := add_pos_of_pos_of_nonneg (hv x h) (hmd x)
  have hh (x : ℝ) (h : 0 ≤ x) : reduction K D R x q = K*(v x/d x) := by
    change K/(1+D*(m x/v x)) = _
    rw [show 1+D*(m x/v x) = d x/v x by dsimp only [d]; field_simp [(hv x h).ne']]
    field_simp
  let w := v t/d t
  have hw : 0 ≤ w := div_nonneg (hv t ht).le (hd t ht).le
  have hw1 : w ≤ 1 := (div_le_one (hd t ht)).mpr (le_add_of_nonneg_right (hmd t))
  have hm : |m t-m p| ≤ |R| * |p-t| := by
    calc
      _ ≤ |(q-R*t)-(q-R*p)| := by
        simpa only [m,max_comm (0:ℝ)] using abs_max_sub_max_le_abs (q-R*t) (q-R*p) 0
      _ = _ := by rw [show (q-R*t)-(q-R*p)=R*(p-t) by ring,abs_mul]
  have he : reduction K D R p q-reduction K D R t q =
      (K/d p)*((p-t)*(1-w)+(D*(m t-m p))*w) := by
    rw [hh p hp,hh t ht]
    dsimp only [w]
    field_simp [(hd p hp).ne',(hd t ht).ne']
    dsimp [d,v]
    ring
  rw [he,abs_mul,abs_of_nonneg (div_nonneg hK (hd p hp).le)]
  have hsum := abs_add_le ((p-t)*(1-w)) (D*(m t-m p)*w)
  rw [abs_mul,abs_mul,abs_mul,abs_of_nonneg hD,abs_of_nonneg hw,abs_of_nonneg (sub_nonneg.mpr hw1)] at hsum
  have h₁ := mul_le_of_le_one_right (abs_nonneg (p-t)) (by linarith : 1-w ≤ 1)
  have h₂ := (mul_le_of_le_one_right (mul_nonneg hD (abs_nonneg (m t-m p))) hw1).trans
    (mul_le_mul_of_nonneg_left hm hD)
  calc
    _ ≤ (K/(powerFloor:ℝ))*(|p-t|+D*(|R| * |p-t|)) := mul_le_mul
      (div_le_div_of_nonneg_left hK floor_pos (by dsimp [d,v]; linarith [hmd p]))
      (hsum.trans (add_le_add h₁ h₂)) (abs_nonneg _) (div_nonneg hK floor_pos.le)
    _ = _ := by ring

end DSP.NaiveReuppingLadder.DetectorDynamics
