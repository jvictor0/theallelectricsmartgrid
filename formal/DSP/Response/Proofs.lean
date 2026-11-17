import DSP.Response
import DSP.ResponseFrequency.Proofs
import DSP.IfLess.Real
import DSP.Transfer.Bilinear

noncomputable section
namespace DSP.Response

@[simp] theorem normSquared_eq (r i : ℝ) :
    normSquared r i = Complex.normSq (⟨r, i⟩ : ℂ) := by
  simp [normSquared, fma_eq, Complex.normSq_apply]

theorem ratio_eq (n d : ℝ) (hn : 0 ≤ n) : ratio n d = n / d := by
  by_cases h : 0 < n
  · simp [ratio, IfLess.ifLess, h]
  · have hz : n = 0 := le_antisymm (le_of_not_gt h) hn
    simp [ratio, hz, IfLess.ifLess]

/-- Reusable cascade rule: the two independently proved response kernels
compose without repeating trigonometry or expanding the combined transfer. -/
theorem cascade (m1 m2 : ℝ) (h1 h2 : ℂ)
    (p1 : m1 = Complex.normSq h1) (p2 : m2 = Complex.normSq h2) :
    m1 * m2 = Complex.normSq (h1 * h2) := by
  rw [Complex.normSq_mul, p1, p2]

/-- Squared magnitudes do not add under mixing. The cross term retains phase. -/
theorem mix (a b : ℂ) :
    Complex.normSq (a + b) = Complex.normSq a + Complex.normSq b +
      2 * (a.re * b.re + a.im * b.im) := by
  simp [Complex.normSq_apply]
  ring

/-- A nonzero common scale can be cancelled from homogeneous coordinates. -/
theorem onePole_scale (g a b t : ℂ) (ht : t ≠ 0) :
    onePole g (t * a) (t * b) = onePole g a b := by
  unfold onePole
  have h : t * a + g * (t * b) = t * (a + g * b) := by ring
  rw [h, show g * (t * b) = t * (g * b) by ring, mul_div_mul_left _ _ ht]

theorem highPole_scale (g a b t : ℂ) (ht : t ≠ 0) :
    highPole g (t * a) (t * b) = highPole g a b := by
  unfold highPole
  rw [show t * a + g * (t * b) = t * (a + g * b) by ring,
    mul_div_mul_left _ _ ht]

theorem svf_scale (g k l m h a b t : ℂ) (ht : t ≠ 0) :
    svf g k l m h (t * a) (t * b) = svf g k l m h a b := by
  unfold svf
  have hn : l * (g * (t*b) * (g * (t*b))) + m * (g*(t*b)) * (t*a) + h * (t*a*(t*a)) =
      t^2 * (l*(g*b*(g*b)) + m*(g*b)*a + h*(a*a)) := by ring
  have hd : t*a*(t*a) + k*(g*(t*b))*(t*a) + g*(t*b)*(g*(t*b)) =
      t^2 * (a*a + k*(g*b)*a + g*b*(g*b)) := by ring
  rw [hn, hd, mul_div_mul_left _ _ (pow_ne_zero 2 ht)]

theorem ladder_scale (g1 g2 g3 g4 k a b t : ℂ) (ht : t ≠ 0) :
    ladder g1 g2 g3 g4 k (t*a) (t*b) = ladder g1 g2 g3 g4 k a b := by
  dsimp only [ladder]
  have hn : g1*(t*b)*(g2*(t*b))*(g3*(t*b))*(g4*(t*b)) =
      t^4 * (g1*b*(g2*b)*(g3*b)*(g4*b)) := by ring
  have hd : (t*a+g1*(t*b))*(t*a+g2*(t*b))*(t*a+g3*(t*b))*(t*a+g4*(t*b)) +
      k*(g1*(t*b)*(g2*(t*b))*(g3*(t*b))*(g4*(t*b))) =
      t^4 * ((a+g1*b)*(a+g2*b)*(a+g3*b)*(a+g4*b) + k*(g1*b*(g2*b)*(g3*b)*(g4*b))) := by ring
  rw [hd, hn, mul_div_mul_left _ _ (pow_ne_zero 4 ht)]

/-- Half-angle coordinates remove the bilinear division even at Nyquist.
This is a finite algebraic identity, not evaluation of an infinite series. -/
theorem halfAngle_coordinates (f : ℝ) :
    ∃ t : ℂ, t ≠ 0 ∧
      1 - ResponseFrequency.unitDelay f = t * (⟨0, Real.sin (Real.pi*f)⟩ : ℂ) ∧
      1 + ResponseFrequency.unitDelay f = t * (Real.cos (Real.pi*f) : ℂ) := by
  let s := Real.sin (Real.pi*f)
  let c := Real.cos (Real.pi*f)
  have hsc : s*s + c*c = 1 := by simpa [s, c, pow_two] using Real.sin_sq_add_cos_sq (Real.pi*f)
  refine ⟨⟨2*c, -(2*s)⟩, ?_, ?_, ?_⟩
  · intro h
    have hr := congrArg Complex.re h
    have hi := congrArg Complex.im h
    simp at hr hi
    have : s = 0 := by linarith
    have : c = 0 := by linarith
    nlinarith
  · apply Complex.ext <;> simp [ResponseFrequency.unitDelay_re, ResponseFrequency.unitDelay_im,
      Real.cos_two_mul_eq_one_sub, Real.sin_two_mul, s, c] <;> ring
  · apply Complex.ext
    · change 1 + (ResponseFrequency.unitDelay f).re = 2*c*c - -(2*s)*0
      rw [ResponseFrequency.unitDelay_re, Real.cos_two_mul_eq_one_sub]
      change 1 + (1 - 2*s^2) = 2*c*c - -(2*s)*0
      nlinarith
    · change 0 + (ResponseFrequency.unitDelay f).im = 2*c*0 + -(2*s)*c
      rw [ResponseFrequency.unitDelay_im, Real.sin_two_mul]
      dsimp [s, c]
      ring

end DSP.Response
