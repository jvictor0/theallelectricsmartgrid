import DSP.Analysis.PhasePower.DiscreteBounds
import DSP.Analysis.CenteredTaylor
import Mathlib.Analysis.Complex.RealDeriv
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

noncomputable section
namespace DSP.PhasePower.Discrete

def rotation (angle : ℝ) : ℂ := Complex.exp (Complex.I*(angle:ℂ))

theorem rotation_norm (angle : ℝ) : ‖rotation angle‖ = 1 :=
  Complex.norm_exp_I_mul_ofReal angle

theorem rotation_add (a b : ℝ) : rotation (a+b) = rotation a*rotation b := by
  simp only [rotation,Complex.ofReal_add,mul_add,Complex.exp_add]

/-- Exact global rotation remainder, including its factor 1/2. -/
theorem rotation_remainder (angle : ℝ) :
    ‖rotation angle-1-Complex.I*(angle:ℂ)‖ ≤ angle^2/2 := by
  let f : ℝ → ℂ := fun t => rotation (t*angle)
  let v : ℂ := Complex.I*(angle:ℂ)
  have hf (t : ℝ) : HasDerivAt f (f t*v) t := by
    have h := (((hasDerivAt_id t).ofReal_comp.mul_const (angle:ℂ)).const_mul Complex.I).cexp
    simpa only [f,v,rotation,Complex.ofReal_mul,Complex.ofReal_one,id_eq,mul_one,one_mul] using h
  have hf' (t : ℝ) : HasDerivAt (fun t => f t*v) (f t*v^2) t := by
    exact ((hf t).mul_const v).congr_deriv (by ring)
  have h := DSP.CenteredTaylor.remainder_one f (fun t => f t*v) (fun t => f t*v^2) (angle^2)
    (fun t _ => hf t) (fun t _ => hf' t) (fun t _ => by
      simp only [norm_mul,norm_pow,f,v,rotation_norm,Complex.norm_I,Complex.norm_real,
        Real.norm_eq_abs,one_mul,sq_abs,le_refl])
  simpa only [f,v,zero_mul,one_mul,rotation,Complex.ofReal_zero,mul_zero,Complex.exp_zero] using h

theorem rotation_displacement (angle : ℝ) : ‖rotation angle-1‖ ≤ |angle| :=
  Real.norm_exp_I_mul_ofReal_sub_one_le

theorem flow_velocity (c : ℝ) (z : ℂ) (hz : ‖z‖ = 1) :
    flow c z = Complex.I*z*(velocity c z:ℂ) := by
  have hu : z.re^2+z.im^2 = 1 := by
    have h : z.re*z.re+z.im*z.im = 1 := by
      rw [← Complex.normSq_apply,Complex.normSq_eq_norm_sq,hz]; norm_num
    nlinarith
  apply Complex.ext
  · simp [flow,velocity,Complex.mul_re,Complex.mul_im,pow_two]
    nlinarith [congrArg (fun r : ℝ => c*r) hu]
  · simp [flow,velocity,Complex.mul_re,Complex.mul_im,pow_two]
    ring

theorem velocity_bound (c : ℝ) (hc : 0 ≤ c) (z : ℂ) (hz : ‖z‖ = 1) :
    |velocity c z| ≤ (1+c)^2 := by
  have hi := Complex.abs_im_le_norm z
  rw [hz] at hi
  have hv : 0 ≤ velocity c z := by
    unfold velocity
    nlinarith [(abs_le.mp hi).1,sq_nonneg (1-c)]
  rw [abs_of_nonneg hv]
  unfold velocity
  nlinarith [(abs_le.mp hi).2]

theorem phase_step (c : ℝ) (hc : 0 ≤ c) (h : ℝ) (hh : 0 ≤ h)
    (angle next phaseError : ℝ)
    (he : |next-angle-h*velocity c (rotation angle)| ≤ phaseError) :
    let D := h*(1+c)^2+phaseError
    ‖rotation next-rotation angle‖ ≤ D ∧
    ‖rotation next-rotation angle-(h:ℂ)*flow c (rotation angle)‖ ≤ phaseError+D^2/2 := by
  let delta := next-angle
  let D := h*(1+c)^2+phaseError
  have hd : |delta| ≤ D := by
    have ht := abs_add_le (delta-h*velocity c (rotation angle)) (h*velocity c (rotation angle))
    rw [sub_add_cancel,abs_mul,abs_of_nonneg hh] at ht
    exact ht.trans (add_le_add he (mul_le_mul_of_nonneg_left
      (velocity_bound c hc _ (rotation_norm _)) hh)) |>.trans_eq (by dsimp [D]; ring)
  have hn : rotation next = rotation angle*rotation delta := by
    rw [← rotation_add]
    congr 1
    dsimp [delta]
    ring
  constructor
  · rw [hn,← mul_sub_one,norm_mul,rotation_norm,one_mul]
    exact (rotation_displacement delta).trans hd
  · have hid : rotation next-rotation angle-(h:ℂ)*flow c (rotation angle) =
        rotation angle*(rotation delta-1-Complex.I*(delta:ℂ))+
          Complex.I*rotation angle*((delta-h*velocity c (rotation angle):ℝ):ℂ) := by
      rw [hn,flow_velocity c _ (rotation_norm _)]
      push_cast
      ring
    rw [hid]
    apply (norm_add_le _ _).trans
    simp only [norm_mul,rotation_norm,Complex.norm_I,one_mul,Complex.norm_real,Real.norm_eq_abs]
    have hs : delta^2 ≤ D^2 := by
      have hb := pow_le_pow_left₀ (abs_nonneg delta) hd 2
      simpa only [sq_abs] using hb
    exact (add_le_add ((rotation_remainder delta).trans (div_le_div_of_nonneg_right hs (by norm_num))) he).trans_eq
      (by dsimp [D]; ring)

end DSP.PhasePower.Discrete
