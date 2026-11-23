import DSP.Filters.NaiveReuppingLadder.NoiseResponse.Display

noncomputable section
namespace NoiseDisplay.CutoffGrid

variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

/-- A norm bound on the two endpoint discrepancies and their interpolation defect
    gives a bound on the discrepancy at the query. E may be the space of visible
    curves with its normalized L² norm over log frequency. -/
theorem from_endpoints (u a b : E) {t ea eb defect : ℝ}
    (ht : 0 ≤ t) (ht' : t ≤ 1) (ha : ‖a‖ ≤ ea) (hb : ‖b‖ ≤ eb)
    (hd : ‖u - ((1-t) • a + t • b)‖ ≤ defect) :
    ‖u‖ ≤ (1-t)*ea + t*eb + defect := by
  calc
    ‖u‖ = ‖((1-t) • a + t • b) + (u - ((1-t) • a + t • b))‖ := by congr 1; abel
    _ ≤ ‖(1-t) • a + t • b‖ + defect := (norm_add_le ..).trans (add_le_add le_rfl hd)
    _ ≤ (1-t)*ea + t*eb + defect := by
      apply add_le_add _ le_rfl
      apply (norm_add_le ..).trans
      simp only [norm_smul,Real.norm_eq_abs,abs_of_nonneg ht,abs_of_nonneg (sub_nonneg.mpr ht')]
      exact add_le_add (mul_le_mul_of_nonneg_left ha (sub_nonneg.mpr ht')) (mul_le_mul_of_nonneg_left hb ht)

omit [NormedSpace ℝ E] in
/-- Given a bound E0 at the measured vertices, an interval width h, and a
    local slope bound L on the signed *displayed* error curve, the unsampled
    allowance is at most L*h/2. J allows a fixed jump, e.g. from rounding. -/
theorem slope_allowance (u a b : E) {t h L J E0 : ℝ}
    (hh : 0 ≤ h) (hL : 0 ≤ L)
    (ha : ‖a‖ ≤ E0) (hb : ‖b‖ ≤ E0)
    (left : ‖u-a‖ ≤ J + L*(t*h)) (right : ‖u-b‖ ≤ J + L*((1-t)*h)) :
    ‖u‖ ≤ E0 + J + L*h/2 := by
  by_cases half : t ≤ 1/2
  · have near := mul_le_mul_of_nonneg_right half hh
    have e := (norm_add_le (u-a) a)
    rw [sub_add_cancel] at e
    linarith [mul_le_mul_of_nonneg_left near hL]
  · have near := mul_le_mul_of_nonneg_right (show 1-t ≤ 1/2 by linarith) hh
    have e := (norm_add_le (u-b) b)
    rw [sub_add_cancel] at e
    linarith [mul_le_mul_of_nonneg_left near hL]

/-- On a smooth cell a single local first-order approximation at the query
    cancels between the two endpoints. Quadratic Taylor remainders yield M*h²/8.
    This is a sufficient condition; clipped crossings can use slope_allowance. -/
theorem curvature_allowance (u a b slope : E) {t h M E0 : ℝ}
    (ht : 0 ≤ t) (ht' : t ≤ 1) (hM : 0 ≤ M)
    (ha : ‖a‖ ≤ E0) (hb : ‖b‖ ≤ E0)
    (left : ‖u-a-(t*h) • slope‖ ≤ M/2*(t*h)^2)
    (right : ‖u-b+((1-t)*h) • slope‖ ≤ M/2*((1-t)*h)^2) :
    ‖u‖ ≤ E0 + M*h^2/8 := by
  let l := u-a-(t*h) • slope
  let r := u-b+((1-t)*h) • slope
  have decompose : u - ((1-t) • a + t • b) = (1-t) • l + t • r := by
    dsimp [l,r]
    module
  have defect : ‖u - ((1-t) • a + t • b)‖ ≤ M*h^2/8 := by
    rw [decompose]
    apply (norm_add_le ..).trans
    simp only [norm_smul,Real.norm_eq_abs,abs_of_nonneg ht,abs_of_nonneg (sub_nonneg.mpr ht')]
    calc
      _ ≤ (1-t)*(M/2*(t*h)^2) + t*(M/2*((1-t)*h)^2) :=
        add_le_add (mul_le_mul_of_nonneg_left left (sub_nonneg.mpr ht')) (mul_le_mul_of_nonneg_left right ht)
      _ ≤ M*h^2/8 := by nlinarith [mul_nonneg (mul_nonneg hM (sq_nonneg h)) (sq_nonneg (t-1/2))]
  simpa only [show (1-t)*E0 + t*E0 = E0 by ring] using from_endpoints u a b ht ht' ha hb defect

section Cell
omit [NormedSpace ℝ E]

/-- Controls are normalized to [0,1] within one cell. For a physical slope L in
    the original coordinates and cell width h, take span = L*h. -/
def AxisBound (f : ℝ → E) (jump span : ℝ) : Prop :=
  ∀ t ∈ Set.Icc (0:ℝ) 1, ∀ s ∈ Set.Icc (0:ℝ) 1,
    ‖f t - f s‖ ≤ jump + span*|t-s|

private theorem along_axis {f : ℝ → E} {jump span e t : ℝ} (ht : t ∈ Set.Icc (0:ℝ) 1)
    (hs : 0 ≤ span) (var : AxisBound f jump span) (left : ‖f 0‖ ≤ e) (right : ‖f 1‖ ≤ e) :
    ‖f t‖ ≤ e + jump + span/2 := by
  have l := var t ht 0 ⟨le_rfl,zero_le_one⟩
  have r := var t ht 1 ⟨zero_le_one,le_rfl⟩
  rw [sub_zero,abs_of_nonneg ht.1] at l
  rw [abs_of_nonpos (sub_nonpos.mpr ht.2),neg_sub] at r
  simpa using slope_allowance (f t) (f 0) (f 1) (t:=t) (h:=1) zero_le_one hs left right
    (by simpa using l) (by simpa using r)

/-- Combine measured visible error, bias of the finite protocol at the eight
    vertices, and independent variation allowances for all three knob directions.
    All constants may differ by cell. No control outside this cell is used. -/
theorem visible_cell_error (physical measured fit : ℝ → ℝ → ℝ → E)
    (bias node jd sd jr sr jc sc : ℝ) (hd : 0 ≤ sd) (hr : 0 ≤ sr) (hc : 0 ≤ sc)
    (vertices : ∀ d ∈ ({0,1} : Set ℝ), ∀ r ∈ ({0,1} : Set ℝ), ∀ c ∈ ({0,1} : Set ℝ),
      ‖physical d r c - measured d r c‖ ≤ bias ∧ ‖measured d r c - fit d r c‖ ≤ node)
    (drive : ∀ r ∈ Set.Icc (0:ℝ) 1, ∀ c ∈ Set.Icc (0:ℝ) 1,
      AxisBound (fun d => physical d r c - fit d r c) jd sd)
    (res : ∀ d ∈ ({0,1} : Set ℝ), ∀ c ∈ Set.Icc (0:ℝ) 1,
      AxisBound (fun r => physical d r c - fit d r c) jr sr)
    (cut : ∀ d ∈ ({0,1} : Set ℝ), ∀ r ∈ ({0,1} : Set ℝ),
      AxisBound (fun c => physical d r c - fit d r c) jc sc)
    {d r c : ℝ} (di : d ∈ Set.Icc (0:ℝ) 1) (ri : r ∈ Set.Icc (0:ℝ) 1) (ci : c ∈ Set.Icc (0:ℝ) 1) :
    ‖physical d r c - fit d r c‖ ≤ bias + node + jc + sc/2 + jr + sr/2 + jd + sd/2 := by
  have corner (x : ℝ) (hx : x ∈ ({0,1} : Set ℝ)) (y : ℝ) (hy : y ∈ ({0,1} : Set ℝ))
      (z : ℝ) (hz : z ∈ ({0,1} : Set ℝ)) : ‖physical x y z - fit x y z‖ ≤ bias + node := by
    have h := vertices x hx y hy z hz
    exact (norm_sub_le_norm_sub_add_norm_sub ..).trans (add_le_add h.1 h.2)
  have edge (x : ℝ) (hx : x ∈ ({0,1} : Set ℝ)) (y : ℝ) (hy : y ∈ ({0,1} : Set ℝ)) :
      ‖physical x y c - fit x y c‖ ≤ bias + node + jc + sc/2 :=
    along_axis ci hc (cut x hx y hy) (corner x hx y hy 0 (by simp)) (corner x hx y hy 1 (by simp))
  have face (x : ℝ) (hx : x ∈ ({0,1} : Set ℝ)) :
      ‖physical x r c - fit x r c‖ ≤ bias + node + jc + sc/2 + jr + sr/2 :=
    along_axis ri hr (res x hx c ci) (edge x hx 0 (by simp)) (edge x hx 1 (by simp))
  exact along_axis di hd (drive r ri c ci) (face 0 (by simp)) (face 1 (by simp))
end Cell

end NoiseDisplay.CutoffGrid
