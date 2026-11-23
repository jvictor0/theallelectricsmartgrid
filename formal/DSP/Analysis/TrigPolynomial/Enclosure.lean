import DSP.Analysis.TrigPolynomial
import DSP.Analysis.IntervalArithmetic
import DSP.Analysis.IntervalTrig

/-! Rational enclosures of exact finite Fourier profiles. Differentiated
profiles and correction potentials use the same checker by passing their
coefficient derivatives. Real trigonometric functions retain their proved
Taylor remainders; no binary64 transcendental is trusted. -/

namespace DSP.TrigPolynomial
open LeanCert.Core DSP.IntervalArithmetic
variable {count : ℕ}

def enclosure (p : TrigPolynomial count) (angle : IntervalRat) (depth : ℕ) : IntervalRat :=
  IntervalArithmetic.sum (fun i =>
    (IntervalTrig.cosine (angle.scale i.val) depth).scale (p.cosine i) |>.add
      ((IntervalTrig.sine (angle.scale i.val) depth).scale (p.sine i)))

theorem mem_enclosure (p : TrigPolynomial count) (angle : IntervalRat) (depth : ℕ)
    (θ : ℝ) (hθ : θ ∈ angle) : p.eval (θ:Real.Angle) ∈ p.enclosure angle depth := by
  rw [p.eval_coe]
  apply mem_sum
  intro i
  have hi : (i.val:ℝ)*θ ∈ angle.scale i.val := by
    simpa only [Rat.cast_natCast] using IntervalRat.mem_scale (i.val:ℚ) hθ
  exact IntervalRat.mem_add
    (IntervalRat.mem_scale (p.cosine i) (IntervalTrig.cosine_mem _ depth _ hi))
    (IntervalRat.mem_scale (p.sine i) (IntervalTrig.sine_mem _ depth _ hi))

end DSP.TrigPolynomial
