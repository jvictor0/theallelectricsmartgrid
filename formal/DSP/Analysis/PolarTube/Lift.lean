import DSP.Analysis.PolarTube.Phase

/-! A global degree-one real lift of the phase update, when the updated polar
head stays aligned with the input phase. Checking only positive local phase
derivatives would not rule out a map that winds around the circle twice. -/

noncomputable section
namespace DSP.PolarTube

def unitPhase (θ : Real.Angle) : ℂ := θ.cos+θ.sin*Complex.I

theorem unitPhase_ne_zero (θ : Real.Angle) : unitPhase θ ≠ 0 := by
  apply norm_ne_zero_iff.mp
  rw [unitPhase, norm_unit]
  norm_num

theorem unitPhase_continuous : Continuous unitPhase := by
  exact (Complex.continuous_ofReal.comp Real.Angle.continuous_cos).add
    ((Complex.continuous_ofReal.comp Real.Angle.continuous_sin).mul_const Complex.I)

theorem relative_real (z : ℂ) (θ : Real.Angle) :
    (z/unitPhase θ).re = z.re*θ.cos+z.im*θ.sin := by
  have hn : Complex.normSq (unitPhase θ) = 1 := by
    rw [Complex.normSq_eq_norm_sq, unitPhase, norm_unit]
    norm_num
  rw [Complex.div_re, hn]
  simp [unitPhase]

theorem relative_safe (z : ℂ) (θ : Real.Angle)
    (h : 0 < z.re*θ.cos+z.im*θ.sin) : z/unitPhase θ ∈ Complex.slitPlane :=
  Or.inl (by simpa only [relative_real] using h)

namespace Chart

def nextHead (chart : Chart) (f : State → State) (θ : Real.Angle) (r : Normal) : ℂ :=
  head (chart.linear (f (chart.decode (θ,r))))

def phaseLift (chart : Chart) (f : State → State) (θ : ℝ) (r : Normal) : ℝ :=
  θ+(chart.nextHead f (θ:Real.Angle) r/unitPhase (θ:Real.Angle)).arg

theorem phaseLift_agrees (chart : Chart) (f : State → State) (θ : ℝ) (r : Normal)
    (hne : chart.nextHead f (θ:Real.Angle) r ≠ 0) :
    (chart.phaseLift f θ r:Real.Angle) = (chart.coordinateStep f ((θ:Real.Angle),r)).1 := by
  rw [phaseLift, Real.Angle.coe_add,
    Complex.arg_div_coe_angle hne (unitPhase_ne_zero (θ:Real.Angle))]
  have hu : ((unitPhase (θ:Real.Angle)).arg:Real.Angle) = θ :=
    Complex.arg_cos_add_sin_mul_I_coe_angle _
  rw [hu]
  change (θ:Real.Angle)+(↑(chart.nextHead f (θ:Real.Angle) r).arg-(θ:Real.Angle)) =
    ↑(chart.nextHead f (θ:Real.Angle) r).arg
  abel

/-- The lift has degree one exactly, not merely at sampled phase points. -/
theorem phaseLift_period (chart : Chart) (f : State → State) (θ : ℝ) (r : Normal) :
    chart.phaseLift f (θ+2*Real.pi) r = chart.phaseLift f θ r+2*Real.pi := by
  simp only [phaseLift, Real.Angle.coe_add, Real.Angle.coe_two_pi, add_zero]
  ring

theorem nextHead_continuous (chart : Chart) (f : State → State)
    (hp : Continuous chart.profile) (hf : Continuous f) :
    Continuous (fun x : Coordinates => chart.nextHead f x.1 x.2) :=
  head_continuous.comp (chart.linear.toLinearMap.continuous_of_finiteDimensional.comp
    (hf.comp (chart.decode_continuous hp)))

/-- Alignment is the uniform domain check used by the proposal program.
It keeps the relative phase off the branch cut over the entire domain. -/
theorem phaseLift_continuousOn (chart : Chart) (f : State → State)
    (hp : Continuous chart.profile) (hf : Continuous f) (domain : Set (ℝ × Normal))
    (ha : ∀ x ∈ domain,
      0 < (chart.nextHead f (x.1:Real.Angle) x.2).re*(x.1:Real.Angle).cos+
        (chart.nextHead f (x.1:Real.Angle) x.2).im*(x.1:Real.Angle).sin) :
    ContinuousOn (fun x : ℝ × Normal => chart.phaseLift f x.1 x.2) domain := by
  have hc : Continuous (fun x : ℝ × Normal => ((x.1:Real.Angle),x.2)) :=
    (Real.Angle.continuous_coe.comp continuous_fst).prodMk continuous_snd
  have hh := (chart.nextHead_continuous f hp hf).comp hc
  have hu := unitPhase_continuous.comp (Real.Angle.continuous_coe.comp
    (continuous_fst : Continuous (fun x : ℝ × Normal => x.1)))
  have hq : Continuous (fun x : ℝ × Normal =>
      chart.nextHead f (x.1:Real.Angle) x.2/unitPhase (x.1:Real.Angle)) := by
    simpa only [Function.comp_def, Pi.div_apply] using!
      hh.div hu (fun x => unitPhase_ne_zero (x.1:Real.Angle))
  intro x hx
  have harg : ContinuousAt (fun y : ℝ × Normal =>
      (chart.nextHead f (y.1:Real.Angle) y.2/unitPhase (y.1:Real.Angle)).arg) x :=
    ContinuousAt.comp (f := fun y : ℝ × Normal =>
      chart.nextHead f (y.1:Real.Angle) y.2/unitPhase (y.1:Real.Angle))
      (Complex.continuousAt_arg (relative_safe _ _ (ha x hx))) hq.continuousAt
  exact (continuous_fst.continuousAt.add harg).continuousWithinAt

end Chart
end DSP.PolarTube
