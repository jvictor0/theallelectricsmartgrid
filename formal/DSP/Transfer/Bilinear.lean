import DSP.Step
import Mathlib.RingTheory.LaurentSeries
import Mathlib.RingTheory.PowerSeries.Inverse
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.LinearCombination

/-! Stream semantics for state-only TPT filters.

A causal stream u is the power series Σ u n qⁿ in the delay q, embedded in
the field of Laurent series so that transfers can be divided. The bilinear map
sends the analog variable s to σ = (1 - q) / (1 + q); it is a ring
homomorphism, so series, parallel and feedback connections of analog
prototypes map to the same connections of their digital realizations.

`Realizes p initial f H` says: starting from `initial`, the stream of field
`f` read after each step is H times the input stream, for every input. The
coefficient fields are part of the state, so `initial` fixes them as well.
-/

noncomputable section

namespace DSP.Bilinear

open Polynomial

variable {K : Type} [Field K]

abbrev Stream (K : Type) [Field K] := LaurentSeries K

def embed : PowerSeries K →+* Stream K := HahnSeries.ofPowerSeries ℤ K

def ser (u : ℕ → K) : Stream K := embed (PowerSeries.mk u)

/-- The unit delay q. -/
def delay : Stream K := embed PowerSeries.X

def const (c : K) : Stream K := embed (PowerSeries.C c)

def sigmaSeries : PowerSeries K := (1 - PowerSeries.X) * (1 + PowerSeries.X)⁻¹

/-- The bilinear image of the analog variable: σ = (1 - q) / (1 + q). -/
def sigma : Stream K := embed sigmaSeries

/-- Substitute s := σ into an analog polynomial. -/
def bil : K[X] →+* Stream K := embed.comp (aeval (sigmaSeries (K := K))).toRingHom

/-- The bilinear image of the analog rational transfer N(s) / M(s). -/
def analog (N M : K[X]) : Stream K := bil N / bil M

theorem embed_injective : Function.Injective (embed (K := K)) :=
  HahnSeries.ofPowerSeries_injective

theorem embed_ne_zero (p : PowerSeries K) (h : PowerSeries.constantCoeff p ≠ 0) : embed p ≠ 0 := by
  intro hp
  apply h
  rw [← map_zero embed] at hp
  rw [embed_injective hp, map_zero]

@[simp] theorem bil_X : bil (X : K[X]) = sigma := by
  simp [bil, sigma]

@[simp] theorem bil_C (c : K) : bil (Polynomial.C c) = const c := by
  simp [bil, const]

theorem constantCoeff_sigmaSeries : PowerSeries.constantCoeff (sigmaSeries (K := K)) = 1 := by
  simp [sigmaSeries, PowerSeries.constantCoeff_inv]

/-- Well-posedness of a zero-delay loop is M(1) ≠ 0, where M is its analog
denominator: σ has constant term 1, so bil M has constant term M(1). -/
theorem bil_ne_zero (M : K[X]) (h : M.eval 1 ≠ 0) : bil M ≠ 0 := by
  apply embed_ne_zero
  change PowerSeries.constantCoeff (aeval sigmaSeries M) ≠ 0
  rw [aeval_def, hom_eval₂, constantCoeff_sigmaSeries]
  have hc : (PowerSeries.constantCoeff.comp (algebraMap K (PowerSeries K))) = RingHom.id K := by
    ext c
    simp
  rw [hc, eval₂_id]
  exact h

theorem one_add_delay_ne_zero : (1 : Stream K) + delay ≠ 0 := by
  have h := embed_ne_zero (1 + PowerSeries.X : PowerSeries K) (by simp)
  simpa [delay] using h

theorem sigma_mul : sigma * (1 + delay) = (1 : Stream K) - delay := by
  have h := PowerSeries.inv_mul_cancel (1 + PowerSeries.X : PowerSeries K) (by simp)
  simp only [sigma, delay, sigmaSeries, ← map_one embed, ← map_add, ← map_mul, ← map_sub]
  rw [mul_assoc, h, mul_one]

theorem ser_add (u v : ℕ → K) : ser (fun n => u n + v n) = ser u + ser v := by
  rw [ser, ser, ser, ← map_add]
  rfl

theorem ser_sub (u v : ℕ → K) : ser (fun n => u n - v n) = ser u - ser v := by
  rw [ser, ser, ser, ← map_sub]
  rfl

theorem ser_mul (c : K) (u : ℕ → K) : ser (fun n => c * u n) = const c * ser u := by
  rw [ser, ser, const, ← map_mul]
  congr 1
  ext n
  simp [PowerSeries.coeff_C_mul]

theorem ser_congr {u v : ℕ → K} (h : ∀ n, u n = v n) : ser u = ser v := by
  rw [funext h]

/-- A stream is its first sample plus the delayed remainder. -/
theorem ser_shift (u : ℕ → K) : ser u = const (u 0) + delay * ser (fun n => u (n + 1)) := by
  rw [ser, ser, const, delay, ← map_mul, ← map_add]
  congr 1
  ext n
  cases n with
  | zero => simp
  | succ n => simp [PowerSeries.coeff_succ_X_mul]

@[simp] theorem const_zero : const (0 : K) = 0 := by
  simp [const]

theorem const_add (a b : K) : const (a + b) = const a + const b := by
  simp [const]

theorem const_mul (a b : K) : const (a * b) = const a * const b := by
  simp [const]

/-- The trapezoidal integrator. From a zero state, ic n+1 = 2 v n - ic n and
v n = ic n + g w n make v the integral of g w: σ V = g W. -/
theorem integrator (ic v w : ℕ → K) (g : K) (hzero : ic 0 = 0)
    (hstate : ∀ n, ic (n + 1) = 2 * v n - ic n) (hout : ∀ n, v n = ic n + g * w n) :
    sigma * ser v = const g * ser w := by
  have hshift := ser_shift ic
  rw [hzero, const_zero, zero_add, ser_congr hstate, ser_sub, ser_mul] at hshift
  have hv : ser v = ser ic + const g * ser w := by
    rw [← ser_mul, ← ser_add]
    exact ser_congr hout
  have htwo : const (2 : K) = 2 := by
    rw [const, show (PowerSeries.C (2 : K)) = 2 from map_ofNat _ 2, map_ofNat]
  rw [htwo] at hshift
  apply mul_right_cancel₀ (one_add_delay_ne_zero (K := K))
  linear_combination ser v * sigma_mul (K := K) + (1 + delay) * hv + hshift

section Realization

variable {S T : Type}

/-- The field f, read after each sample, from a given initial state. -/
def response (p : Step S (Sample K)) (u : ℕ → K) (initial : S) (f : S → K) (n : ℕ) : K :=
  f (Step.states p (fun k => ⟨u k⟩) initial (n + 1))

def Realizes (p : Step S (Sample K)) (initial : S) (f : S → K) (H : Stream K) : Prop :=
  ∀ u, ser (response p u initial f) = H * ser u

/-- A component that sees the composite's input realizes the same transfer
inside the composite. -/
theorem Realizes.project (p : Step S (Sample K)) (q : Step T (Sample K)) (π : S → T)
    (h : ∀ s x, π (p s x) = q (π s) x) {initial : S} {f : T → K} {H : Stream K}
    (hq : Realizes q (π initial) f H) : Realizes p initial (fun s => f (π s)) H := by
  intro u
  have hs := Step.states_map p q π (fun _ x => x) h (fun k => ⟨u k⟩) initial
  have hr : response p u initial (fun s => f (π s)) = response q u (π initial) f := by
    funext n
    simp only [response]
    rw [hs]
  rw [hr]
  exact hq u

/-- A component driven by another stored field d of the composite's new state
multiplies d's stream by its own transfer. -/
theorem driven (p : Step S (Sample K)) (q : Step T (Sample K)) (π : S → T) (d : S → K)
    (h : ∀ s x, π (p s x) = q (π s) ⟨d (p s x)⟩) {initial : S} {f : T → K} {H : Stream K}
    (hq : Realizes q (π initial) f H) (u : ℕ → K) :
    ser (response p u initial (fun s => f (π s))) = H * ser (response p u initial d) := by
  have hs := Step.states_map p q π (fun s x => ⟨d (p s x)⟩) h (fun k => ⟨u k⟩) initial
  have hr : response p u initial (fun s => f (π s)) =
      response q (response p u initial d) (π initial) f := by
    funext n
    simp only [response]
    rw [hs]
    rfl
  rw [hr]
  exact hq _

/-- Series connection through a stored field: transfers multiply. -/
theorem Realizes.cascade (p : Step S (Sample K)) (q : Step T (Sample K)) (π : S → T) (d : S → K)
    (h : ∀ s x, π (p s x) = q (π s) ⟨d (p s x)⟩) {initial : S} {f : T → K} {H G : Stream K}
    (hd : Realizes p initial d G) (hq : Realizes q (π initial) f H) :
    Realizes p initial (fun s => f (π s)) (H * G) := by
  intro u
  rw [driven p q π d h hq u, hd u, mul_assoc]

theorem Realizes.add {p : Step S (Sample K)} {initial : S} {f h : S → K} {F G : Stream K}
    (hf : Realizes p initial f F) (hh : Realizes p initial h G) :
    Realizes p initial (fun s => f s + h s) (F + G) := by
  intro u
  have hs := ser_add (response p u initial f) (response p u initial h)
  rw [show response p u initial (fun s => f s + h s) =
    fun n => response p u initial f n + response p u initial h n from rfl, hs, hf u, hh u, add_mul]

theorem Realizes.sub {p : Step S (Sample K)} {initial : S} {f h : S → K} {F G : Stream K}
    (hf : Realizes p initial f F) (hh : Realizes p initial h G) :
    Realizes p initial (fun s => f s - h s) (F - G) := by
  intro u
  have hs := ser_sub (response p u initial f) (response p u initial h)
  rw [show response p u initial (fun s => f s - h s) =
    fun n => response p u initial f n - response p u initial h n from rfl, hs, hf u, hh u, sub_mul]

theorem Realizes.mul {p : Step S (Sample K)} {initial : S} {f : S → K} {F : Stream K}
    (c : S → K) (hc : ∀ s x, c (p s x) = c s) (hf : Realizes p initial f F) :
    Realizes p initial (fun s => c s * f s) (const (c initial) * F) := by
  intro u
  have hcn : ∀ n, c (Step.states p (fun k => ⟨u k⟩) initial (n + 1)) = c initial :=
    fun n => Step.states_invariant p c hc _ initial (n + 1)
  have hr : response p u initial (fun s => c s * f s) =
      fun n => c initial * response p u initial f n := by
    funext n
    simp only [response, hcn]
  rw [hr, ser_mul, hf u, mul_assoc]

/-- A stored field equal to the input minus another stored field. -/
theorem Realizes.complement {p : Step S (Sample K)} {initial : S} {f d : S → K} {H : Stream K}
    (h : ∀ s x, d (p s x) = x.x - f (p s x)) (hf : Realizes p initial f H) :
    Realizes p initial d (1 - H) := by
  intro u
  have hr : response p u initial d = fun n => u n - response p u initial f n := by
    funext n
    exact h _ _
  rw [hr, ser_sub, hf u]
  ring

/-- Stored fields that agree on every reachable state realize the same
transfer; reachability is witnessed by an invariant. -/
theorem Realizes.congr_of_invariant {p : Step S (Sample K)} {initial : S} {f h : S → K}
    {H : Stream K} (I : S → Prop) (hzero : I initial) (hstep : ∀ s x, I s → I (p s x))
    (heq : ∀ s x, I s → h (p s x) = f (p s x)) (hf : Realizes p initial f H) :
    Realizes p initial h H := by
  intro u
  have hI : ∀ n, I (Step.states p (fun k => ⟨u k⟩) initial n) := by
    intro n
    induction n with
    | zero => exact hzero
    | succ n ih => exact hstep _ _ ih
  have hr : response p u initial h = response p u initial f := by
    funext n
    exact heq _ _ (hI n)
  rw [hr]
  exact hf u

/-- Pointwise equal stored fields realize the same transfer. -/
theorem Realizes.congr {p : Step S (Sample K)} {initial : S} {f h : S → K} {H : Stream K}
    (heq : ∀ s x, h (p s x) = f (p s x)) (hf : Realizes p initial f H) :
    Realizes p initial h H :=
  hf.congr_of_invariant (fun _ => True) trivial (fun _ _ _ => trivial) (fun s x _ => heq s x)

theorem Realizes.transfer_eq {p : Step S (Sample K)} {initial : S} {f : S → K} {H G : Stream K}
    (hf : Realizes p initial f H) (heq : H = G) : Realizes p initial f G :=
  heq ▸ hf

end Realization

/-- σ + g is the bilinear image of the one-pole denominator s + g. -/
theorem bil_onePole (g : K) : bil (X + Polynomial.C g) = sigma + const g := by
  simp

/-! Analog rational algebra, transported by the bilinear homomorphism. -/

theorem analog_mul (N₁ M₁ N₂ M₂ : K[X]) :
    analog N₁ M₁ * analog N₂ M₂ = analog (N₁ * N₂) (M₁ * M₂) := by
  simp only [analog, map_mul, div_mul_div_comm]

theorem analog_add (N₁ N₂ M : K[X]) : analog N₁ M + analog N₂ M = analog (N₁ + N₂) M := by
  simp only [analog, map_add, add_div]

theorem const_mul_analog (c : K) (N M : K[X]) :
    const c * analog N M = analog (Polynomial.C c * N) M := by
  simp only [analog, map_mul, bil_C, mul_div_assoc]

theorem one_sub_analog (N M : K[X]) (hM : bil M ≠ 0) : 1 - analog N M = analog (M - N) M := by
  simp only [analog, map_sub]
  field_simp

/-- Clear the denominator of a realized analog transfer. -/
theorem cleared {Y U : Stream K} {N M : K[X]} (hM : bil M ≠ 0) (h : Y = analog N M * U) :
    Y * bil M = bil N * U := by
  rw [h, analog]
  field_simp

/-- Conclude a realization from a cleared stream equation. -/
theorem of_cleared {Y U : Stream K} {N M : K[X]} (hM : bil M ≠ 0) (h : Y * bil M = bil N * U) :
    Y = analog N M * U := by
  rw [analog, div_mul_eq_mul_div, eq_div_iff hM]
  exact h

end DSP.Bilinear
