import DSP.Filters.TPTSVF
import DSP.Transfer.Bilinear

noncomputable section

namespace DSP.TPTSVF

open Polynomial

variable {K : Type} [Field K]

/-- The coefficients solve the zero-delay loop for integrator gain g. -/
structure Tuned (f : TPTSVF K) (g : K) : Prop where
  solve : f.a1 * (1 + g * (g + f.k)) = 1
  a2 : f.a2 = g * f.a1
  a3 : f.a3 = g * f.a2

/-- Both integrators are empty; stored responses are overwritten by the first sample. -/
structure AtRest (f : TPTSVF K) : Prop where
  ic1 : f.ic1 = 0
  ic2 : f.ic2 = 0

/-- The analog prototype's denominator s² + k g s + g². -/
def denominator (g k : K) : K[X] := X ^ 2 + C k * C g * X + C g * C g

def lowPassNumerator (g : K) : K[X] := C g * C g

def bandPassNumerator (g : K) : K[X] := C g * X

def highPassNumerator : K[X] := X ^ 2

def notchNumerator (g : K) : K[X] := X ^ 2 + C g * C g

theorem bil_denominator (g k : K) :
    Bilinear.bil (denominator g k) =
      Bilinear.sigma ^ 2 + Bilinear.const k * Bilinear.const g * Bilinear.sigma +
        Bilinear.const g * Bilinear.const g := by
  simp only [denominator, map_add, map_mul, map_pow, Bilinear.bil_X, Bilinear.bil_C]

theorem denominator_ne_zero (f : TPTSVF K) (g : K) (ht : f.Tuned g) :
    Bilinear.bil (denominator g f.k) ≠ 0 := by
  apply Bilinear.bil_ne_zero
  intro h
  simp only [denominator, eval_add, eval_mul, eval_pow, eval_X, eval_C] at h
  have := ht.solve
  have hzero : (0 : K) = 1 := by linear_combination this - f.a1 * h
  exact zero_ne_one hzero

section Streams

variable (f : TPTSVF K) (u : ℕ → K)

abbrev trajectory : ℕ → TPTSVF K := Step.states step (fun k => ⟨u k⟩) f

theorem states_coefficients (n : ℕ) :
    (trajectory f u n).k = f.k ∧ (trajectory f u n).a1 = f.a1 ∧ (trajectory f u n).a2 = f.a2 ∧ (trajectory f u n).a3 = f.a3 :=
  ⟨Step.states_invariant step (·.k) (fun _ _ => rfl) _ f n,
    Step.states_invariant step (·.a1) (fun _ _ => rfl) _ f n,
    Step.states_invariant step (·.a2) (fun _ _ => rfl) _ f n,
    Step.states_invariant step (·.a3) (fun _ _ => rfl) _ f n⟩

/-- The three stream equations of the analog SVF, read off the stored
responses: two trapezoidal integrators and the summing junction. -/
theorem streams (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.sigma * Bilinear.ser (Bilinear.response step u f (·.bandPass)) =
        Bilinear.const g * Bilinear.ser (Bilinear.response step u f (·.highPass)) ∧
      Bilinear.sigma * Bilinear.ser (Bilinear.response step u f (·.lowPass)) =
        Bilinear.const g * Bilinear.ser (Bilinear.response step u f (·.bandPass)) ∧
      Bilinear.ser (Bilinear.response step u f (·.highPass)) =
        Bilinear.ser u - Bilinear.const f.k * Bilinear.ser (Bilinear.response step u f (·.bandPass)) -
          Bilinear.ser (Bilinear.response step u f (·.lowPass)) := by
  have hc := states_coefficients f u
  refine ⟨?_, ?_, ?_⟩
  · apply Bilinear.integrator (fun n => (trajectory f u n).ic1) _ _ g hr.ic1
    · intro n
      simp only [fma_eq, Bilinear.response, Step.states_succ, step]
      ring
    · intro n
      obtain ⟨hk, h1, h2, h3⟩ := hc n
      simp only [fma_eq, Bilinear.response, Step.states_succ, step, hk, h1, h2, h3]
      rw [ht.a3, ht.a2]
      linear_combination ((trajectory f u n).ic1 + g * (u n - (trajectory f u n).ic2)) * ht.solve
  · apply Bilinear.integrator (fun n => (trajectory f u n).ic2) _ _ g hr.ic2
    · intro n
      simp only [fma_eq, Bilinear.response, Step.states_succ, step]
      ring
    · intro n
      obtain ⟨hk, h1, h2, h3⟩ := hc n
      simp only [fma_eq, Bilinear.response, Step.states_succ, step, hk, h1, h2, h3]
      rw [ht.a3, ht.a2]
      ring
  · rw [← Bilinear.ser_mul, ← Bilinear.ser_sub, ← Bilinear.ser_sub]
    apply Bilinear.ser_congr
    intro n
    obtain ⟨hk, -⟩ := hc n
    simp only [fma_eq, Bilinear.response, Step.states_succ, step, hk]
    ring

end Streams

theorem realizes_lowPass (f : TPTSVF K) (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.Realizes step f (·.lowPass)
      (Bilinear.analog (lowPassNumerator g) (denominator g f.k)) := by
  intro u
  obtain ⟨e1, e2, e3⟩ := streams f u g ht hr
  apply Bilinear.of_cleared (denominator_ne_zero f g ht)
  simp only [bil_denominator, lowPassNumerator, map_mul, Bilinear.bil_C]
  linear_combination (Bilinear.sigma + Bilinear.const f.k * Bilinear.const g) * e2 +
    Bilinear.const g * e1 + Bilinear.const g * Bilinear.const g * e3

theorem realizes_bandPass (f : TPTSVF K) (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.Realizes step f (·.bandPass)
      (Bilinear.analog (bandPassNumerator g) (denominator g f.k)) := by
  intro u
  obtain ⟨e1, e2, e3⟩ := streams f u g ht hr
  apply Bilinear.of_cleared (denominator_ne_zero f g ht)
  simp only [bil_denominator, bandPassNumerator, map_mul, Bilinear.bil_C, Bilinear.bil_X]
  linear_combination Bilinear.sigma * e1 - Bilinear.const g * e2 +
    Bilinear.const g * Bilinear.sigma * e3

theorem realizes_highPass (f : TPTSVF K) (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.Realizes step f (·.highPass)
      (Bilinear.analog highPassNumerator (denominator g f.k)) := by
  intro u
  obtain ⟨e1, e2, e3⟩ := streams f u g ht hr
  apply Bilinear.of_cleared (denominator_ne_zero f g ht)
  simp only [bil_denominator, highPassNumerator, map_pow, Bilinear.bil_X]
  linear_combination Bilinear.sigma ^ 2 * e3 -
    (Bilinear.const f.k * Bilinear.sigma + Bilinear.const g) * e1 - Bilinear.sigma * e2

/-- The notch is stored as lowpass plus highpass, so its transfer is the sum. -/
theorem realizes_notch (f : TPTSVF K) (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.Realizes step f (·.notch)
      (Bilinear.analog (notchNumerator g) (denominator g f.k)) := by
  refine (((realizes_lowPass f g ht hr).add (realizes_highPass f g ht hr)).congr (fun _ _ => rfl)).transfer_eq ?_
  rw [Bilinear.analog_add, lowPassNumerator, highPassNumerator, notchNumerator, add_comm]

/-- An all-pass tap shares both integrators with the other SVF responses. -/
theorem realizes_allPass (f : TPTSVF K) (g : K) (ht : f.Tuned g) (hr : f.AtRest) :
    Bilinear.Realizes step f (·.allPass)
      (Bilinear.analog (denominator g (-f.k)) (denominator g f.k)) := by
  have scaled := Bilinear.Realizes.mul (p := step) (fun s : TPTSVF K => s.k * 2)
    (fun _ _ => rfl) (realizes_bandPass f g ht hr)
  refine (scaled.complement (d := fun s => s.allPass) ?_).transfer_eq ?_
  · intro s x
    simp only [step, fma_eq]
    ring
  · rw [Bilinear.const_mul_analog, Bilinear.one_sub_analog _ _ (denominator_ne_zero f g ht)]
    congr 1
    simp only [denominator, bandPassNumerator, map_mul, map_ofNat, map_neg]
    ring

end DSP.TPTSVF

end
