import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBounds
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.CoordinatePartials
import DSP.Analysis.CoordinateBounds

/-! True partial derivatives and centered bounds for the complete sample
block in this input cell. Both input and output chart curvature are included.
This is one cell of the whole-circle coverage obligation. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftCellMap
open LeanCert.Core DSP.IntervalArithmetic PolarTube DriftOutputBoundsData DerivativeFrame
open scoped Matrix
set_option maxRecDepth 10000
set_option maxHeartbeats 800000

def width : State := fun i => (DriftOutputBoundsData.width i:ℝ)
def InCell (x : State) : Prop := CoordinateBounds.InBox DriftBlockInitial.seed width x
def normal (x : State) : Normal := ![x 1,x 2,x 3]
theorem normal_apply (x : State) (i : Fin 3) : normal x i = x i.succ := by
  fin_cases i <;> rfl
def anchor : ℂ := unitPhase ((DriftOutputCellData.anchor:ℝ):Real.Angle)

def map (s : NaiveReuppingLadder ℝ) : State → State :=
  coordinateMap DriftChart.chart (fun θ => DriftChart.profileDerivative θ)
    (fun θ => DriftChart.profileSecondDerivative θ) s DriftBlockParameters.forcing 162 anchor

def jacobian (s : NaiveReuppingLadder ℝ) : State → Fin 4 → State :=
  coordinatePartial DriftChart.chart (fun θ => DriftChart.profileDerivative θ)
    (fun θ => DriftChart.profileSecondDerivative θ) s DriftBlockParameters.forcing 162 anchor

theorem coordinates_eq (x : State) : DriftBlockInitial.coordinates (x 0) (normal x) = x := by
  ext i
  fin_cases i <;> rfl

theorem phase_mem (x : State) (hx : InCell x) : x 0 ∈ DriftProfileCellData.angle := by
  have h := abs_le.mp (hx 0)
  norm_num [DriftBlockInitial.seed,width,DriftOutputBoundsData.width] at h
  norm_num [DriftProfileCellData.angle,DriftProfileCellData.interval,IntervalRat.mem_def]
  constructor <;> linarith

theorem normal_mem (x : State) (hx : InCell x) (j : Fin 3) : |normal x j| ≤ (1/500000:ℝ) := by
  have h := hx j.succ
  fin_cases j <;>
    simpa [DriftBlockInitial.seed,width,DriftOutputBoundsData.width,normal] using h

theorem seed_mem : InCell DriftBlockInitial.seed := by
  intro i
  fin_cases i <;> norm_num [width,DriftOutputBoundsData.width]

theorem frame_eq (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) (t : ℝ) :
    DriftSecondOutput.outputFrame s x j k t =
      block DriftChart.chart (fun θ => DriftChart.profileDerivative θ)
        (fun θ => DriftChart.profileSecondDerivative θ) s DriftBlockParameters.forcing 162 anchor
        (affine x (Pi.single j 1) (Pi.single k 1) t) := rfl

theorem frame_value (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) (t : ℝ) :
    (DriftSecondOutput.outputFrame s x j k t).value = map s (x+t • Pi.single j 1) := by
  rw [frame_eq]
  exact affine_value _ _ _ _ _ _ _ _ _ _ _

theorem frame_right (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) (t : ℝ) :
    (DriftSecondOutput.outputFrame s x j k t).right = jacobian s (x+t • Pi.single j 1) k := by
  rw [frame_eq]
  exact affine_right _ _ _ _ _ _ _ _ _ _ _

theorem frame_left (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) :
    (DriftSecondOutput.outputFrame s x j k 0).left = jacobian s x j := by
  rw [frame_eq]
  exact affine_left _ _ _ _ _ _ _ _ _ _

theorem differentiates (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (j k : Fin 4) :
    Differentiates (DriftSecondOutput.outputFrame s x j k) 0 := by
  simpa only [coordinates_eq] using DriftSecondOutput.output_differentiates s h
    (x 0) (normal x) (phase_mem x hx) (normal_mem x hx) j k

theorem map_hasDerivAt (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (i j : Fin 4) :
    HasDerivAt (fun t => map s (Function.update x j t) i) (jacobian s x j i) (x j) := by
  apply axis_derivative (fun z => map s z i) x j
  simpa only [frame_value,frame_left] using (differentiates s h x hx j 0).1 i

theorem partial_hasDerivAt (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (i j k : Fin 4) :
    HasDerivAt (fun t => jacobian s (Function.update x j t) k i)
      ((DriftSecondOutput.outputFrame s x j k 0).mixed i) (x j) := by
  apply axis_derivative (fun z => jacobian s z k i) x j
  simpa only [frame_right] using (differentiates s h x hx j k).2 i

theorem hessian_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (i j k : Fin 4) :
    (DriftSecondOutput.outputFrame s x j k 0).mixed i ∈ hessian i j k := by
  simpa only [coordinates_eq] using DriftOutputBounds.hessian_mem s h
    (x 0) (normal x) (phase_mem x hx) (normal_mem x hx) i j k

theorem center_gradient_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (i j : Fin 4) : jacobian s DriftBlockInitial.seed j i ∈ gradient i j := by
  simpa only [frame_left] using DriftOutputBounds.center_mem s h i j 0

theorem whole_gradient_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (i j : Fin 4) : jacobian s x j i ∈ wholeGradient i j := by
  have hb := CoordinateBounds.box_bound DriftBlockInitial.seed width (fun x => jacobian s x j i)
    (fun x k => (DriftSecondOutput.outputFrame s x k j 0).mixed i)
    (fun k => (absoluteBound (hessian i k j):ℝ))
    (fun x hx k => partial_hasDerivAt s h x hx i k j)
    (fun x hx k => norm_le_absoluteBound (hessian_mem s h x hx i k j))
    DriftBlockInitial.seed x seed_mem hx
  have hsym : ∀ i j k, hessian i j k = hessian i k j := by decide +kernel
  have hrad : |jacobian s x j i-jacobian s DriftBlockInitial.seed j i| ≤
      (IntervalTaylor.firstOrderRadius (hessian i j) DriftOutputBoundsData.width:ℝ) := by
    apply hb.trans
    simp only [IntervalTaylor.firstOrderRadius,Rat.cast_sum,Rat.cast_mul]
    apply Finset.sum_le_sum
    intro k _
    rw [hsym i k j]
    exact mul_le_mul_of_nonneg_left (hx k) (by
      exact_mod_cast (abs_nonneg (hessian i j k).lo).trans (le_max_left _ |(hessian i j k).hi|))
  exact IntervalTaylor.radiusCheck_sound _ _ _ (DriftOutputBounds.wholeGradient_checked i j)
    _ _ (center_gradient_mem s h i j) hrad

theorem map_matches (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) :
    ((map s x 0:Real.Angle),normal (map s x)) =
      DriftChart.chart.coordinateStep (DriftBlockGeometry.block s) ((x 0:Real.Angle),normal x) := by
  have hn := DriftBlockGeometry.head_ne_zero s h (x 0) (normal x)
    (phase_mem x hx) (normal_mem x hx)
  have he := congrArg (liftEncode DriftChart.profile anchor)
    (DriftSecondOutput.value_eq s (x 0) (normal x) 0 0)
  rw [coordinates_eq] at he
  change (DriftSecondOutput.outputFrame s x 0 0 0).value = _ at he
  rw [frame_value,zero_smul,add_zero] at he
  rw [he]
  exact liftEncode_matches _ _ (unitPhase_ne_zero _) _ hn

theorem center_defect (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (i : Fin 3) : |map s DriftBlockInitial.seed i.succ| ≤ (1/100000000:ℝ) := by
  have hm := congrArg (fun z : Coordinates => z.2 i) (map_matches s h _ seed_mem)
  have he : (DriftChart.chart.coordinateStep (DriftBlockGeometry.block s)
      ((DriftBlockInitial.seed 0:Real.Angle),normal DriftBlockInitial.seed)).2 i =
        (rawEncode DriftChart.profile (DriftChart.chart.linear
          (DriftBlockGeometry.block s DriftBlockInitial.centerPoint))).2 i := by
    rfl
  rw [he] at hm
  have hn : normal (map s DriftBlockInitial.seed) i = map s DriftBlockInitial.seed i.succ := by
    fin_cases i <;> rfl
  change normal (map s DriftBlockInitial.seed) i = _ at hm
  rw [hn] at hm
  rw [hm]
  exact (norm_le_absoluteBound (DriftOutputCell.transverse_mem 0 _
    (DriftOutputCell.center_state_mem s h) i)).trans (by
      have hc := (Rat.cast_le (K := ℝ)).mpr (DriftOutputCell.center_defect_checked i)
      norm_num only [Rat.cast_div,Rat.cast_natCast] at hc
      exact hc)

/-- Every point of this cell maps into the transverse tube, with explicit
slack. The output phase need not lie in the same input cell. -/
theorem transverse_image_bound (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (hx : InCell x) (i : Fin 3) :
    |map s x i.succ| ≤ (7/5000000:ℝ) := by
  have hb := CoordinateBounds.box_bound DriftBlockInitial.seed width (fun x => map s x i.succ)
    (fun x j => jacobian s x j i.succ) (fun j => (absoluteBound (wholeGradient i.succ j):ℝ))
    (fun x hx j => map_hasDerivAt s h x hx i.succ j)
    (fun x hx j => norm_le_absoluteBound (whole_gradient_mem s h x hx i.succ j))
    DriftBlockInitial.seed x seed_mem hx
  have hrad : |map s x i.succ-map s DriftBlockInitial.seed i.succ| ≤
      (IntervalTaylor.firstOrderRadius (wholeGradient i.succ) DriftOutputBoundsData.width:ℝ) := by
    apply hb.trans
    simp only [IntervalTaylor.firstOrderRadius,Rat.cast_sum,Rat.cast_mul]
    apply Finset.sum_le_sum
    intro j _
    exact mul_le_mul_of_nonneg_left (hx j) (by
      exact_mod_cast (abs_nonneg (wholeGradient i.succ j).lo).trans
        (le_max_left _ |(wholeGradient i.succ j).hi|))
  have ht := abs_add_le (map s x i.succ-map s DriftBlockInitial.seed i.succ)
    (map s DriftBlockInitial.seed i.succ)
  rw [sub_add_cancel] at ht
  have hc : (IntervalTaylor.firstOrderRadius (wholeGradient i.succ) DriftOutputBoundsData.width:ℝ)+
      1/100000000 ≤ 7/5000000 := by
    have hc' := (Rat.cast_le (K := ℝ)).mpr (DriftOutputBounds.image_checked i)
    norm_num only [Rat.cast_add,Rat.cast_div,Rat.cast_natCast] at hc'
    exact hc'
  linarith [center_defect s h i]

theorem coordinates_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) : InCell (DriftBlockInitial.coordinates θ r) := by
  intro i
  fin_cases i
  · have h := hθ
    norm_num [DriftProfileCellData.angle,DriftProfileCellData.interval,IntervalRat.mem_def] at h
    norm_num [DriftBlockInitial.coordinates,DriftBlockInitial.seed,width,DriftOutputBoundsData.width]
    exact abs_le.mpr ⟨by linarith [h.1],by linarith [h.2]⟩
  all_goals simpa [DriftBlockInitial.coordinates,DriftBlockInitial.seed,width,DriftOutputBoundsData.width] using hr _

/-- The certified image bound concerns the actual ladder block, not just its
derivative frame or a separately evaluated approximation. -/
theorem actual_transverse_image (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (i : Fin 3) :
    |(DriftChart.chart.coordinateStep (DriftBlockGeometry.block s) ((θ:Real.Angle),r)).2 i| ≤
      (7/5000000:ℝ) := by
  have hx := coordinates_mem θ r hθ hr
  have hm := congrArg (fun z : Coordinates => z.2 i) (map_matches s h _ hx)
  have he : normal (DriftBlockInitial.coordinates θ r) = r := by ext j; fin_cases j <;> rfl
  rw [he] at hm
  change normal (map s (DriftBlockInitial.coordinates θ r)) i =
    (DriftChart.chart.coordinateStep (DriftBlockGeometry.block s) ((θ:Real.Angle),r)).2 i at hm
  rw [← hm]
  have he' : normal (map s (DriftBlockInitial.coordinates θ r)) i =
      map s (DriftBlockInitial.coordinates θ r) i.succ := by fin_cases i <;> rfl
  rw [he']
  exact transverse_image_bound s h _ hx i

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftCellMap
