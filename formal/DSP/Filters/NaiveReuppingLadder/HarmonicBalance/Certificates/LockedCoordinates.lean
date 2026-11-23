import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.AffineEnclosure

/-! Exact rational coordinate and residual checks for the locked fixture.
The coordinate system is arbitrary: its numerical provenance is not part of
the proof. Its inverse, initial ball, and final matrix bounds are checked here. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCoordinates
open LeanCert.Core DSP.IntervalArithmetic Enclosure LockedData Metric
open scoped Matrix
set_option maxRecDepth 100000
set_option maxHeartbeats 0

noncomputable def center : Vector := fun i => centerRat i
noncomputable def transform : RealMatrix := fun i j => transformRat i j
noncomputable def inverse : RealMatrix := fun i j => inverseRat i j
def centerBox : Box := fun i => IntervalRat.singleton (centerRat i)
def transformBox : MatrixBox := fun i j => IntervalRat.singleton (transformRat i j)

theorem center_mem : ∀ i, center i ∈ centerBox i := fun i => IntervalRat.mem_singleton (centerRat i)
theorem transform_mem : ∀ i j, transform i j ∈ transformBox i j := fun i j => IntervalRat.mem_singleton (transformRat i j)

theorem inverse_mem : ∀ i j, inverse i j ∈ matrices 0 i j := by
  have h : ∀ i j, (matrices 0 i j).lo ≤ inverseRat i j ∧ inverseRat i j ≤ (matrices 0 i j).hi := by decide +kernel
  intro i j
  exact ⟨Rat.cast_le.mpr (h i j).1, Rat.cast_le.mpr (h i j).2⟩

theorem inverse_transform : inverse*transform = 1 := by
  have h : inverseRat*transformRat = 1 := by decide +kernel
  ext i j
  have hij := congrFun (congrFun h i) j
  simp only [Matrix.mul_apply, Matrix.one_apply] at hij ⊢
  dsimp only [inverse, transform]
  have hr := congrArg (fun q : ℚ => (q:ℝ)) hij
  push_cast at hr
  simpa only [apply_ite, Rat.cast_one, Rat.cast_zero] using hr

private theorem startCheck (radius : ℚ) (hr : 0 ≤ radius) (box : Box)
    (h : ∀ i, contains (box i) ((centerBox i).add
      (matrixVector (matrices 0) (fun _ => ⟨-radius, radius, by linarith⟩) i)) = true)
    (w : Vector) (hw : w ∈ closedBall (0:Vector) (radius:ℝ)) :
    ∀ i, decode center inverse w i ∈ box i :=
  decode_mem_box center inverse centerBox (matrices 0) center_mem inverse_mem radius hr box h w hw

theorem whole_start (w : Vector) (hw : w ∈ closedBall (0:Vector) (1/10000:ℝ)) :
    ∀ i, decode center inverse w i ∈ wholeHistory 0 i :=
  startCheck (1/10000) (by norm_num) (wholeHistory 0) (by decide +kernel) w (by simpa using hw)

theorem small_start (w : Vector) (hw : w ∈ closedBall (0:Vector) (1/100000000:ℝ)) :
    ∀ i, decode center inverse w i ∈ smallHistory 0 i :=
  startCheck (1/100000000) (by norm_num) (smallHistory 0) (by decide +kernel) w (by simpa using hw)

theorem point_start : ∀ i, center i ∈ pointHistory 0 i := by
  have h : ∀ i, contains (pointHistory 0 i) (centerBox i) = true := by decide +kernel
  exact fun i => contains_sound (h i) (center_mem i)

theorem contraction_check : matrixBound (matrixProduct transformBox (matrices 170)) (3/5) = true := by decide +kernel

theorem residual_bound : ∀ i,
    (absoluteBound (matrixVector transformBox (fun j => (pointHistory 170 j).sub (centerBox j)) i):ℝ) ≤ 1/1000000000000 := by
  have h : ∀ i, absoluteBound
    (matrixVector transformBox (fun j => (pointHistory 170 j).sub (centerBox j)) i) ≤ (1/1000000000000:ℚ) := by decide +kernel
  intro i
  have hr : (absoluteBound (matrixVector transformBox (fun j => (pointHistory 170 j).sub (centerBox j)) i):ℝ) ≤ ((1/1000000000000:ℚ):ℝ) := Rat.cast_le.mpr (h i)
  norm_num only [Rat.cast_div, Rat.cast_one, Rat.cast_ofNat] at hr
  exact hr

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCoordinates
