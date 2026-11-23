import Mathlib.Analysis.SpecialFunctions.Complex.Arg
import Mathlib.LinearAlgebra.Matrix.ToLin
import Mathlib.Topology.Instances.AddCircle.Real
import Mathlib.Analysis.Normed.Module.FiniteDimension

/-! Exact curved coordinates for an attracting oscillator tube. Phase lives
in `Real.Angle`, so the atan2 branch cut is an identification, not a
discontinuity of the phase state. A positive polar radius is required for the
coordinates to be injective. The profile need not already be invariant. -/

noncomputable section
namespace DSP.PolarTube

abbrev State := Fin 4 → ℝ
abbrev Normal := Fin 3 → ℝ
abbrev Coordinates := Real.Angle × Normal

def head (x : State) : ℂ := ⟨x 0, x 1⟩

def radial (profile : Real.Angle → Normal) (x : Coordinates) : ℝ :=
  profile x.1 0+x.2 0

def rawDecode (profile : Real.Angle → Normal) (x : Coordinates) : State :=
  ![radial profile x*x.1.cos, radial profile x*x.1.sin,
    profile x.1 1+x.2 1, profile x.1 2+x.2 2]

def rawEncode (profile : Real.Angle → Normal) (x : State) : Coordinates :=
  let phase : Real.Angle := (head x).arg
  (phase, ![‖head x‖-profile phase 0, x 2-profile phase 1, x 3-profile phase 2])

theorem head_rawDecode (profile : Real.Angle → Normal) (x : Coordinates) :
    head (rawDecode profile x) =
      (radial profile x:ℂ)*(x.1.cos+x.1.sin*Complex.I) := by
  apply Complex.ext <;> simp [head, rawDecode]

theorem norm_unit (phase : Real.Angle) :
    ‖(phase.cos:ℂ)+phase.sin*Complex.I‖ = 1 := by
  induction phase using Real.Angle.induction_on with
  | _ phase => simp

theorem head_norm (profile : Real.Angle → Normal) (x : Coordinates)
    (h : 0 ≤ radial profile x) : ‖head (rawDecode profile x)‖ = radial profile x := by
  rw [head_rawDecode, norm_mul, norm_unit, mul_one, Complex.norm_real, Real.norm_eq_abs,
    abs_of_nonneg h]

theorem rawEncode_rawDecode (profile : Real.Angle → Normal) (x : Coordinates)
    (h : 0 < radial profile x) : rawEncode profile (rawDecode profile x) = x := by
  have hp : ((head (rawDecode profile x)).arg:Real.Angle) = x.1 := by
    rw [head_rawDecode]
    exact Complex.arg_mul_cos_add_sin_mul_I_coe_angle h x.1
  apply Prod.ext
  · exact hp
  · dsimp only [rawEncode]
    rw [hp, head_norm profile x h.le]
    funext i
    fin_cases i <;> simp [rawDecode, radial]

/-- Decoding an encoded state is exact even on the polar axis. Injectivity
in the reverse direction additionally needs a positive radius. -/
theorem rawDecode_rawEncode (profile : Real.Angle → Normal) (x : State) :
    rawDecode profile (rawEncode profile x) = x := by
  have hr : radial profile (rawEncode profile x) = ‖head x‖ := by simp [radial, rawEncode]
  funext i
  fin_cases i
  · change radial profile (rawEncode profile x)*(rawEncode profile x).1.cos = x 0
    rw [hr]
    exact Complex.norm_mul_cos_arg (head x)
  · change radial profile (rawEncode profile x)*(rawEncode profile x).1.sin = x 1
    rw [hr]
    exact Complex.norm_mul_sin_arg (head x)
  · simp [rawDecode, rawEncode]
  · simp [rawDecode, rawEncode]

theorem head_continuous : Continuous head := by
  have he : head = fun x : State => (x 0:ℂ)+(x 1:ℂ)*Complex.I := by
    funext x
    apply Complex.ext <;> simp [head]
  rw [he]
  fun_prop

theorem rawDecode_continuous (profile : Real.Angle → Normal) (hp : Continuous profile) :
    Continuous (rawDecode profile) := by
  have hc : Continuous (fun x : Coordinates => x.1.cos) := Real.Angle.continuous_cos.comp continuous_fst
  have hs : Continuous (fun x : Coordinates => x.1.sin) := Real.Angle.continuous_sin.comp continuous_fst
  unfold rawDecode radial
  fun_prop

theorem rawEncode_continuousAt (profile : Real.Angle → Normal) (hp : Continuous profile)
    (x : State) (hx : head x ≠ 0) : ContinuousAt (rawEncode profile) x := by
  have hphase : ContinuousAt (fun y : State => ((head y).arg:Real.Angle)) x :=
    (Complex.continuousAt_arg_coe_angle hx).comp head_continuous.continuousAt
  have hprofile := hp.continuousAt.comp hphase
  have hnorm := head_continuous.norm.continuousAt (x := x)
  apply hphase.prodMk
  apply continuousAt_pi.mpr
  intro i
  fin_cases i
  · exact hnorm.sub ((continuous_apply 0).continuousAt.comp hprofile)
  · exact (continuous_apply 2).continuousAt.sub ((continuous_apply 1).continuousAt.comp hprofile)
  · exact (continuous_apply 3).continuousAt.sub ((continuous_apply 2).continuousAt.comp hprofile)

/-- A uniform radial margin makes the whole transverse tube valid, not just
the sampled center curve. -/
theorem radial_pos_of_tube (profile : Real.Angle → Normal) (radius : ℝ)
    (hp : ∀ phase, radius < profile phase 0) (x : Coordinates)
    (hx : ‖x.2‖ ≤ radius) : 0 < radial profile x := by
  have hr : |x.2 0| ≤ radius := (norm_le_pi_norm x.2 0).trans hx
  have hl := (abs_le.mp hr).1
  dsimp [radial]
  linarith [hp x.1]

/-- The linear change of coordinates is exact data, not a rounded matrix
inverse silently identified with its floating-point counterpart. -/
structure Chart where
  linear : State ≃ₗ[ℝ] State
  profile : Real.Angle → Normal

namespace Chart

def decode (chart : Chart) (x : Coordinates) : State :=
  chart.linear.symm (rawDecode chart.profile x)

def encode (chart : Chart) (x : State) : Coordinates :=
  rawEncode chart.profile (chart.linear x)

theorem decode_encode (chart : Chart) (x : State) : chart.decode (chart.encode x) = x := by
  simp [decode, encode, rawDecode_rawEncode]

theorem encode_decode (chart : Chart) (x : Coordinates)
    (h : 0 < radial chart.profile x) : chart.encode (chart.decode x) = x := by
  simp [decode, encode, rawEncode_rawDecode chart.profile x h]

theorem decode_injective (chart : Chart) :
    Set.InjOn chart.decode {x | 0 < radial chart.profile x} := by
  intro x hx y hy he
  have h := congrArg chart.encode he
  simpa only [chart.encode_decode x hx, chart.encode_decode y hy] using h

theorem decode_continuous (chart : Chart) (hp : Continuous chart.profile) :
    Continuous chart.decode :=
  chart.linear.symm.toLinearMap.continuous_of_finiteDimensional.comp (rawDecode_continuous chart.profile hp)

theorem encode_continuousAt (chart : Chart) (hp : Continuous chart.profile)
    (x : State) (hx : head (chart.linear x) ≠ 0) : ContinuousAt chart.encode x :=
  (rawEncode_continuousAt chart.profile hp _ hx).comp chart.linear.toLinearMap.continuous_of_finiteDimensional.continuousAt

def coordinateStep (chart : Chart) (f : State → State) (x : Coordinates) : Coordinates :=
  chart.encode (f (chart.decode x))

/-- Coordinate iteration follows the actual state map, with no averaged
phase dynamics substituted for it. -/
theorem coordinateStep_commutes (chart : Chart) (f : State → State) (x : Coordinates) :
    chart.decode (chart.coordinateStep f x) = f (chart.decode x) :=
  chart.decode_encode _

theorem iterate_commutes (chart : Chart) (f : State → State) (x : Coordinates) (n : ℕ) :
    chart.decode ((chart.coordinateStep f)^[n] x) = f^[n] (chart.decode x) := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [Function.iterate_succ_apply', coordinateStep_commutes, ih]

theorem observations_commute {E : Type*} (chart : Chart) (f : State → State)
    (observe : State → E) (x : Coordinates) (n : ℕ) :
    observe (chart.decode ((chart.coordinateStep f)^[n] x)) = observe (f^[n] (chart.decode x)) := by
  rw [iterate_commutes]

end Chart
end DSP.PolarTube
