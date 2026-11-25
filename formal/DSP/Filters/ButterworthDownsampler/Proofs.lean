import DSP.Filters.ButterworthDownsampler
import DSP.IIR.Proofs
import DSP.Processor.Simulation
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring

namespace DSP.ButterworthDownsampler

variable {R : Type} [CommRing R]

abbrev History (R : Type) := IIR.State 2 2 R

def Coefficients.toIIR (c : Coefficients R) : IIR.Coefficients 2 2 R :=
  ⟨c.gain, fun i => if i = 0 then c.gain + c.gain else c.gain,
    fun i => if i = 0 then -c.feedback1 else -c.feedback2⟩

def reference2 (c : Coefficients R) : Processor (History R) (Sample R) (Sample R) :=
  fun state input =>
    let (next, output) := IIR.processor c.toIIR state input.x
    (next, ⟨output⟩)

/-- Each transposed delay sums the contributions still pending from the
direct form I input and output histories. -/
def save (c : Coefficients R) (s : History R) : Section R where
  coefficients := c
  delay1 := (c.gain + c.gain) * s.inputs 0 + c.gain * s.inputs 1 +
    c.feedback1 * s.outputs 0 + c.feedback2 * s.outputs 1
  delay2 := c.gain * s.inputs 0 + c.feedback2 * s.outputs 0

theorem simulation2 (c : Coefficients R) :
    Processor.Simulation (reference2 c) process2 (fun s t => t = save c s) := by
  intro s t ht x
  subst t
  have shift_zero (y : R) (h : Fin 2 → R) : IIR.shift y h 0 = y := rfl
  have shift_one (y : R) (h : Fin 2 → R) : IIR.shift y h 1 = h 0 := rfl
  constructor
  · simp [process2, reference2, save, Coefficients.toIIR, IIR.processor, shift_zero, shift_one,
      StateSpace.dot_eq_dotProduct, dotProduct, Fin.sum_univ_two, fma_eq]
    constructor <;> ring
  · simp [process2, reference2, save, Coefficients.toIIR, IIR.processor,
      StateSpace.dot_eq_dotProduct, dotProduct, Fin.sum_univ_two, fma_eq]
    ring

omit [CommRing R] in
theorem Cascade.simulation {S T : Type}
    {p₁ p₂ : Processor S (Sample R) (Sample R)} {q₁ q₂ : Processor T (Sample R) (Sample R)}
    {save₁ save₂ : S → T} (h₁ : Processor.Simulation p₁ q₁ (fun s t => t = save₁ s))
    (h₂ : Processor.Simulation p₂ q₂ (fun s t => t = save₂ s)) :
    Processor.Simulation (Cascade.process p₁ p₂) (Cascade.process q₁ q₂)
      (fun s t => t = ⟨save₁ s.first, save₂ s.second⟩) := by
  intro s t ht x
  subst t
  obtain ⟨hfirst, hmiddle⟩ := h₁ s.first (save₁ s.first) rfl x
  obtain ⟨hsecond, houtput⟩ := h₂ s.second (save₂ s.second) rfl (p₁ s.first x).2
  simp only [Cascade.process, ← hmiddle, hfirst, hsecond, houtput, and_self]

def reference4 (c : Cascade (Coefficients R)) :
    Processor (Cascade (History R)) (Sample R) (Sample R) :=
  Cascade.process (reference2 c.first) (reference2 c.second)

def save4 (c : Cascade (Coefficients R)) (s : Cascade (History R)) : Cascade (Section R) :=
  ⟨save c.first s.first, save c.second s.second⟩

theorem simulation4 (c : Cascade (Coefficients R)) :
    Processor.Simulation (reference4 c) process4 (fun s t => t = save4 c s) :=
  Cascade.simulation (simulation2 c.first) (simulation2 c.second)

def reference8 (c : FourSections (Coefficients R)) :
    Processor (FourSections (History R)) (Sample R) (Sample R) :=
  Cascade.process (reference4 c.first) (reference4 c.second)

def save8 (c : FourSections (Coefficients R)) (s : FourSections (History R)) :
    FourSections (Section R) :=
  ⟨save4 c.first s.first, save4 c.second s.second⟩

theorem simulation8 (c : FourSections (Coefficients R)) :
    Processor.Simulation (reference8 c) process8 (fun s t => t = save8 c s) :=
  Cascade.simulation (simulation4 c.first) (simulation4 c.second)

theorem reset2_eq (s : Section R) : reset2 s = (save s.coefficients IIR.zeroState, ⟨0⟩) := by
  simp [reset2, Section.reset, save, IIR.zeroState]

theorem reset4_eq (s : Cascade (Section R)) :
    reset4 s = (save4 (s.map Section.coefficients) ⟨IIR.zeroState, IIR.zeroState⟩, ⟨0⟩) := by
  simp [reset4, Section.reset, Cascade.map, save4, save, IIR.zeroState]

theorem reset8_eq (s : FourSections (Section R)) :
    reset8 s = (save8 (s.map (Cascade.map Section.coefficients))
      ⟨⟨IIR.zeroState, IIR.zeroState⟩, ⟨IIR.zeroState, IIR.zeroState⟩⟩, ⟨0⟩) := by
  simp [reset8, Section.reset, Cascade.map, save8, save4, save, IIR.zeroState]

theorem run2_eq (c : Coefficients R) (u : Nat → Sample R) (s : History R) :
    Processor.run (reference2 c) u s = Processor.run process2 u (save c s) :=
  Processor.run_simulation _ _ _ (simulation2 c) u s (save c s) rfl

theorem run4_eq (c : Cascade (Coefficients R)) (u : Nat → Sample R) (s : Cascade (History R)) :
    Processor.run (reference4 c) u s = Processor.run process4 u (save4 c s) :=
  Processor.run_simulation _ _ _ (simulation4 c) u s (save4 c s) rfl

theorem run8_eq (c : FourSections (Coefficients R)) (u : Nat → Sample R)
    (s : FourSections (History R)) :
    Processor.run (reference8 c) u s = Processor.run process8 u (save8 c s) :=
  Processor.run_simulation _ _ _ (simulation8 c) u s (save8 c s) rfl

/-- The prewarped coefficient calculation equals the legacy sine/cosine
normalization for a half-angle below Nyquist and nonnegative damping. -/
theorem prepareSection_eq_trigonometric (θ k : ℝ) (hθ : 0 ≤ θ) (hθ' : θ < Real.pi / 2)
    (hk : 0 ≤ k) :
    let alpha := Real.sin (2 * θ) * k / 2
    prepareSection (Real.tan θ) k =
      ⟨(1 - Real.cos (2 * θ)) / 2 / (1 + alpha), 2 * Real.cos (2 * θ) / (1 + alpha),
        (alpha - 1) / (1 + alpha)⟩ := by
  have hc : 0 < Real.cos θ := Real.cos_pos_of_mem_Ioo ⟨by linarith [Real.pi_pos], hθ'⟩
  have hs : 0 ≤ Real.sin θ := Real.sin_nonneg_of_nonneg_of_le_pi hθ (by linarith [Real.pi_pos])
  have hunit := Real.sin_sq_add_cos_sq θ
  simp only [Real.sin_two_mul, Real.cos_two_mul', Real.tan_eq_sin_div_cos]
  generalize Real.sin θ = s at hs hunit ⊢
  generalize Real.cos θ = c at hc hunit ⊢
  have hc' := ne_of_gt hc
  let a := k * s * c
  have ha : 0 ≤ a := by
    dsimp [a]
    positivity
  have hA : (1 + a) ≠ 0 := by positivity
  have ha' : 2 * s * c * k / 2 = a := by
    dsimp [a]
    ring
  rw [ha']
  have hgg : (s / c) * (s / c) = s ^ 2 / c ^ 2 := by rw [← div_pow, pow_two]
  have hkg : k * (s / c) = a / c ^ 2 := by
    dsimp [a]
    field_simp
  have hden : 1 + a / c ^ 2 + s ^ 2 / c ^ 2 = (1 + a) / c ^ 2 := by
    field_simp
    linarith
  have hg : s ^ 2 = (1 - (c ^ 2 - s ^ 2)) / 2 := by linarith
  have scale (n : ℝ) : (n / c ^ 2) / ((1 + a) / c ^ 2) = n / (1 + a) := by field_simp
  have hfirst : 2 * (1 - s ^ 2 / c ^ 2) = 2 * (c ^ 2 - s ^ 2) / c ^ 2 := by field_simp
  have hsecond : a / c ^ 2 - 1 - s ^ 2 / c ^ 2 = (a - 1) / c ^ 2 := by
    field_simp
    linarith
  simp only [prepareSection, hgg, hkg, hden, hfirst, hsecond, scale, Coefficients.mk.injEq]
  rw [← hg]
  simp

end DSP.ButterworthDownsampler
