import DSP.Filters.TPTLadder
import DSP.Filters.OnePoleTPTLowPass.Proofs

noncomputable section

namespace DSP.TPTLadder

open Polynomial

variable {K : Type} [Field K]

def gain (f : TPTLadder K) : K := f.stage1.G * f.stage2.G * f.stage3.G * f.stage4.G

/-- Each stage solves its own loop for gain gᵢ and starts empty; the global
loop is well posed: 1 + k G₁ G₂ G₃ G₄ ≠ 0. -/
structure Ready (f : TPTLadder K) (g₁ g₂ g₃ g₄ : K) : Prop where
  tuned₁ : f.stage1.G * (1 + g₁) = g₁
  tuned₂ : f.stage2.G * (1 + g₂) = g₂
  tuned₃ : f.stage3.G * (1 + g₃) = g₃
  tuned₄ : f.stage4.G * (1 + g₄) = g₄
  rest₁ : f.stage1.s = 0
  rest₂ : f.stage2.s = 0
  rest₃ : f.stage3.s = 0
  rest₄ : f.stage4.s = 0
  loop : 1 + f.k * f.gain ≠ 0

/-- The analog ladder g₁ g₂ g₃ g₄ / (∏ (s + gᵢ) + k g₁ g₂ g₃ g₄). -/
def numerator (g₁ g₂ g₃ g₄ : K) : K[X] := C g₁ * C g₂ * C g₃ * C g₄

def denominator (k g₁ g₂ g₃ g₄ : K) : K[X] :=
  (X + C g₁) * (X + C g₂) * (X + C g₃) * (X + C g₄) + C k * (C g₁ * C g₂ * C g₃ * C g₄)

@[simp] theorem step_k (f : TPTLadder K) (input : Sample K) : (step f input).k = f.k := rfl

@[simp] theorem step_gain (f : TPTLadder K) (input : Sample K) :
    (step f input).gain = f.gain := rfl

/-- The closed-form drive satisfies the zero-delay loop e = x - k y₄ exactly. -/
theorem loop_solved (f : TPTLadder K) (input : Sample K) (h : 1 + f.k * f.gain ≠ 0) :
    (step f input).drive = input.x - f.k * (step f input).stage4.y := by
  simp only [gain] at h
  simp only [step, OnePoleTPTLowPass.step_y]
  set d := (input.x - f.k * f.stage4.respond (f.stage3.respond (f.stage2.respond
    (f.stage1.respond 0)))) / (1 + f.k * (f.stage1.G * f.stage2.G * f.stage3.G * f.stage4.G))
  have hd : d * (1 + f.k * (f.stage1.G * f.stage2.G * f.stage3.G * f.stage4.G)) =
      input.x - f.k * f.stage4.respond (f.stage3.respond (f.stage2.respond (f.stage1.respond 0))) :=
    div_mul_cancel₀ _ h
  simp only [OnePoleTPTLowPass.respond_eq] at hd ⊢
  linear_combination hd

theorem denominator_ne_zero (f : TPTLadder K) (g₁ g₂ g₃ g₄ : K) (h : f.Ready g₁ g₂ g₃ g₄) :
    Bilinear.bil (denominator f.k g₁ g₂ g₃ g₄) ≠ 0 := by
  apply Bilinear.bil_ne_zero
  have hp : (1 + g₁) * (1 + g₂) * (1 + g₃) * (1 + g₄) ≠ 0 := by
    have := OnePoleTPTLowPass.one_add_ne_zero _ _ h.tuned₁
    have := OnePoleTPTLowPass.one_add_ne_zero _ _ h.tuned₂
    have := OnePoleTPTLowPass.one_add_ne_zero _ _ h.tuned₃
    have := OnePoleTPTLowPass.one_add_ne_zero _ _ h.tuned₄
    positivity
  have he : (denominator f.k g₁ g₂ g₃ g₄).eval 1 =
      (1 + g₁) * (1 + g₂) * (1 + g₃) * (1 + g₄) * (1 + f.k * f.gain) := by
    simp only [denominator, gain, eval_add, eval_mul, eval_X, eval_C]
    linear_combination (-f.k) * (g₂ * g₃ * g₄ * h.tuned₁ +
      f.stage1.G * (1 + g₁) * g₃ * g₄ * h.tuned₂ +
      f.stage1.G * (1 + g₁) * f.stage2.G * (1 + g₂) * g₄ * h.tuned₃ +
      f.stage1.G * (1 + g₁) * f.stage2.G * (1 + g₂) * f.stage3.G * (1 + g₃) * h.tuned₄)
  rw [he]
  exact mul_ne_zero hp h.loop

/-- From rest, the output realizes the bilinear image of the analog ladder.
The stages compose in series through their stored outputs, and the solved
loop closes the global feedback. -/
theorem realizes (f : TPTLadder K) (g₁ g₂ g₃ g₄ : K) (h : f.Ready g₁ g₂ g₃ g₄) :
    Bilinear.Realizes step f (·.stage4.y)
      (Bilinear.analog (numerator g₁ g₂ g₃ g₄) (denominator f.k g₁ g₂ g₃ g₄)) := by
  intro u
  have y₁ := Bilinear.driven step OnePoleTPTLowPass.step (·.stage1) (·.drive) (fun _ _ => rfl)
    (OnePoleTPTLowPass.realizes f.stage1 g₁ h.tuned₁ h.rest₁) u
  have y₂ := Bilinear.driven step OnePoleTPTLowPass.step (·.stage2) (·.stage1.y) (fun _ _ => rfl)
    (OnePoleTPTLowPass.realizes f.stage2 g₂ h.tuned₂ h.rest₂) u
  have y₃ := Bilinear.driven step OnePoleTPTLowPass.step (·.stage3) (·.stage2.y) (fun _ _ => rfl)
    (OnePoleTPTLowPass.realizes f.stage3 g₃ h.tuned₃ h.rest₃) u
  have y₄ := Bilinear.driven step OnePoleTPTLowPass.step (·.stage4) (·.stage3.y) (fun _ _ => rfl)
    (OnePoleTPTLowPass.realizes f.stage4 g₄ h.tuned₄ h.rest₄) u
  replace y₁ := Bilinear.cleared (OnePoleTPTLowPass.denominator_ne_zero _ _ h.tuned₁) y₁
  replace y₂ := Bilinear.cleared (OnePoleTPTLowPass.denominator_ne_zero _ _ h.tuned₂) y₂
  replace y₃ := Bilinear.cleared (OnePoleTPTLowPass.denominator_ne_zero _ _ h.tuned₃) y₃
  replace y₄ := Bilinear.cleared (OnePoleTPTLowPass.denominator_ne_zero _ _ h.tuned₄) y₄
  simp only [Bilinear.bil_onePole, Bilinear.bil_C] at y₁ y₂ y₃ y₄
  have loop : Bilinear.ser (Bilinear.response step u f (·.drive)) =
      Bilinear.ser u - Bilinear.const f.k * Bilinear.ser (Bilinear.response step u f (·.stage4.y)) := by
    rw [← Bilinear.ser_mul, ← Bilinear.ser_sub]
    apply Bilinear.ser_congr
    intro n
    have hk := Step.states_invariant step (·.k) (fun _ _ => rfl) (fun k => ⟨u k⟩) f n
    have hg := Step.states_invariant step gain (fun _ _ => rfl) (fun k => ⟨u k⟩) f n
    simp only [Bilinear.response, Step.states_succ]
    rw [loop_solved _ _ (by rw [hk, hg]; exact h.loop), hk]
  apply Bilinear.of_cleared (denominator_ne_zero f g₁ g₂ g₃ g₄ h)
  simp only [denominator, numerator, map_add, map_mul, Bilinear.bil_X, Bilinear.bil_C]
  linear_combination
    (Bilinear.sigma + Bilinear.const g₁) * (Bilinear.sigma + Bilinear.const g₂) *
      (Bilinear.sigma + Bilinear.const g₃) * y₄ +
    Bilinear.const g₄ * (Bilinear.sigma + Bilinear.const g₁) *
      (Bilinear.sigma + Bilinear.const g₂) * y₃ +
    Bilinear.const g₄ * Bilinear.const g₃ * (Bilinear.sigma + Bilinear.const g₁) * y₂ +
    Bilinear.const g₄ * Bilinear.const g₃ * Bilinear.const g₂ * y₁ +
    Bilinear.const g₁ * Bilinear.const g₂ * Bilinear.const g₃ * Bilinear.const g₄ * loop

/-- Equal stages give the classic idealized ladder g⁴ / ((s + g)⁴ + k g⁴). -/
theorem realizes_equal (f : TPTLadder K) (g : K) (h : f.Ready g g g g) :
    Bilinear.Realizes step f (·.stage4.y)
      (Bilinear.analog (C g ^ 4) ((X + C g) ^ 4 + C f.k * C g ^ 4)) := by
  refine (realizes f g g g g h).transfer_eq ?_
  simp only [numerator, denominator]
  ring_nf

end DSP.TPTLadder

end
