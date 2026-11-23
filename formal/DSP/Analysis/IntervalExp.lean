import LeanCert.Core.IntervalDyadic
import LeanCert.Core.IntervalRat.Taylor

/-! Fixed-precision, outward-rounded exponential certificates. Rounding after
each operation prevents the exact-rational Taylor denominators from exploding
during kernel reduction. Every rounded operation uses LeanCert's containment
theorem; the Taylor remainder is the library's proved real remainder. -/

namespace DSP.IntervalExp
open LeanCert.Core

abbrev prec : ℤ := -80

def rational (q : ℚ) : IntervalDyadic :=
  IntervalDyadic.ofIntervalRat (IntervalRat.singleton q) prec

theorem mem_rational (q : ℚ) : (q:ℝ) ∈ rational q :=
  IntervalDyadic.mem_ofIntervalRat (IntervalRat.mem_singleton q) prec (by norm_num)

def add (a b : IntervalDyadic) : IntervalDyadic := (a.add b).roundOut prec
def mul (a b : IntervalDyadic) : IntervalDyadic := (a.mul b).roundOut prec

theorem mem_add {x y : ℝ} {a b : IntervalDyadic} (hx : x ∈ a) (hy : y ∈ b) :
    x+y ∈ add a b := IntervalDyadic.roundOut_contains (IntervalDyadic.mem_add hx hy) prec
theorem mem_mul {x y : ℝ} {a b : IntervalDyadic} (hx : x ∈ a) (hy : y ∈ b) :
    x*y ∈ mul a b := IntervalDyadic.roundOut_contains (IntervalDyadic.mem_mul hx hy) prec

def power (a : IntervalDyadic) : ℕ → IntervalDyadic
  | 0 => rational 1
  | n+1 => mul (power a n) a

theorem mem_power {x : ℝ} {a : IntervalDyadic} (hx : x ∈ a) (n : ℕ) :
    x^n ∈ power a n := by
  induction n with
  | zero => simpa [power] using mem_rational 1
  | succ n ih => simpa only [power, pow_succ] using mem_mul ih hx

def series (a : IntervalDyadic) : ℕ → IntervalDyadic
  | 0 => rational 0
  | n+1 => add (series a n) (mul (rational (1/n.factorial)) (power a n))

theorem mem_series {x : ℝ} {a : IntervalDyadic} (hx : x ∈ a) (n : ℕ) :
    ∑ i ∈ Finset.range n, (1/(i.factorial:ℝ))*x^i ∈ series a n := by
  induction n with
  | zero => simpa [series] using mem_rational 0
  | succ n ih =>
    rw [Finset.sum_range_succ]
    exact mem_add ih (mem_mul (by simpa using mem_rational (1/(n.factorial:ℚ))) (mem_power hx n))

def small (q : ℚ) (depth : ℕ) : IntervalDyadic :=
  add (series (rational q) (depth+1))
    (IntervalDyadic.ofIntervalRat
      (IntervalRat.expRemainderBoundComputable ⟨-1, 1, by norm_num⟩ depth) prec)

theorem mem_small (q : ℚ) (depth : ℕ) (hq : |q| ≤ 1) : Real.exp q ∈ small q depth := by
  have hx : (q:ℝ) ∈ (⟨-1, 1, by norm_num⟩ : IntervalRat) := by
    rw [IntervalRat.mem_def]
    exact_mod_cast abs_le.mp hq
  have ht := IntervalRat.exp_taylor_remainder_in_interval hx depth
  simp only [IntervalRat.iteratedDeriv_exp_zero] at ht
  have hp := mem_series (mem_rational q) (depth+1)
  have he := mem_add hp (IntervalDyadic.mem_ofIntervalRat ht prec (by norm_num))
  simpa only [small, add_sub_cancel] using he

def squareIterate (a : IntervalDyadic) : ℕ → IntervalDyadic
  | 0 => a
  | n+1 => let v := squareIterate a n; mul v v

theorem mem_squareIterate {x : ℝ} {a : IntervalDyadic} (hx : x ∈ a) (n : ℕ) :
    x^(2^n) ∈ squareIterate a n := by
  induction n with
  | zero => simpa [squareIterate] using hx
  | succ n ih =>
    have h := mem_mul ih ih
    simpa only [squareIterate, pow_succ, pow_mul, pow_two, pow_zero, one_mul] using h

/-- Fixed reduction by 64 suffices for the ladder certificate arguments.
Larger arguments are rejected by the explicit domain premise. -/
def point (q : ℚ) (depth : ℕ) : IntervalRat :=
  (squareIterate (small (q/64) depth) 6).toIntervalRat

theorem mem_point (q : ℚ) (depth : ℕ) (hq : |q| ≤ 64) : Real.exp q ∈ point q depth := by
  have hsmall : |q/64| ≤ 1 := by rw [abs_div]; norm_num; linarith
  have h := mem_squareIterate (mem_small (q/64) depth hsmall) 6
  have he : Real.exp (q:ℝ) = Real.exp ((q/64:ℚ):ℝ)^64 := by
    rw [← Real.exp_nat_mul]
    congr 1
    push_cast
    ring
  apply IntervalDyadic.mem_toIntervalRat.mp
  rw [he]
  convert h using 1
  norm_num

end DSP.IntervalExp
