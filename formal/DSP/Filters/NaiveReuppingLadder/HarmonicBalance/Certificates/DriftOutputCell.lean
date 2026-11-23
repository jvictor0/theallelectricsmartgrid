import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCellData

/-! Replay the actual output chart's phase, radius, reciprocals and Fourier
profile bounds. Index zero is the center orbit; index one is the whole input
cell. No numerical atan2 value is trusted: it only proposes the anchor. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCell
open LeanCert.Core DSP.IntervalArithmetic PolarTube TrigPolynomial DriftOutputCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0

theorem base_mem : Real.cos (anchor:ℝ) ∈ base 0 ∧ Real.sin (anchor:ℝ) ∈ base 1 := by
  have hc : contains (base 0) (IntervalTrig.cosinePoint anchor 30) = true := by decide +kernel
  have hs : contains (base 1) (IntervalTrig.sinePoint anchor 30) = true := by decide +kernel
  exact ⟨contains_sound hc (IntervalTrig.cosinePoint_mem anchor 30),
    contains_sound hs (IntervalTrig.sinePoint_mem anchor 30)⟩

theorem carrier_checked : carrierCheck base carrier 80 = true := by decide +kernel

theorem midpoint_checked : ∀ k, (angle k).midpoint = anchor := by decide +kernel

theorem carrier_mem (k : Fin 2) (i : Fin 81) :
    Real.cos ((i.val:ℝ)*((angle k).midpoint:ℝ)) ∈ carrier i.val 0 ∧
    Real.sin ((i.val:ℝ)*((angle k).midpoint:ℝ)) ∈ carrier i.val 1 := by
  rw [midpoint_checked]
  exact carrierCheck_sound base carrier 80 carrier_checked anchor base_mem.1 base_mem.2 i.val (by omega)

theorem phase_checked : ∀ k, phaseWindowCheck (state k 0) (state k 1) (base 0) (base 1)
    (phaseRadius k) = true := by decide +kernel

theorem phase_mem (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) :
    anchoredPhase (anchor:ℝ) (head z) ∈ angle k := by
  have h := abs_le.mp (phaseWindowCheck_sound _ _ _ _ _ (phase_checked k)
    anchor (head z) (hz 0) (hz 1) base_mem.1 base_mem.2)
  have he : ∀ k, (angle k).lo = anchor-phaseRadius k ∧ (angle k).hi = anchor+phaseRadius k := by decide +kernel
  rw [IntervalRat.mem_def,(he k).1,(he k).2]
  push_cast
  constructor <;> linarith

theorem ranges_checked : ∀ k order coordinate,
    contains (ranges k order coordinate)
      ((DriftProfileCellData.polynomial order coordinate).harmonicEnclosure (angle k) carrier) = true := by
  decide +kernel

theorem ranges_mem (k : Fin 2) (order coordinate : Fin 3) (z : State)
    (hz : ∀ i, z i ∈ state k i) :
    (DriftProfileCellData.polynomial order coordinate).eval
      (anchoredPhase (anchor:ℝ) (head z):Real.Angle) ∈ ranges k order coordinate :=
  contains_sound (ranges_checked k order coordinate)
    ((DriftProfileCellData.polynomial order coordinate).mem_harmonicEnclosure
      (angle k) carrier (carrier_mem k) _ (phase_mem k z hz))

def squareBox (k : Fin 2) : IntervalRat :=
  ((state k 0).mul (state k 0)).add ((state k 1).mul (state k 1))

theorem radius_checked : ∀ k, sqrtCheck (squareBox k) (radius k) = true ∧
    inverseCheck (squareBox k) (inverseSquare k) = true ∧
    inverseCheck (radius k) (inverseRadius k) = true := by decide +kernel

theorem radius_mem (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) :
    ‖head z‖ ∈ radius k ∧ (z 0^2+z 1^2)⁻¹ ∈ inverseSquare k ∧ ‖head z‖⁻¹ ∈ inverseRadius k := by
  have hs : z 0^2+z 1^2 ∈ squareBox k := by
    simpa only [pow_two,squareBox] using IntervalRat.mem_add (IntervalRat.mem_mul (hz 0) (hz 0))
      (IntervalRat.mem_mul (hz 1) (hz 1))
  have hr : ‖head z‖ ∈ radius k := by
    rw [norm_eq_radius]
    exact sqrtCheck_sound _ _ (radius_checked k).1 _ hs
  exact ⟨hr,inverseCheck_sound _ _ (radius_checked k).2.1 _ hs,
    inverseCheck_sound _ _ (radius_checked k).2.2 _ hr⟩

theorem head_ne_zero (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) : head z ≠ 0 := by
  have hp : ∀ k, 0 < (radius k).lo := by decide +kernel
  have hr := (radius_mem k z hz).1.1
  have hpos : (0:ℝ) < ((radius k).lo:ℝ) := by exact_mod_cast hp k
  exact norm_pos_iff.mp (hpos.trans_le hr)

theorem branch_safe (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) :
    head z/unitPhase ((anchor:ℝ):Real.Angle) ∈ Complex.slitPlane := by
  have hc := phase_checked k
  simp only [phaseWindowCheck,decide_eq_true_eq] at hc
  have hp : (0:ℝ) < ((((state k 0).mul (base 0)).add ((state k 1).mul (base 1))).lo:ℝ) := by
    exact_mod_cast hc.1
  apply relative_safe
  exact hp.trans_le (IntervalRat.mem_add (IntervalRat.mem_mul (hz 0) base_mem.1)
    (IntervalRat.mem_mul (hz 1) base_mem.2)).1

theorem profile_mem (k : Fin 2) (order coordinate : Fin 3) (z : State)
    (hz : ∀ i, z i ∈ state k i) :
    (DriftProfileCellData.polynomial order coordinate).eval ((head z).arg:Real.Angle) ∈
      ranges k order coordinate := by
  rw [← anchoredPhase_angle anchor (head z) (head_ne_zero k z hz)]
  exact ranges_mem k order coordinate z hz

theorem local_derivative_mem (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) (j : Fin 3) :
    DriftChart.profileDerivative (localPhase (unitPhase ((anchor:ℝ):Real.Angle)) (head z):Real.Angle) j ∈ ranges k 1 j := by
  rw [localPhase_angle _ _ (unitPhase_ne_zero _) (head_ne_zero k z hz)]
  exact profile_mem k 1 j z hz

theorem local_secondDerivative_mem (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) (j : Fin 3) :
    DriftChart.profileSecondDerivative (localPhase (unitPhase ((anchor:ℝ):Real.Angle)) (head z):Real.Angle) j ∈ ranges k 2 j := by
  rw [localPhase_angle _ _ (unitPhase_ne_zero _) (head_ne_zero k z hz)]
  exact profile_mem k 2 j z hz

def transverseBox (k : Fin 2) : Fin 3 → IntervalRat :=
  ![(radius k).sub (ranges k 0 0), (state k 2).sub (ranges k 0 1), (state k 3).sub (ranges k 0 2)]

theorem transverse_mem (k : Fin 2) (z : State) (hz : ∀ i, z i ∈ state k i) :
    ∀ j, (rawEncode DriftChart.profile z).2 j ∈ transverseBox k j := by
  intro j
  fin_cases j
  · exact IntervalRat.mem_sub (radius_mem k z hz).1 (profile_mem k 0 0 z hz)
  · exact IntervalRat.mem_sub (hz 2) (profile_mem k 0 1 z hz)
  · exact IntervalRat.mem_sub (hz 3) (profile_mem k 0 2 z hz)

theorem center_defect_checked : ∀ j, absoluteBound (transverseBox 0 j) ≤ (1/100000000:ℚ) := by decide +kernel

theorem whole_state_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    ∀ i, DriftChart.chart.linear (DriftBlockGeometry.block s
      (DriftChart.chart.decode ((θ:Real.Angle),r))) i ∈ state 1 i := by
  have hc : ∀ i, contains (state 1 i) (DriftBlockGeometry.endBox i) = true := by decide +kernel
  exact fun i => contains_sound (hc i) (DriftBlockGeometry.end_mem s h θ r hθ hr i)

theorem center_state_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s) :
    ∀ i, DriftChart.chart.linear (DriftBlockGeometry.block s DriftBlockInitial.centerPoint) i ∈ state 0 i := by
  have hc : ∀ i, contains (state 0 i) (matrixVector DriftBlockGeometry.transformBox
    (DriftBlockCellData.center 162) i) = true := by decide +kernel
  have hs := (DriftBlockCell.replay s h (5/2) 0
    (by norm_num [DriftProfileCellData.angle,DriftProfileCellData.interval,IntervalRat.mem_def])
    (by intro j; norm_num) 162 le_rfl).1
  exact fun i => contains_sound (hc i) (mem_matrixVector _ _ _ _ DriftBlockGeometry.transform_mem hs i)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCell
