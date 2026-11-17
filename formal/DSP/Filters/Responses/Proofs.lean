import DSP.Filters.Responses
import DSP.Response.Proofs
import DSP.Filters.Channels

set_option maxRecDepth 4000

noncomputable section
namespace DSP
open Polynomial

instance lowPassSnapshotFunctor : Functor OnePoleTPTLowPass.Snapshot where
  map f s := ⟨f s.g⟩
instance highPassSnapshotFunctor : Functor OnePoleTPTHighPass.Snapshot where
  map f s := ⟨f s.g⟩
instance baseWidthSnapshotFunctor : Functor TPTBaseWidth.Snapshot where
  map f s := ⟨f <$> s.highPass, f <$> s.lowPass⟩
instance svfSnapshotFunctor : Functor TPTSVF.Snapshot where
  map f s := ⟨f s.g, f s.k⟩
instance morphSnapshotFunctor : Functor MorphingTPTSVF.Snapshot where
  map f s := ⟨f <$> s.svf, f s.low, f s.band, f s.high⟩
instance svf4SnapshotFunctor : Functor TPTSVF4Pole.Snapshot where
  map f s := ⟨f <$> s.stage1, f <$> s.stage2⟩
instance crossoverSnapshotFunctor : Functor LinkwitzRileyCrossover.Snapshot where
  map f s := ⟨f <$> s.first, f <$> s.lowSecond, f <$> s.highSecond⟩
instance ladderSnapshotFunctor : Functor TPTLadder.Snapshot where
  map f s := ⟨f s.g1, f s.g2, f s.g3, f s.g4, f s.k⟩

namespace Response

theorem quotient_norm (nr ni dr di : ℝ) :
    ratio (normSquared nr ni) (normSquared dr di) =
      Complex.normSq ((⟨nr, ni⟩ : ℂ) / (⟨dr, di⟩ : ℂ)) := by
  rw [normSquared_eq, normSquared_eq, ratio_eq _ _ (Complex.normSq_nonneg _), Complex.normSq_div]

theorem real_quotient_norm (n dr di : ℝ) :
    ratio (n*n) (normSquared dr di) = Complex.normSq ((n : ℂ) / (⟨dr, di⟩ : ℂ)) := by
  simpa [normSquared, fma_eq] using quotient_norm n 0 dr di

/-- Any transfer invariant under a common nonzero coordinate scale can use the
prepared half-angle pair instead of the delay pair (1-q,1+q), including Nyquist. -/
theorem frequency_eq_delay (H : ℂ → ℂ → ℂ)
    (scale : ∀ a b t, t ≠ 0 → H (t*a) (t*b) = H a b) (f : ℝ) :
    H ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f)) =
      H (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) := by
  obtain ⟨t, ht, ha, hb⟩ := halfAngle_coordinates f
  rw [ha, hb, scale _ _ _ ht]

end Response

namespace OnePoleTPTLowPass
def Snapshot.transfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  Response.onePole s.g a b

theorem Snapshot.transfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.transfer (t*a) (t*b) = s.transfer a b := Response.onePole_scale _ _ _ _ ht

def Snapshot.response (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).transfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.response_delay (s : Snapshot ℝ) (f : ℝ) :
    s.response f = (Complex.ofReal <$> s).transfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.transfer_scale _) f

theorem Snapshot.magnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).transfer
      ⟨0, f.sinHalf⟩ (f.cosHalf : ℂ)) := by
  unfold Snapshot.magnitudeSquared
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Snapshot.transfer, Response.onePole, Complex.normSq_div]
  congr 1 <;> simp [Response.normSquared, fma_eq, Complex.normSq_apply, Functor.map]

theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.response f) :=
  s.magnitudeSquared_prepared _
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
def Snapshot.transfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  Response.highPole s.g a b

theorem Snapshot.transfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.transfer (t*a) (t*b) = s.transfer a b := Response.highPole_scale _ _ _ _ ht

def Snapshot.response (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).transfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.response_delay (s : Snapshot ℝ) (f : ℝ) :
    s.response f = (Complex.ofReal <$> s).transfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.transfer_scale _) f

theorem Snapshot.magnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).transfer
      ⟨0, f.sinHalf⟩ (f.cosHalf : ℂ)) := by
  unfold Snapshot.magnitudeSquared
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Snapshot.transfer, Response.highPole, Complex.normSq_div]
  congr 1 <;> simp [Response.normSquared, fma_eq, Complex.normSq_apply, Functor.map]

theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.response f) :=
  s.magnitudeSquared_prepared _
end OnePoleTPTHighPass

namespace TPTSVF

def Snapshot.weightedTransfer {K : Type} [Field K] (s : Snapshot K) (l m h a b : K) : K :=
  Response.svf s.g s.k l m h a b

theorem Snapshot.weightedTransfer_scale (s : Snapshot ℂ) (l m h a b t : ℂ) (ht : t ≠ 0) :
    s.weightedTransfer l m h (t*a) (t*b) = s.weightedTransfer l m h a b :=
  Response.svf_scale _ _ _ _ _ _ _ _ ht

theorem Snapshot.weightedMagnitudeSquared_prepared (s : Snapshot ℝ) (l m h : ℝ)
    (f : ResponseFrequency ℝ) :
    s.weightedMagnitudeSquared l m h f = Complex.normSq
      ((Complex.ofReal <$> s).weightedTransfer l m h ⟨0, f.sinHalf⟩ f.cosHalf) := by
  unfold Snapshot.weightedMagnitudeSquared
  rw [Response.quotient_norm]
  congr 1
  unfold Snapshot.weightedTransfer Response.svf
  congr 1 <;> apply Complex.ext <;>
    simp [Complex.ofReal_re, Complex.ofReal_im, Functor.map] <;> ring

def Snapshot.lowPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.weightedTransfer 1 0 0 a b

def Snapshot.lowPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).lowPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.lowPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.lowPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).lowPassTransfer
      ⟨0, f.sinHalf⟩ f.cosHalf) := by
  simpa [Snapshot.lowPassMagnitudeSquared, Snapshot.lowPassTransfer] using
    s.weightedMagnitudeSquared_prepared 1 0 0 f

theorem Snapshot.lowPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.lowPassResponse f) :=
  s.lowPassMagnitudeSquared_prepared _

theorem Snapshot.lowPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassResponse f = (Complex.ofReal <$> s).lowPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.weightedTransfer_scale _ 1 0 0) f

def Snapshot.bandPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.weightedTransfer 0 1 0 a b

def Snapshot.bandPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).bandPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.bandPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.bandPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).bandPassTransfer
      ⟨0, f.sinHalf⟩ f.cosHalf) := by
  simpa [Snapshot.bandPassMagnitudeSquared, Snapshot.bandPassTransfer] using
    s.weightedMagnitudeSquared_prepared 0 1 0 f

theorem Snapshot.bandPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.bandPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.bandPassResponse f) :=
  s.bandPassMagnitudeSquared_prepared _

theorem Snapshot.bandPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.bandPassResponse f = (Complex.ofReal <$> s).bandPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.weightedTransfer_scale _ 0 1 0) f

def Snapshot.highPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.weightedTransfer 0 0 1 a b

def Snapshot.highPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).highPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.highPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.highPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).highPassTransfer
      ⟨0, f.sinHalf⟩ f.cosHalf) := by
  simpa [Snapshot.highPassMagnitudeSquared, Snapshot.highPassTransfer] using
    s.weightedMagnitudeSquared_prepared 0 0 1 f

theorem Snapshot.highPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.highPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.highPassResponse f) :=
  s.highPassMagnitudeSquared_prepared _

theorem Snapshot.highPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.highPassResponse f = (Complex.ofReal <$> s).highPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.weightedTransfer_scale _ 0 0 1) f

def Snapshot.notchTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.weightedTransfer 1 0 1 a b

def Snapshot.notchResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).notchTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.notchMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.notchMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).notchTransfer
      ⟨0, f.sinHalf⟩ f.cosHalf) := by
  simpa [Snapshot.notchMagnitudeSquared, Snapshot.notchTransfer] using
    s.weightedMagnitudeSquared_prepared 1 0 1 f

theorem Snapshot.notchMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.notchMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.notchResponse f) :=
  s.notchMagnitudeSquared_prepared _

theorem Snapshot.notchResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.notchResponse f = (Complex.ofReal <$> s).notchTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.weightedTransfer_scale _ 1 0 1) f
end TPTSVF

namespace MorphingTPTSVF

def Snapshot.transfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.svf.weightedTransfer s.low s.band s.high a b

theorem Snapshot.transfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.transfer (t*a) (t*b) = s.transfer a b :=
  s.svf.weightedTransfer_scale _ _ _ _ _ _ ht

def Snapshot.response (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).transfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.response_delay (s : Snapshot ℝ) (f : ℝ) :
    s.response f = (Complex.ofReal <$> s).transfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.transfer_scale _) f

theorem Snapshot.magnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).transfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  s.svf.weightedMagnitudeSquared_prepared s.low s.band s.high f

theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.response f) :=
  s.magnitudeSquared_prepared _
end MorphingTPTSVF

namespace TPTBaseWidth

def Snapshot.transfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.highPass.transfer a b * s.lowPass.transfer a b

theorem Snapshot.transfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.transfer (t*a) (t*b) = s.transfer a b := by
  exact congrArg₂ (· * ·) (s.highPass.transfer_scale a b t ht) (s.lowPass.transfer_scale a b t ht)

def Snapshot.response (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).transfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.response_delay (s : Snapshot ℝ) (f : ℝ) :
    s.response f = (Complex.ofReal <$> s).transfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.transfer_scale _) f

theorem Snapshot.magnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).transfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.highPass.magnitudeSquared_prepared f) (s.lowPass.magnitudeSquared_prepared f)

theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.response f) :=
  s.magnitudeSquared_prepared _
end TPTBaseWidth

namespace TPTSVF4Pole

def Snapshot.lowPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.stage1.lowPassTransfer a b * s.stage2.lowPassTransfer a b

theorem Snapshot.lowPassTransfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.lowPassTransfer (t*a) (t*b) = s.lowPassTransfer a b := by
  exact congrArg₂ (· * ·) (s.stage1.weightedTransfer_scale 1 0 0 a b t ht) (s.stage2.weightedTransfer_scale 1 0 0 a b t ht)

def Snapshot.lowPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).lowPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.lowPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassResponse f = (Complex.ofReal <$> s).lowPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.lowPassTransfer_scale _) f

theorem Snapshot.lowPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.lowPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).lowPassTransfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.stage1.lowPassMagnitudeSquared_prepared f) (s.stage2.lowPassMagnitudeSquared_prepared f)

theorem Snapshot.lowPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.lowPassResponse f) :=
  s.lowPassMagnitudeSquared_prepared _

def Snapshot.bandPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.stage1.bandPassTransfer a b * s.stage2.bandPassTransfer a b

theorem Snapshot.bandPassTransfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.bandPassTransfer (t*a) (t*b) = s.bandPassTransfer a b := by
  exact congrArg₂ (· * ·) (s.stage1.weightedTransfer_scale 0 1 0 a b t ht) (s.stage2.weightedTransfer_scale 0 1 0 a b t ht)

def Snapshot.bandPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).bandPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.bandPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.bandPassResponse f = (Complex.ofReal <$> s).bandPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.bandPassTransfer_scale _) f

theorem Snapshot.bandPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.bandPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).bandPassTransfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.stage1.bandPassMagnitudeSquared_prepared f) (s.stage2.bandPassMagnitudeSquared_prepared f)

theorem Snapshot.bandPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.bandPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.bandPassResponse f) :=
  s.bandPassMagnitudeSquared_prepared _

def Snapshot.highPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.stage1.highPassTransfer a b * s.stage2.highPassTransfer a b

theorem Snapshot.highPassTransfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.highPassTransfer (t*a) (t*b) = s.highPassTransfer a b := by
  exact congrArg₂ (· * ·) (s.stage1.weightedTransfer_scale 0 0 1 a b t ht) (s.stage2.weightedTransfer_scale 0 0 1 a b t ht)

def Snapshot.highPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).highPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.highPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.highPassResponse f = (Complex.ofReal <$> s).highPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.highPassTransfer_scale _) f

theorem Snapshot.highPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.highPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).highPassTransfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.stage1.highPassMagnitudeSquared_prepared f) (s.stage2.highPassMagnitudeSquared_prepared f)

theorem Snapshot.highPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.highPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.highPassResponse f) :=
  s.highPassMagnitudeSquared_prepared _
end TPTSVF4Pole

namespace LinkwitzRileyCrossover

def Snapshot.lowPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.first.lowPassTransfer a b * s.lowSecond.lowPassTransfer a b

theorem Snapshot.lowPassTransfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.lowPassTransfer (t*a) (t*b) = s.lowPassTransfer a b := by
  exact congrArg₂ (· * ·) (s.first.weightedTransfer_scale 1 0 0 a b t ht) (s.lowSecond.weightedTransfer_scale 1 0 0 a b t ht)

def Snapshot.lowPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).lowPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.lowPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassResponse f = (Complex.ofReal <$> s).lowPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.lowPassTransfer_scale _) f

theorem Snapshot.lowPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.lowPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).lowPassTransfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.first.lowPassMagnitudeSquared_prepared f) (s.lowSecond.lowPassMagnitudeSquared_prepared f)

theorem Snapshot.lowPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.lowPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.lowPassResponse f) :=
  s.lowPassMagnitudeSquared_prepared _

def Snapshot.highPassTransfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  s.first.highPassTransfer a b * s.highSecond.highPassTransfer a b

theorem Snapshot.highPassTransfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.highPassTransfer (t*a) (t*b) = s.highPassTransfer a b := by
  exact congrArg₂ (· * ·) (s.first.weightedTransfer_scale 0 0 1 a b t ht) (s.highSecond.weightedTransfer_scale 0 0 1 a b t ht)

def Snapshot.highPassResponse (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).highPassTransfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.highPassResponse_delay (s : Snapshot ℝ) (f : ℝ) :
    s.highPassResponse f = (Complex.ofReal <$> s).highPassTransfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.highPassTransfer_scale _) f

theorem Snapshot.highPassMagnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.highPassMagnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).highPassTransfer ⟨0, f.sinHalf⟩ f.cosHalf) :=
  Response.cascade _ _ _ _ (s.first.highPassMagnitudeSquared_prepared f) (s.highSecond.highPassMagnitudeSquared_prepared f)

theorem Snapshot.highPassMagnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.highPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.highPassResponse f) :=
  s.highPassMagnitudeSquared_prepared _
end LinkwitzRileyCrossover

namespace TPTLadder

def Snapshot.transfer {K : Type} [Field K] (s : Snapshot K) (a b : K) : K :=
  Response.ladder s.g1 s.g2 s.g3 s.g4 s.k a b

theorem Snapshot.transfer_scale (s : Snapshot ℂ) (a b t : ℂ) (ht : t ≠ 0) :
    s.transfer (t*a) (t*b) = s.transfer a b := Response.ladder_scale _ _ _ _ _ _ _ _ ht

def Snapshot.response (s : Snapshot ℝ) (f : ℝ) : ℂ :=
  (Complex.ofReal <$> s).transfer ⟨0, Real.sin (Real.pi*f)⟩ (Real.cos (Real.pi*f))

theorem Snapshot.response_delay (s : Snapshot ℝ) (f : ℝ) :
    s.response f = (Complex.ofReal <$> s).transfer
      (1 - ResponseFrequency.unitDelay f) (1 + ResponseFrequency.unitDelay f) :=
  Response.frequency_eq_delay _ (Snapshot.transfer_scale _) f

theorem Snapshot.magnitudeSquared_prepared (s : Snapshot ℝ) (f : ResponseFrequency ℝ) :
    s.magnitudeSquared f = Complex.normSq ((Complex.ofReal <$> s).transfer ⟨0, f.sinHalf⟩ f.cosHalf) := by
  unfold Snapshot.magnitudeSquared
  rw [Response.real_quotient_norm]
  congr 1
  dsimp only [Snapshot.transfer, Response.ladder, Functor.map, ladderSnapshotFunctor]
  congr 1 <;> apply Complex.ext <;> simp [fma_eq] <;> ring

theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (f : ℝ) :
    s.magnitudeSquared (ResponseFrequency.ofCyclesPerSample f) = Complex.normSq (s.response f) :=
  s.magnitudeSquared_prepared _
end TPTLadder

end DSP
