import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedChecks
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCoordinates
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedParameters

/-! A closed local existence-and-attraction certificate for the actual sample
map at the locked fixture. Numerical contraction and residual bounds are
conclusions of kernel checks, not assumptions of this theorem. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedOrbit
open Enclosure LockedData LockedCoordinates LockedParameters Metric Filter
open scoped Topology NNReal

def block (s : NaiveReuppingLadder ℝ) (x : Vector) : Vector :=
  Step.states (audioStep s) forcing x 170

theorem contraction (s : NaiveReuppingLadder ℝ) (h : Parameters s) :
    LipschitzOnWith (3/5)
      (coordinateBlock s forcing 170 center transform inverse) (closedBall 0 (1/10000)) := by
  convert coordinateBlock_lipschitz s h.damping coefficients (valid s h) LockedData.input
    wholeHistory wholeSaturation matrices 24 170 LockedChecks.wholeValues LockedChecks.wholeMatrices
    forcing forcing_mem center transform inverse transformBox transform_mem inverse_mem
    (1/10000) whole_start (3/5) (by norm_num) contraction_check using 1
  apply NNReal.coe_injective
  change (3/5:ℝ) = ((3/5:ℚ):ℝ)
  norm_num

theorem residual (s : NaiveReuppingLadder ℝ) (h : Parameters s) :
    dist (coordinateBlock s forcing 170 center transform inverse 0) 0 ≤ 1/1000000000000 :=
  coordinateBlock_residual s h.damping coefficients (valid s h) LockedData.input
    pointHistory pointSaturation 24 170 LockedChecks.pointValues forcing forcing_mem
    center transform inverse centerBox transformBox center_mem point_start transform_mem
    (1/1000000000000) (by norm_num) residual_bound

/-- The exact orbit is within 2.5e-12 in coordinates of the rational center;
we report the larger 1e-8 ball used for the output enclosure. Every initial
coordinate in the 1e-4 ball is attracted. No basin-from-reset claim is made. -/
theorem exists_attracting (s : NaiveReuppingLadder ℝ) (h : Parameters s) :
    ∃ w ∈ closedBall (0:Vector) (1/100000000),
      block s (decode center inverse w) = decode center inverse w ∧
      (∀ v ∈ closedBall (0:Vector) (1/10000),
        Tendsto (fun n => (block s)^[n] (decode center inverse v)) atTop (𝓝 (decode center inverse w))) := by
  have hr := residual s h
  have ht : dist (coordinateBlock s forcing 170 center transform inverse 0) 0+
      ((3/5:ℝ≥0):ℝ)*(1/10000) ≤ (1/10000:ℝ) := by norm_num at *; linarith
  obtain ⟨w, _, hfix, ha, he⟩ := DSP.AttractingBlock.exists_conjugate
    (coordinateBlock s forcing 170 center transform inverse) (block s)
    (decode center inverse) (decode_continuous center inverse)
    (coordinateBlock_commutes s forcing 170 center transform inverse inverse_transform)
    0 (1/10000) (3/5) (by norm_num) (by norm_num) (contraction s h) ht
  refine ⟨w, ?_, hfix, ha⟩
  rw [mem_closedBall, dist_comm]
  have he' : dist (0:Vector) w ≤ (1/1000000000000:ℝ)/(1-(3/5:ℝ≥0)) := by
    apply he.trans
    apply div_le_div_of_nonneg_right _ (by norm_num)
    simpa only [dist_comm] using hr
  norm_num at he'
  simpa only [dist_zero_left] using he'.trans (by norm_num : (1/400000000000:ℝ) ≤ 1/100000000)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedOrbit
