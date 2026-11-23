import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftChartData
import DSP.Analysis.PolarTube.Decode
import Mathlib.Analysis.Calculus.Deriv.Prod

/-! Exact coordinates for the proposed drifting fixture. The rational matrix
has a checked exact inverse, and the finite Fourier profile has a uniform
positive radial margin. These facts validate the chart, not its invariance.
The correction-potential bounds do not assert a coboundary residual bound. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftChart
open DriftChartData PolarTube
open scoped Matrix
set_option maxRecDepth 100000
set_option maxHeartbeats 0

def transform : Matrix (Fin 4) (Fin 4) ℝ := fun i j => transformRat i j
def inverse : Matrix (Fin 4) (Fin 4) ℝ := fun i j => inverseRat i j

theorem inverse_transform : inverse*transform = 1 := by
  have h : inverseRat*transformRat = 1 := by decide +kernel
  ext i j
  have hij := congrFun (congrFun h i) j
  simp only [Matrix.mul_apply, Matrix.one_apply] at hij ⊢
  dsimp only [inverse, transform]
  have hr := congrArg (fun q : ℚ => (q:ℝ)) hij
  push_cast at hr
  simpa only [apply_ite, Rat.cast_one, Rat.cast_zero] using hr

theorem transform_inverse : transform*inverse = 1 := by
  have h : transformRat*inverseRat = 1 := by decide +kernel
  ext i j
  have hij := congrFun (congrFun h i) j
  simp only [Matrix.mul_apply, Matrix.one_apply] at hij ⊢
  dsimp only [inverse, transform]
  have hr := congrArg (fun q : ℚ => (q:ℝ)) hij
  push_cast at hr
  simpa only [apply_ite, Rat.cast_one, Rat.cast_zero] using hr

def coordinates : State ≃ₗ[ℝ] State where
  toFun x := transform *ᵥ x
  invFun x := inverse *ᵥ x
  left_inv x := by
    dsimp only
    rw [Matrix.mulVec_mulVec, inverse_transform, Matrix.one_mulVec]
  right_inv x := by
    dsimp only
    rw [Matrix.mulVec_mulVec, transform_inverse, Matrix.one_mulVec]
  map_add' := Matrix.mulVec_add transform
  map_smul' a x := Matrix.mulVec_smul transform a x

def profile (θ : Real.Angle) : Normal := fun j => (profilePolynomial j).eval θ
def profileDerivative (θ : Real.Angle) : Normal :=
  fun j => (profilePolynomial j).derivative.eval θ
def profileSecondDerivative (θ : Real.Angle) : Normal :=
  fun j => (profilePolynomial j).derivative.derivative.eval θ

theorem profile_continuous : Continuous profile :=
  continuous_pi (fun j => (profilePolynomial j).continuous_eval)

theorem profile_hasDerivAt (θ : ℝ) :
    HasDerivAt (fun t : ℝ => profile (t:Real.Angle)) (profileDerivative θ) θ := by
  exact hasDerivAt_pi.mpr (fun j => (profilePolynomial j).hasDerivAt_eval θ)

theorem profileDerivative_hasDerivAt (θ : ℝ) :
    HasDerivAt (fun t : ℝ => profileDerivative (t:Real.Angle)) (profileSecondDerivative θ) θ := by
  exact hasDerivAt_pi.mpr (fun j => (profilePolynomial j).derivative.hasDerivAt_eval θ)

theorem radial_margin (θ : Real.Angle) : (11/10:ℝ) ≤ profile θ 0 := by
  have h : (11/10:ℚ) ≤ (profilePolynomial 0).lowerBound := by decide +kernel
  have hr : ((11/10:ℚ):ℝ) ≤ ((profilePolynomial 0).lowerBound:ℝ) := Rat.cast_le.mpr h
  norm_num only [Rat.cast_div, Rat.cast_ofNat] at hr
  exact hr.trans ((profilePolynomial 0).lowerBound_le θ)

def chart : PolarTube.Chart := ⟨coordinates, profile⟩

theorem chart_radial_positive (x : Coordinates) (hx : ‖x.2‖ ≤ (1/500000:ℝ)) :
    0 < radial chart.profile x := by
  apply radial_pos_of_tube profile (1/500000) _ x hx
  intro θ
  linarith [radial_margin θ]

theorem encode_decode_on_tube (x : Coordinates) (hx : ‖x.2‖ ≤ (1/500000:ℝ)) :
    chart.encode (chart.decode x) = x :=
  chart.encode_decode x (chart_radial_positive x hx)

def correction (j : Fin 3) (θ : Real.Angle) : ℝ := (correctionPolynomial j).eval θ
def correctionBound : Fin 3 → ℚ := ![3, 3, 1]
def correctionDerivativeBound : Fin 3 → ℚ := ![10, 10, 2]

theorem correction_bound (j : Fin 3) (θ : Real.Angle) :
    |correction j θ| ≤ (correctionBound j:ℝ) := by
  have h : ∀ j, (correctionPolynomial j).absoluteBound ≤ correctionBound j := by decide +kernel
  exact ((correctionPolynomial j).eval_absoluteBound θ).trans (Rat.cast_le.mpr (h j))

theorem correction_derivative_bound (j : Fin 3) (θ : Real.Angle) :
    |(correctionPolynomial j).derivative.eval θ| ≤ (correctionDerivativeBound j:ℝ) := by
  have h : ∀ j, (correctionPolynomial j).derivative.absoluteBound ≤ correctionDerivativeBound j := by
    decide +kernel
  exact ((correctionPolynomial j).derivative.eval_absoluteBound θ).trans (Rat.cast_le.mpr (h j))

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftChart
