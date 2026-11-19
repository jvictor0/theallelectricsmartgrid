import DSP.Filters.TunedDecayComb
import DSP.DelayBuffer.Transfer
import DSP.DelayBuffer.Energy
import DSP.Filters.NormalizedAllPass.Proofs

namespace DSP.TunedDecayComb

variable {capacity : ℕ} {K : Type}

section History
variable [CommRing K]

theorem step_buffer (c₀ c₁ : Coefficients capacity K) (mix : Option K)
    (s : State capacity K) (x : Sample K) :
    (step c₀ c₁ mix s x).buffer = s.buffer.push (step c₀ c₁ mix s x).y := by
  cases mix <;> rfl

/-- Even during retuning, every tap reads the same history of mixed outputs.
Neither reader owns a private write stream. -/
theorem read_trajectory (c₀ c₁ : ℕ → Coefficients capacity K) (mix : ℕ → Option K)
    (s : ℕ → State capacity K) (u : ℕ → K)
    (h0 : (s 0).buffer.history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = step (c₀ n) (c₁ n) (mix n) (s n) ⟨u n⟩)
    (n : ℕ) (age : Fin capacity) :
    (s n).buffer.read age = IIR.delay (age.val + 1) (fun t => (s (t + 1)).y) n := by
  apply DelayBuffer.read_trajectory (fun n => (s n).buffer) _ h0
  intro t
  rw [hs t]
  exact step_buffer _ _ _ _ _

end History

section Transfer
variable [Field K]

def lowpassDenominator (c : Coefficients capacity K) (q : K) : K :=
  (1 + c.k * c.g + c.g ^ 2) + 2 * (c.g ^ 2 - 1) * (c.r * q) +
    (1 - c.k * c.g + c.g ^ 2) * (c.r * q) ^ 2

def lowpassResponse (c : Coefficients capacity K) (q : K) : K :=
  if c.bypass then 1
  else c.gain * c.g ^ 2 * (1 + c.r * q) ^ 2 / lowpassDenominator c q

def returnResponse (c : Coefficients capacity K) (q : K) : K :=
  lowpassResponse c q * c.sign * NormalizedAllPass.response c.a c.r q *
    c.delayGain * q ^ (c.age.val + 1)

def settledResponse (c : Coefficients capacity K) (q : K) : K :=
  lowpassResponse c q / (1 - returnResponse c q)

def mixedResponse (c₀ c₁ : Coefficients capacity K) (m q : K) : K :=
  ((1 - m) * lowpassResponse c₀ q + m * lowpassResponse c₁ q) /
    (1 - ((1 - m) * returnResponse c₀ q + m * returnResponse c₁ q))

/-- Eliminate both radial TPT memories without dividing by 1±r*q. This covers
DC and Nyquist and exposes the full second-order denominator. -/
theorem lowpass_mode_equation (g k h r q i₁ i₂ e v₁ v₂ : K)
    (hc : h * (1 + k * g + g ^ 2) = 1)
    (hv₁ : v₁ = h * (r * i₁ + g * (e - r * i₂)))
    (hv₂ : v₂ = r * i₂ + g * v₁)
    (hi₁ : i₁ = q * (v₁ * 2 - r * i₁))
    (hi₂ : i₂ = q * (v₂ * 2 - r * i₂)) :
    ((1 + k * g + g ^ 2) + 2 * (g ^ 2 - 1) * (r * q) +
      (1 - k * g + g ^ 2) * (r * q) ^ 2) * v₂ = g ^ 2 * (1 + r * q) ^ 2 * e := by
  have ha : (1 + k * g + g ^ 2) * v₁ = r * i₁ + g * (e - r * i₂) := by
    linear_combination (1 + k * g + g ^ 2) * hv₁ + (r * i₁ + g * (e - r * i₂)) * hc
  have hb : (1 + g * k) * v₁ + g * v₂ = r * i₁ + g * e := by
    linear_combination ha + g * hv₂
  have h₁ : (1 - r * q + g * k * (1 + r * q)) * v₁ +
      g * (1 + r * q) * v₂ = g * (1 + r * q) * e := by
    linear_combination (1 + r * q) * hb + r * hi₁
  have h₂ : (1 - r * q) * v₂ = g * (1 + r * q) * v₁ := by
    linear_combination (1 + r * q) * hv₂ + r * hi₂
  linear_combination g * (1 + r * q) * h₁ +
    (1 - r * q + g * k * (1 + r * q)) * h₂

/-- The mode relation for the actual branch update, over ℂ as well as ℝ.
Normalization and prepared h are explicit contracts on supplied coefficients. -/
theorem branch_mode (c : Coefficients capacity K) (s : Branch K) (x d q : K)
    (hn : c.a ^ 2 + c.b ^ 2 = 1) (hc : c.h * (1 + c.k * c.g + c.g ^ 2) = 1)
    (ha : 1 + c.a * c.r * q ≠ 0)
    (hl : c.bypass = false → lowpassDenominator c q ≠ 0)
    (hs : s.allpass = q * (branchStep c s x d).allpass)
    (hi₁ : s.ic1 = q * (branchStep c s x d).ic1)
    (hi₂ : s.ic2 = q * (branchStep c s x d).ic2) :
    (branchStep c s x d).y = lowpassResponse c q *
      (x + c.sign * NormalizedAllPass.response c.a c.r q * c.delayGain * d) := by
  let ap := NormalizedAllPass.step (⟨c.a, c.b, c.r, s.allpass, 0⟩ : NormalizedAllPass K)
    ⟨d * c.delayGain⟩
  have hs' : s.allpass = q * ap.s := by
    cases hb : c.bypass <;> simpa [branchStep, ap, hb] using hs
  have hap := NormalizedAllPass.mode_equation
    (⟨c.a, c.b, c.r, s.allpass, 0⟩ : NormalizedAllPass K) ⟨d * c.delayGain⟩ q hn hs'
  have hap' : ap.y = NormalizedAllPass.response c.a c.r q * c.delayGain * d := by
    unfold NormalizedAllPass.response
    rw [div_mul_eq_mul_div, div_mul_eq_mul_div, eq_div_iff ha]
    dsimp only at hap
    linear_combination hap
  let e := x + c.sign * ap.y
  let v₁ := c.h * (c.r * s.ic1 + c.g * (e - c.r * s.ic2))
  let v₂ := c.r * s.ic2 + c.g * v₁
  have he : e = x + c.sign * NormalizedAllPass.response c.a c.r q * c.delayGain * d := by
    dsimp [e]
    rw [hap']
    ring
  cases hb : c.bypass with
  | true => simpa [branchStep, hb, lowpassResponse, ap, e] using he
  | false =>
    have hi₁' : s.ic1 = q * (v₁ * 2 - c.r * s.ic1) := by
      simpa [branchStep, hb, v₁, e, ap] using hi₁
    have hi₂' : s.ic2 = q * (v₂ * 2 - c.r * s.ic2) := by
      simpa [branchStep, hb, v₂, v₁, e, ap] using hi₂
    have h := lowpass_mode_equation c.g c.k c.h c.r q s.ic1 s.ic2 e v₁ v₂
      hc rfl rfl hi₁' hi₂'
    simp only [branchStep, hb, Bool.false_eq_true, if_false]
    change c.gain * v₂ = lowpassResponse c q * _
    rw [← he]
    simp only [lowpassResponse, hb, Bool.false_eq_true, if_false]
    rw [div_mul_eq_mul_div, eq_div_iff (hl hb)]
    unfold lowpassDenominator
    linear_combination c.gain * h

/-- Fixed tap transfer uses the same generic read theorem as Schroeder. -/
theorem read_series (c₀ c₁ : ℕ → Coefficients capacity K) (mix : ℕ → Option K)
    (s : ℕ → State capacity K) (u : ℕ → K)
    (h0 : (s 0).buffer.history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = step (c₀ n) (c₁ n) (mix n) (s n) ⟨u n⟩)
    (age : Fin capacity) :
    Bilinear.ser (fun n => (s n).buffer.read age) =
      Bilinear.delay ^ (age.val + 1) * Bilinear.ser (fun n => (s (n + 1)).y) := by
  apply DelayBuffer.read_series _ _ h0
  intro n
  rw [hs n]
  exact step_buffer _ _ _ _ _

/-- A fixed branch's three memories all advance by the same geometric factor. -/
def BranchMode (c : Coefficients capacity K) (s : Branch K) (x d q : K) : Prop :=
  s.allpass = q * (branchStep c s x d).allpass ∧
  s.ic1 = q * (branchStep c s x d).ic1 ∧ s.ic2 = q * (branchStep c s x d).ic2

def Prepared (c : Coefficients capacity K) : Prop :=
  c.a ^ 2 + c.b ^ 2 = 1 ∧ c.h * (1 + c.k * c.g + c.g ^ 2) = 1

def Regular (c : Coefficients capacity K) (q : K) : Prop :=
  1 + c.a * c.r * q ≠ 0 ∧ (c.bypass = false → lowpassDenominator c q ≠ 0)

/-- Settled response of the executable comb core, including the full radial
allpass and lowpass. The buffer history is the circulating output mode. -/
theorem settled_mode (c₀ c₁ : Coefficients capacity K) (s : State capacity K) (x q : K)
    (hc : Prepared c₀) (hr : Regular c₀ q)
    (hm : BranchMode c₀ s.first x (s.buffer.read c₀.age) q)
    (hh : ∀ i, s.buffer.read i = q ^ (i.val + 1) * (step c₀ c₁ none s ⟨x⟩).y)
    (hd : 1 - returnResponse c₀ q ≠ 0) :
    (step c₀ c₁ none s ⟨x⟩).y = settledResponse c₀ q * x := by
  have h := branch_mode c₀ s.first x (s.buffer.read c₀.age) q
    hc.1 hc.2 hr.1 hr.2 hm.1 hm.2.1 hm.2.2
  change (step c₀ c₁ none s ⟨x⟩).y = _ at h
  unfold settledResponse
  rw [div_mul_eq_mul_div, eq_div_iff hd]
  unfold returnResponse
  linear_combination h + (lowpassResponse c₀ q * c₀.sign *
    NormalizedAllPass.response c₀.a c₀.r q * c₀.delayGain) * hh c₀.age

/-- The two-reader denominator has one mixed feedback loop. Treating the
branches as two independent combs would give a different transfer function. -/
theorem mixed_mode (c₀ c₁ : Coefficients capacity K) (s : State capacity K) (m x q : K)
    (hc₀ : Prepared c₀) (hc₁ : Prepared c₁) (hr₀ : Regular c₀ q) (hr₁ : Regular c₁ q)
    (hm₀ : BranchMode c₀ s.first x (s.buffer.read c₀.age) q)
    (hm₁ : BranchMode c₁ s.second x (s.buffer.read c₁.age) q)
    (hh : ∀ i, s.buffer.read i = q ^ (i.val + 1) * (step c₀ c₁ (some m) s ⟨x⟩).y)
    (hd : 1 - ((1 - m) * returnResponse c₀ q + m * returnResponse c₁ q) ≠ 0) :
    (step c₀ c₁ (some m) s ⟨x⟩).y = mixedResponse c₀ c₁ m q * x := by
  have h₀ := branch_mode c₀ s.first x (s.buffer.read c₀.age) q
    hc₀.1 hc₀.2 hr₀.1 hr₀.2 hm₀.1 hm₀.2.1 hm₀.2.2
  have h₁ := branch_mode c₁ s.second x (s.buffer.read c₁.age) q
    hc₁.1 hc₁.2 hr₁.1 hr₁.2 hm₁.1 hm₁.2.1 hm₁.2.2
  have hy : (step c₀ c₁ (some m) s ⟨x⟩).y =
      (1 - m) * (branchStep c₀ s.first x (s.buffer.read c₀.age)).y +
      m * (branchStep c₁ s.second x (s.buffer.read c₁.age)).y := by
    simp only [step]
    ring
  unfold mixedResponse
  rw [div_mul_eq_mul_div, eq_div_iff hd]
  unfold returnResponse
  linear_combination hy + (1 - m) * h₀ + m * h₁ +
    ((1 - m) * lowpassResponse c₀ q * c₀.sign *
      NormalizedAllPass.response c₀.a c₀.r q * c₀.delayGain) * hh c₀.age +
    (m * lowpassResponse c₁ q * c₁.sign *
      NormalizedAllPass.response c₁.a c₁.r q * c₁.delayGain) * hh c₁.age

/-- The tuner places one factor r on every unit of delay. -/
theorem radial_return (c : Coefficients capacity K) (q : K)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) :
    returnResponse c q = lowpassResponse c q * c.sign *
      NormalizedAllPass.response c.a c.r q * (c.r * q) ^ (c.age.val + 1) := by
  rw [returnResponse, hr, mul_pow]
  ring

end Transfer

/-- The delayed return path's weighted storage balance, shared by either comb
branch. This covers the delay plus lattice, not the entire feedback loop. -/
theorem return_energy (c : Coefficients capacity ℝ) (buffer : DelayBuffer capacity ℝ)
    (memory written : ℝ) (hn : c.a ^ 2 + c.b ^ 2 = 1)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) :
    let ap := NormalizedAllPass.step
      (⟨c.a, c.b, c.r, memory, 0⟩ : NormalizedAllPass ℝ)
      ⟨buffer.read c.age * c.delayGain⟩
    ap.y ^ 2 + ap.s ^ 2 + DelayBuffer.energy (buffer.push written) (c.age.val + 1) c.r =
      written ^ 2 + c.r ^ 2 *
        (memory ^ 2 + DelayBuffer.energy buffer (c.age.val + 1) c.r) := by
  have ha := NormalizedAllPass.step_energy
    (⟨c.a, c.b, c.r, memory, 0⟩ : NormalizedAllPass ℝ)
    ⟨buffer.read c.age * c.delayGain⟩ hn
  have hb := DelayBuffer.weighted_energy buffer written c.r c.age
  dsimp only at ha ⊢
  rw [hr] at ha ⊢
  linear_combination ha + hb

/-- Apply the return-path theorem to an actual shared-write comb sample.
The written energy is the final mixed output, even for the first reader. -/
theorem step_return_energy (c₀ c₁ : Coefficients capacity ℝ) (mix : Option ℝ)
    (s : State capacity ℝ) (x : Sample ℝ) (hn : c₀.a ^ 2 + c₀.b ^ 2 = 1)
    (hr : c₀.delayGain = c₀.r ^ (c₀.age.val + 1)) :
    let next := step c₀ c₁ mix s x
    let returned := c₀.a * (s.buffer.read c₀.age * c₀.delayGain) +
      c₀.b * (c₀.r * s.first.allpass)
    returned ^ 2 + next.first.allpass ^ 2 +
        DelayBuffer.energy next.buffer (c₀.age.val + 1) c₀.r =
      next.y ^ 2 + c₀.r ^ 2 * (s.first.allpass ^ 2 +
        DelayBuffer.energy s.buffer (c₀.age.val + 1) c₀.r) := by
  have h := return_energy c₀ s.buffer s.first.allpass (step c₀ c₁ mix s x).y hn hr
  dsimp only
  rw [step_buffer]
  cases mix <;> cases hb : c₀.bypass <;>
    simpa [step, branchStep, hb, NormalizedAllPass.step] using h

end DSP.TunedDecayComb
