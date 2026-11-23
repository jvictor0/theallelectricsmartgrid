import Mathlib.Analysis.SpecialFunctions.Trigonometric.Angle
import Mathlib.Analysis.Calculus.Deriv.MeanValue
import Mathlib.Topology.Order.IntermediateValue

/-! Turn a continuous degree-one real phase lift with a positive lower slope
bound into a circle bijection with a quantitative separation bound. All
distances on the circle use the quotient metric, including across its seam. -/

noncomputable section
namespace DSP.CircleLift
open Filter
open scoped Topology

theorem angle_dist_le (x y : ℝ) : dist (x:Real.Angle) (y:Real.Angle) ≤ |x-y| := by
  rw [dist_eq_norm, ← Real.Angle.coe_sub]
  exact QuotientAddGroup.norm_mk_le_norm

theorem angle_shift (x : ℝ) (k : ℤ) :
    ((x+k*(2*Real.pi):ℝ):Real.Angle) = (x:Real.Angle) := by
  rw [Real.Angle.coe_add, Real.Angle.intCast_mul_eq_zsmul, Real.Angle.coe_two_pi, zsmul_zero, add_zero]

structure Increasing (A : ℝ) where
  realMap : ℝ → ℝ
  continuous : Continuous realMap
  period : ∀ x, realMap (x+2*Real.pi) = realMap x+2*Real.pi
  positive : 0 < A
  lower : ∀ x y, x ≤ y → A*(y-x) ≤ realMap y-realMap x

namespace Increasing
variable {A : ℝ} (lift : Increasing A)

theorem strictMono : StrictMono lift.realMap := by
  intro x y hxy
  have h := lift.lower x y hxy.le
  have hp := mul_pos lift.positive (sub_pos.mpr hxy)
  linarith

theorem real_separation (x y : ℝ) : A*|x-y| ≤ |lift.realMap x-lift.realMap y| := by
  rcases le_total x y with h | h
  · rw [abs_of_nonpos (sub_nonpos.mpr h), abs_of_nonpos (sub_nonpos.mpr (lift.strictMono.monotone h))]
    linarith [lift.lower x y h]
  · rw [abs_of_nonneg (sub_nonneg.mpr h), abs_of_nonneg (sub_nonneg.mpr (lift.strictMono.monotone h))]
    exact lift.lower y x h

theorem int_period (x : ℝ) (k : ℤ) :
    lift.realMap (x+k*(2*Real.pi)) = lift.realMap x+k*(2*Real.pi) := by
  have hp : Function.Periodic (fun x => lift.realMap x-x) (2*Real.pi) := by
    intro y
    dsimp only
    rw [lift.period]
    ring
  have h := hp.int_mul k x
  linarith

theorem respects_angle {x y : ℝ} (h : (x:Real.Angle) = (y:Real.Angle)) :
    (lift.realMap x:Real.Angle) = lift.realMap y := by
  obtain ⟨k,hk⟩ := Real.Angle.angle_eq_iff_two_pi_dvd_sub.mp h
  have hx : x = y+k*(2*Real.pi) := by linarith
  rw [hx, lift.int_period, angle_shift]

theorem real_surjective : Function.Surjective lift.realMap := by
  have htop : Tendsto lift.realMap atTop atTop := by
    apply tendsto_atTop.mpr
    intro b
    filter_upwards [eventually_ge_atTop (max 0 ((b-lift.realMap 0)/A))] with x hx
    have hx0 := (le_max_left 0 ((b-lift.realMap 0)/A)).trans hx
    have hax := (div_le_iff₀ lift.positive).mp ((le_max_right 0 ((b-lift.realMap 0)/A)).trans hx)
    have h := lift.lower 0 x hx0
    linarith
  have hbot : Tendsto lift.realMap atBot atBot := by
    apply tendsto_atBot.mpr
    intro b
    filter_upwards [eventually_le_atBot (min 0 ((b-lift.realMap 0)/A))] with x hx
    have hx0 := hx.trans (min_le_left 0 ((b-lift.realMap 0)/A))
    have hax := (le_div_iff₀ lift.positive).mp (hx.trans (min_le_right 0 ((b-lift.realMap 0)/A)))
    have h := lift.lower x 0 hx0
    linarith
  exact lift.continuous.surjective htop hbot

def circleMap (θ : Real.Angle) : Real.Angle := lift.realMap θ.toReal

theorem circleMap_coe (x : ℝ) : lift.circleMap (x:Real.Angle) = (lift.realMap x:Real.Angle) :=
  lift.respects_angle (Real.Angle.coe_toReal (x:Real.Angle))

theorem circleMap_injective : Function.Injective lift.circleMap := by
  intro θ φ h
  obtain ⟨k,hk⟩ := Real.Angle.angle_eq_iff_two_pi_dvd_sub.mp h
  have he : lift.realMap θ.toReal = lift.realMap (φ.toReal+k*(2*Real.pi)) := by
    rw [lift.int_period]
    linarith
  have hr := lift.strictMono.injective he
  have hc := congrArg (fun x : ℝ => (x:Real.Angle)) hr
  simpa only [angle_shift, Real.Angle.coe_toReal] using hc

theorem circleMap_surjective : Function.Surjective lift.circleMap := by
  intro θ
  obtain ⟨x,hx⟩ := lift.real_surjective θ.toReal
  exact ⟨(x:Real.Angle), by rw [lift.circleMap_coe, hx, Real.Angle.coe_toReal]⟩

/-- Choosing the closest integer translate of the OUTPUT lift transfers the
real lower bound to the quotient distance without assuming equal branch cuts. -/
theorem circleMap_separation (θ φ : Real.Angle) :
    A*dist θ φ ≤ dist (lift.circleMap θ) (lift.circleMap φ) := by
  let x := θ.toReal
  let y := φ.toReal
  let k : ℤ := round ((2*Real.pi)⁻¹*(lift.realMap x-lift.realMap y))
  have hinput : dist θ φ ≤ |x-(y+k*(2*Real.pi))| := by
    simpa only [angle_shift, x, y, Real.Angle.coe_toReal] using angle_dist_le x (y+k*(2*Real.pi))
  have hout : dist (lift.circleMap θ) (lift.circleMap φ) =
      |lift.realMap x-lift.realMap (y+k*(2*Real.pi))| := by
    rw [lift.int_period, circleMap, circleMap, dist_eq_norm, ← Real.Angle.coe_sub]
    change ‖((lift.realMap x-lift.realMap y:ℝ):AddCircle (2*Real.pi))‖ = _
    rw [AddCircle.norm_eq]
    dsimp [k]
    congr 1
    ring
  rw [hout]
  exact (mul_le_mul_of_nonneg_left hinput lift.positive.le).trans (lift.real_separation _ _)

def equiv : Real.Angle ≃ Real.Angle :=
  Equiv.ofBijective lift.circleMap ⟨lift.circleMap_injective,lift.circleMap_surjective⟩

end Increasing

/-- A uniform derivative lower bound gives the real finite-difference bound
used above. The eventual graph need only be Lipschitz: its own derivative is
not required by this theorem or by the graph construction. -/
theorem lower_of_derivative (f f' : ℝ → ℝ) (a : ℝ)
    (hf : ∀ x, HasDerivAt f (f' x) x) (hl : ∀ x, a ≤ f' x)
    (x y : ℝ) (hxy : x ≤ y) : a*(y-x) ≤ f y-f x := by
  have hm : Monotone (fun z => f z-a*z) := monotone_of_hasDerivAt_nonneg
    (fun z => (hf z).sub ((hasDerivAt_id z).const_mul a))
    (fun z => by simpa only [mul_one, Pi.zero_apply] using sub_nonneg.mpr (hl z))
  have h := hm hxy
  linarith

end DSP.CircleLift
