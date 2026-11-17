import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF.Proofs
import DSP.IfLess.Real
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic

noncomputable section

namespace DSP.MorphingTPTSVF

open Polynomial

/-- Weights of the lowpass, bandpass and highpass responses. -/
structure Weights (K : Type) where
  low : K
  band : K
  high : K

section Transfer

variable {K : Type} [Field K] [IfLess K] [LawfulIfLess K]

def weights (cosine sine : K) (upperHalf : Bool) : Weights K :=
  if upperHalf then ⟨0, cosine, sine⟩ else ⟨cosine, sine, 0⟩

/-- The morphed analog numerator: w_low g² + w_band g s + w_high s², over the
shared SVF denominator s² + k g s + g². -/
def numerator (g : K) (w : Weights K) : K[X] :=
  C w.low * TPTSVF.lowPassNumerator g + C w.band * TPTSVF.bandPassNumerator g +
    C w.high * TPTSVF.highPassNumerator

/-- The selection compares the stored blend with one half. -/
def upperHalf (f : MorphingTPTSVF K) : Bool := LawfulIfLess.less 0.5 f.blend

def stateWeights (f : MorphingTPTSVF K) : Weights K := weights f.cosine f.sine (upperHalf f)

theorem stateWeights_step (f : MorphingTPTSVF K) (input : Sample K) :
    stateWeights (step f input) = stateWeights f := rfl

omit [LawfulIfLess K] in
/-- Every response of the inner SVF keeps its specification. -/
theorem realizes_svf (f : MorphingTPTSVF K) {field : TPTSVF K → K} {H : Bilinear.Stream K}
    (h : Bilinear.Realizes TPTSVF.step f.svf field H) :
    Bilinear.Realizes step f (fun s => field s.svf) H :=
  Bilinear.Realizes.project step TPTSVF.step (·.svf) (fun _ _ => rfl) h

/-- From rest, the morphed output realizes the weighted analog response. The
weights are coefficients of the state, like g and k. -/
theorem realizes (f : MorphingTPTSVF K) (g : K) (ht : f.svf.Tuned g) (hr : f.svf.AtRest) :
    Bilinear.Realizes step f (·.output)
      (Bilinear.analog (numerator g (stateWeights f)) (TPTSVF.denominator g f.svf.k)) := by
  have low := (realizes_svf f (TPTSVF.realizes_lowPass f.svf g ht hr)).mul
    (fun s => (stateWeights s).low) (fun s x => by rw [stateWeights_step])
  have band := (realizes_svf f (TPTSVF.realizes_bandPass f.svf g ht hr)).mul
    (fun s => (stateWeights s).band) (fun s x => by rw [stateWeights_step])
  have high := (realizes_svf f (TPTSVF.realizes_highPass f.svf g ht hr)).mul
    (fun s => (stateWeights s).high) (fun s x => by rw [stateWeights_step])
  refine (((low.add band).add high).congr ?_).transfer_eq ?_
  · intro s x
    simp only [fma_eq, step, stateWeights, weights, upperHalf, LawfulIfLess.ifLess_eq]
    cases LawfulIfLess.less (0.5 : K) s.blend <;> simp <;> ring
  · simp only [Bilinear.const_mul_analog, Bilinear.analog_add, numerator]

end Transfer

/-! The blend parameter, as the app maps it: clamp to [0, 1]; the phase is a
quarter turn across each half. -/

def clamped (blend : ℝ) : ℝ := min 1 (max 0 blend)

def upperHalfOf (blend : ℝ) : Bool := decide (1 / 2 < clamped blend)

def phase (blend : ℝ) : ℝ :=
  if upperHalfOf blend then (clamped blend - 1 / 2) * (1 / 2) else clamped blend * (1 / 2)

def blendWeights (blend : ℝ) : Weights ℝ :=
  weights (Real.cos (2 * Real.pi * phase blend)) (Real.sin (2 * Real.pi * phase blend))
    (upperHalfOf blend)

/-- The morph is equal power for every blend. -/
theorem blendWeights_power (blend : ℝ) :
    (blendWeights blend).low ^ 2 + (blendWeights blend).band ^ 2 +
      (blendWeights blend).high ^ 2 = 1 := by
  have h := Real.cos_sq_add_sin_sq (2 * Real.pi * phase blend)
  unfold blendWeights weights
  split <;> simp only <;> linear_combination h

theorem quarter_turn : 2 * Real.pi * (1 / 4) = Real.pi / 2 := by ring

theorem blendWeights_zero : blendWeights 0 = ⟨1, 0, 0⟩ := by
  have hc : clamped 0 = 0 := by norm_num [clamped]
  have hu : upperHalfOf 0 = false := by simp [upperHalfOf, hc]
  have hp : phase 0 = 0 := by simp [phase, hu, hc]
  rw [blendWeights, hu, hp]
  simp [weights]

theorem blendWeights_half : blendWeights (1 / 2) = ⟨0, 1, 0⟩ := by
  have hc : clamped (1 / 2) = 1 / 2 := by norm_num [clamped]
  have hu : upperHalfOf (1 / 2) = false := by
    rw [upperHalfOf, hc]
    simp
  have hp : phase (1 / 2) = 1 / 4 := by
    rw [phase, hu, hc]
    norm_num
  rw [blendWeights, hu, hp, quarter_turn]
  simp [weights]

theorem blendWeights_one : blendWeights 1 = ⟨0, 0, 1⟩ := by
  have hc : clamped 1 = 1 := by norm_num [clamped]
  have hu : upperHalfOf 1 = true := by
    rw [upperHalfOf, hc]
    norm_num
  have hp : phase 1 = 1 / 4 := by
    rw [phase, hu, hc]
    norm_num
  rw [blendWeights, hu, hp, quarter_turn]
  simp [weights]

/-- With coefficients computed from a blend, the output realizes the analog
morph at that blend. -/
theorem realizes_blend (f : MorphingTPTSVF ℝ) (g blend : ℝ) (ht : f.svf.Tuned g)
    (hr : f.svf.AtRest) (hc : f.cosine = Real.cos (2 * Real.pi * phase blend))
    (hs : f.sine = Real.sin (2 * Real.pi * phase blend)) (hb : f.blend = clamped blend) :
    Bilinear.Realizes step f (·.output)
      (Bilinear.analog (numerator g (blendWeights blend)) (TPTSVF.denominator g f.svf.k)) := by
  have h := realizes f g ht hr
  have hu : upperHalf f = upperHalfOf blend := by
    change decide ((0.5 : ℝ) < f.blend) = decide (1 / 2 < clamped blend)
    norm_num [hb]
  rwa [stateWeights, hc, hs, hu] at h

/-- At blend 0, 1/2 and 1 the morph is exactly the lowpass, bandpass and highpass. -/
theorem numerator_endpoints (g : ℝ) :
    numerator g (blendWeights 0) = TPTSVF.lowPassNumerator g ∧
      numerator g (blendWeights (1 / 2)) = TPTSVF.bandPassNumerator g ∧
      numerator g (blendWeights 1) = TPTSVF.highPassNumerator := by
  refine ⟨?_, ?_, ?_⟩
  · simp [numerator, blendWeights_zero]
  · rw [blendWeights_half]
    simp [numerator]
  · simp [numerator, blendWeights_one]

end DSP.MorphingTPTSVF

end
