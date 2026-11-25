import DSP.Filters.SquiggleBoyQuadReverb.Frozen.ModeParameters

namespace DSP.SquiggleBoyQuadReverb.Frozen

open scoped BigOperators

theorem tapRead_tapeMode {n : ℕ} (p : Tap n ℂ) (q : ℂ) (x : Lanes ℂ) :
    tapRead p (tapeMode n q x) = tapResponse p q * x := by
  funext i
  rw [tapRead_apply]
  simp only [tapeMode, Pi.smul_apply, smul_eq_mul, Pi.mul_apply, tapResponse,
    Finset.sum_mul, mul_assoc]

theorem push_tapeMode (n : ℕ) (q : ℂ) (hq : q ≠ 0) (x : Lanes ℂ) :
    push (tapeMode n q x, x) = q⁻¹ • tapeMode n q x := by
  rw [push_eq_shift]
  cases n with
  | zero => exact funext fun j => j.elim0
  | succ n =>
    funext j
    refine Fin.cases ?_ (fun k => ?_) j
    · change x = q⁻¹ • (q ^ 1 • x)
      rw [pow_one, smul_smul, inv_mul_cancel₀ hq, one_smul]
    · change q ^ (k.val + 1) • x = q⁻¹ • (q ^ (k.val + 1 + 1) • x)
      rw [smul_smul]
      have hpow : q⁻¹ * q ^ (k.val + 1 + 1) = q ^ (k.val + 1) := by
        rw [pow_succ' q (k.val + 1), ← mul_assoc, inv_mul_cancel₀ hq, one_mul]
      rw [hpow]

theorem allPass_apfMode {n : ℕ} (p : Tap n ℂ) (q : ℂ) (hq : q ≠ 0) (x : Lanes ℂ)
    (hd : ∀ i, 1 - allPassGain * tapResponse p q i ≠ 0) :
    allPass p allPassGain (apfMode p q x, x) =
      (q⁻¹ • apfMode p q x, allPassResponse p q * x) := by
  rw [allPass_apply]
  have hy : tapRead p (apfMode p q x) - (allPassGain : ℂ) • x =
      allPassResponse p q * x := by
    rw [apfMode, tapRead_tapeMode]
    funext i
    simp only [Pi.sub_apply, Pi.add_apply, Pi.mul_apply, Pi.smul_apply, smul_eq_mul,
      allPassResponse, SchroederAllPass.response, pow_one]
    have h (c g u : ℂ) (hne : 1 - g * c ≠ 0) :
        c * (u + g * ((c - g) / (1 - g * c) * u)) - g * u =
          (c - g) / (1 - g * c) * u := by
      let y := (c - g) / (1 - g * c) * u
      change c * (u + g * y) - g * u = y
      have hy : y * (1 - g * c) = (c - g) * u := by
        dsimp only [y]
        rw [mul_right_comm, div_mul_cancel₀ _ hne]
      linear_combination -hy
    exact h (tapResponse p q i) allPassGain (x i) (hd i)
  dsimp only
  rw [hy, apfMode, push_tapeMode n q hq]

theorem intro_introMode (q : ℂ) (hq : q ≠ 0) (order : List (Fin 8))
    (taps : IntroTaps order ℂ) (x : Lanes ℂ)
    (hd : ∀ j i, 1 - allPassGain * tapResponse (taps j) q i ≠ 0) :
    intro allPassGain order taps (introMode q order taps x, x) =
      (q⁻¹ • introMode q order taps x, introResponse q order taps * x) := by
  induction order generalizing x with
  | nil =>
    apply Prod.ext
    · exact funext fun j => j.elim0
    · change x = 1 * x
      rw [one_mul]
  | cons i order ih =>
    rw [intro_cons]
    change
      (let head := allPass (taps 0) allPassGain (apfMode (taps 0) q x, x)
       let tail := intro allPassGain order (fun j => taps j.succ)
         (introMode q order (fun j => taps j.succ) (allPassResponse (taps 0) q * x), head.2)
       (introCons (F := fun j => Tape (squiggleBoyReverbIntroAPFCapacity j) ℂ)
         (i := i) (order := order) head.1 tail.1, tail.2)) = _
    rw [allPass_apfMode (taps 0) q hq x (hd 0)]
    dsimp only
    rw [ih (fun j => taps j.succ) _ (fun j => hd j.succ)]
    apply Prod.ext
    · funext j
      refine Fin.cases ?_ (fun k => ?_) j <;> rfl
    · change introResponse q order (fun j => taps j.succ) * (allPassResponse (taps 0) q * x) =
        (allPassResponse (taps 0) q * introResponse q order (fun j => taps j.succ)) * x
      rw [← mul_assoc, mul_comm (introResponse q order (fun j => taps j.succ))]

end DSP.SquiggleBoyQuadReverb.Frozen
