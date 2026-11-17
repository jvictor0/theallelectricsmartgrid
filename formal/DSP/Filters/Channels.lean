import DSP.Transfer.Channels
import DSP.Filters.OnePoleZOHLowPass.Channels
import DSP.Filters.OnePoleTPTHighPass.Proofs
import DSP.Filters.TPTBaseWidth.Proofs
import DSP.Filters.MorphingTPTSVF.Proofs
import DSP.Filters.TPTSVF4Pole.Proofs
import DSP.Filters.TPTLadder.Proofs
import DSP.Filters.LinkwitzRileyCrossover.Proofs

/-!
Multi-channel instances of the ZOH and TPT filters. Each record is a functor, each step
commutes with reading a channel, and each scalar transfer theorem therefore
holds for every channel with that channel's coefficients.
-/

noncomputable section

namespace DSP

deriving instance Functor for OnePoleTPTLowPass
deriving instance Functor for OnePoleTPTHighPass
deriving instance Functor for TPTBaseWidth
deriving instance Functor for TPTSVF
deriving instance Functor for MorphingTPTSVF
deriving instance Functor for TPTSVF4Pole
deriving instance Functor for TPTLadder
deriving instance Functor for LinkwitzRileyCrossover

open Polynomial Channels

variable {n : ℕ} {K : Type} [Field K]

namespace OnePoleTPTLowPass

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_channels (f : OnePoleTPTLowPass (Fin n → K)) (g : Fin n → K)
    (hG : ∀ i, f.G i * (1 + g i) = g i) (hs : ∀ i, f.s i = 0) :
    Channels.Realizes step f (·.y) fun i => Bilinear.analog (C (g i)) (X + C (g i)) :=
  Channels.realizes natural (fun i => realizes (channel i f) (g i) (hG i) (hs i)) fun _ _ => rfl

end OnePoleTPTLowPass

namespace OnePoleTPTHighPass

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_channels (f : OnePoleTPTHighPass (Fin n → K)) (g : Fin n → K)
    (hG : ∀ i, f.lowPass.G i * (1 + g i) = g i) (hs : ∀ i, f.lowPass.s i = 0) :
    Channels.Realizes step f (·.y) fun i => Bilinear.analog X (X + C (g i)) :=
  Channels.realizes natural (fun i => realizes (channel i f) (g i) (hG i) (hs i)) fun _ _ => rfl

end OnePoleTPTHighPass

namespace TPTBaseWidth

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_channels (f : TPTBaseWidth (Fin n → K)) (g₁ g₂ : Fin n → K)
    (hG₁ : ∀ i, f.highPass.lowPass.G i * (1 + g₁ i) = g₁ i)
    (hs₁ : ∀ i, f.highPass.lowPass.s i = 0)
    (hG₂ : ∀ i, f.lowPass.G i * (1 + g₂ i) = g₂ i) (hs₂ : ∀ i, f.lowPass.s i = 0) :
    Channels.Realizes step f (·.lowPass.y) fun i =>
      Bilinear.analog (C (g₂ i) * X) ((X + C (g₂ i)) * (X + C (g₁ i))) :=
  Channels.realizes natural (fun i =>
      realizes (channel i f) (g₁ i) (g₂ i) (hG₁ i) (hs₁ i) (hG₂ i) (hs₂ i))
    fun _ _ => rfl

end TPTBaseWidth

namespace TPTSVF

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

variable (f : TPTSVF (Fin n → K)) (g : Fin n → K) (ht : ∀ i, (channel i f).Tuned (g i))
  (hr : ∀ i, (channel i f).AtRest)

include ht hr

theorem realizes_lowPass_channels :
    Channels.Realizes step f (·.lowPass) fun i =>
      Bilinear.analog (lowPassNumerator (g i)) (denominator (g i) (f.k i)) :=
  Channels.realizes natural
    (fun i => realizes_lowPass (channel i f) (g i) (ht i) (hr i)) fun _ _ => rfl

theorem realizes_bandPass_channels :
    Channels.Realizes step f (·.bandPass) fun i =>
      Bilinear.analog (bandPassNumerator (g i)) (denominator (g i) (f.k i)) :=
  Channels.realizes natural
    (fun i => realizes_bandPass (channel i f) (g i) (ht i) (hr i)) fun _ _ => rfl

theorem realizes_highPass_channels :
    Channels.Realizes step f (·.highPass) fun i =>
      Bilinear.analog highPassNumerator (denominator (g i) (f.k i)) :=
  Channels.realizes natural
    (fun i => realizes_highPass (channel i f) (g i) (ht i) (hr i)) fun _ _ => rfl

theorem realizes_notch_channels :
    Channels.Realizes step f (·.notch) fun i =>
      Bilinear.analog (notchNumerator (g i)) (denominator (g i) (f.k i)) :=
  Channels.realizes natural
    (fun i => realizes_notch (channel i f) (g i) (ht i) (hr i)) fun _ _ => rfl

end TPTSVF

namespace MorphingTPTSVF

variable [IfLess K]

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_channels [LawfulIfLess K] (f : MorphingTPTSVF (Fin n → K)) (g : Fin n → K)
    (ht : ∀ i, (channel i f).svf.Tuned (g i)) (hr : ∀ i, (channel i f).svf.AtRest) :
    Channels.Realizes step f (·.output) fun i =>
      Bilinear.analog (numerator (g i) (stateWeights (channel i f)))
        (TPTSVF.denominator (g i) (f.svf.k i)) :=
  Channels.realizes natural (fun i => realizes (channel i f) (g i) (ht i) (hr i)) fun _ _ => rfl

omit [IfLess K] in
/-- Every channel has its own blend, in either half. -/
theorem realizes_blend_channels (f : MorphingTPTSVF (Fin n → ℝ)) (g blend : Fin n → ℝ)
    (ht : ∀ i, (channel i f).svf.Tuned (g i)) (hr : ∀ i, (channel i f).svf.AtRest)
    (hc : ∀ i, f.cosine i = Real.cos (2 * Real.pi * phase (blend i)))
    (hs : ∀ i, f.sine i = Real.sin (2 * Real.pi * phase (blend i)))
    (hb : ∀ i, f.blend i = clamped (blend i)) :
    Channels.Realizes step f (·.output) fun i =>
      Bilinear.analog (numerator (g i) (blendWeights (blend i)))
        (TPTSVF.denominator (g i) (f.svf.k i)) :=
  Channels.realizes natural (fun i =>
      realizes_blend (channel i f) (g i) (blend i) (ht i) (hr i) (hc i) (hs i) (hb i))
    fun _ _ => rfl

end MorphingTPTSVF

namespace TPTSVF4Pole

theorem natural_highPass : Natural (n := n) (stepHighPass (R := Fin n → K)) stepHighPass :=
  fun _ _ _ => rfl

theorem natural_lowPass : Natural (n := n) (stepLowPass (R := Fin n → K)) stepLowPass :=
  fun _ _ _ => rfl

theorem natural_bandPass : Natural (n := n) (stepBandPass (R := Fin n → K)) stepBandPass :=
  fun _ _ _ => rfl

variable (f : TPTSVF4Pole (Fin n → K)) (g₁ g₂ : Fin n → K)
  (ht₁ : ∀ i, (channel i f).stage1.Tuned (g₁ i)) (hr₁ : ∀ i, (channel i f).stage1.AtRest)
  (ht₂ : ∀ i, (channel i f).stage2.Tuned (g₂ i)) (hr₂ : ∀ i, (channel i f).stage2.AtRest)

include ht₁ hr₁ ht₂ hr₂

theorem realizes_highPass_channels :
    Channels.Realizes stepHighPass f (·.stage2.highPass) fun i =>
      Bilinear.analog (TPTSVF.highPassNumerator * TPTSVF.highPassNumerator)
        (TPTSVF.denominator (g₂ i) (f.stage2.k i) * TPTSVF.denominator (g₁ i) (f.stage1.k i)) :=
  Channels.realizes natural_highPass (fun i =>
      realizes_highPass (channel i f) (g₁ i) (g₂ i) (ht₁ i) (hr₁ i) (ht₂ i) (hr₂ i))
    fun _ _ => rfl

theorem realizes_lowPass_channels :
    Channels.Realizes stepLowPass f (·.stage2.lowPass) fun i =>
      Bilinear.analog (TPTSVF.lowPassNumerator (g₂ i) * TPTSVF.lowPassNumerator (g₁ i))
        (TPTSVF.denominator (g₂ i) (f.stage2.k i) * TPTSVF.denominator (g₁ i) (f.stage1.k i)) :=
  Channels.realizes natural_lowPass (fun i =>
      realizes_lowPass (channel i f) (g₁ i) (g₂ i) (ht₁ i) (hr₁ i) (ht₂ i) (hr₂ i))
    fun _ _ => rfl

theorem realizes_bandPass_channels :
    Channels.Realizes stepBandPass f (·.stage2.bandPass) fun i =>
      Bilinear.analog (TPTSVF.bandPassNumerator (g₂ i) * TPTSVF.bandPassNumerator (g₁ i))
        (TPTSVF.denominator (g₂ i) (f.stage2.k i) * TPTSVF.denominator (g₁ i) (f.stage1.k i)) :=
  Channels.realizes natural_bandPass (fun i =>
      realizes_bandPass (channel i f) (g₁ i) (g₂ i) (ht₁ i) (hr₁ i) (ht₂ i) (hr₂ i))
    fun _ _ => rfl

end TPTSVF4Pole

namespace TPTLadder

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_channels (f : TPTLadder (Fin n → K)) (g₁ g₂ g₃ g₄ : Fin n → K)
    (h : ∀ i, (channel i f).Ready (g₁ i) (g₂ i) (g₃ i) (g₄ i)) :
    Channels.Realizes step f (·.stage4.y) fun i =>
      Bilinear.analog (numerator (g₁ i) (g₂ i) (g₃ i) (g₄ i))
        (denominator (f.k i) (g₁ i) (g₂ i) (g₃ i) (g₄ i)) :=
  Channels.realizes natural
    (fun i => realizes (channel i f) (g₁ i) (g₂ i) (g₃ i) (g₄ i) (h i)) fun _ _ => rfl

theorem realizes_equal_channels (f : TPTLadder (Fin n → K)) (g : Fin n → K)
    (h : ∀ i, (channel i f).Ready (g i) (g i) (g i) (g i)) :
    Channels.Realizes step f (·.stage4.y) fun i =>
      Bilinear.analog (C (g i) ^ 4) ((X + C (g i)) ^ 4 + C (f.k i) * C (g i) ^ 4) :=
  Channels.realizes natural (fun i => realizes_equal (channel i f) (g i) (h i)) fun _ _ => rfl

end TPTLadder

namespace LinkwitzRileyCrossover

theorem natural : Natural (n := n) (step (R := Fin n → K)) step := fun _ _ _ => rfl

theorem realizes_lowPass_channels (f : LinkwitzRileyCrossover (Fin n → K)) (g : Fin n → K)
    (h : ∀ i, (channel i f).Ready (g i)) :
    Channels.Realizes step f (·.lowSecond.lowPass) fun i =>
      Bilinear.analog (TPTSVF.lowPassNumerator (g i) * TPTSVF.lowPassNumerator (g i))
        (TPTSVF.denominator (g i) (f.lowSecond.k i) * TPTSVF.denominator (g i) (f.first.k i)) :=
  Channels.realizes natural (fun i => realizes_lowPass (channel i f) (g i) (h i)) fun _ _ => rfl

theorem realizes_highPass_channels (f : LinkwitzRileyCrossover (Fin n → K)) (g : Fin n → K)
    (h : ∀ i, (channel i f).Ready (g i)) :
    Channels.Realizes step f (·.highSecond.highPass) fun i =>
      Bilinear.analog (TPTSVF.highPassNumerator * TPTSVF.highPassNumerator)
        (TPTSVF.denominator (g i) (f.highSecond.k i) * TPTSVF.denominator (g i) (f.first.k i)) :=
  Channels.realizes natural (fun i => realizes_highPass (channel i f) (g i) (h i)) fun _ _ => rfl

end LinkwitzRileyCrossover

end DSP

end
