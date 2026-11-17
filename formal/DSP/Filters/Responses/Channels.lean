import DSP.Filters.Responses.Realizes

/-! The kernels commute with selecting a lane. Scalar magnitude theorems then
apply independently to every channel and every prepared frequency. -/
noncomputable section
namespace DSP
open Channels

namespace OnePoleTPTLowPass

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared p i = (channel i s).magnitudeSquared (channel i p) := rfl

theorem magnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.magnitudeSquared p i = Complex.normSq ((channel i s).response (frequency i)) := by
  rw [magnitudeSquared_channel, hp i]
  exact (channel i s).magnitudeSquared_eq_normSq (frequency i)
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared p i = (channel i s).magnitudeSquared (channel i p) := rfl

theorem magnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.magnitudeSquared p i = Complex.normSq ((channel i s).response (frequency i)) := by
  rw [magnitudeSquared_channel, hp i]
  exact (channel i s).magnitudeSquared_eq_normSq (frequency i)
end OnePoleTPTHighPass

namespace TPTBaseWidth

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared p i = (channel i s).magnitudeSquared (channel i p) := rfl

theorem magnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.magnitudeSquared p i = Complex.normSq ((channel i s).response (frequency i)) := by
  rw [magnitudeSquared_channel, hp i]
  exact (channel i s).magnitudeSquared_eq_normSq (frequency i)
end TPTBaseWidth

namespace TPTSVF

theorem lowPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = (channel i s).lowPassMagnitudeSquared (channel i p) := rfl

theorem lowPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = Complex.normSq ((channel i s).lowPassResponse (frequency i)) := by
  rw [lowPassMagnitudeSquared_channel, hp i]
  exact (channel i s).lowPassMagnitudeSquared_eq_normSq (frequency i)

theorem bandPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.bandPassMagnitudeSquared p i = (channel i s).bandPassMagnitudeSquared (channel i p) := rfl

theorem bandPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.bandPassMagnitudeSquared p i = Complex.normSq ((channel i s).bandPassResponse (frequency i)) := by
  rw [bandPassMagnitudeSquared_channel, hp i]
  exact (channel i s).bandPassMagnitudeSquared_eq_normSq (frequency i)

theorem highPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = (channel i s).highPassMagnitudeSquared (channel i p) := rfl

theorem highPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = Complex.normSq ((channel i s).highPassResponse (frequency i)) := by
  rw [highPassMagnitudeSquared_channel, hp i]
  exact (channel i s).highPassMagnitudeSquared_eq_normSq (frequency i)

theorem notchMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.notchMagnitudeSquared p i = (channel i s).notchMagnitudeSquared (channel i p) := rfl

theorem notchMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.notchMagnitudeSquared p i = Complex.normSq ((channel i s).notchResponse (frequency i)) := by
  rw [notchMagnitudeSquared_channel, hp i]
  exact (channel i s).notchMagnitudeSquared_eq_normSq (frequency i)
end TPTSVF

namespace MorphingTPTSVF

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared p i = (channel i s).magnitudeSquared (channel i p) := rfl

theorem magnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.magnitudeSquared p i = Complex.normSq ((channel i s).response (frequency i)) := by
  rw [magnitudeSquared_channel, hp i]
  exact (channel i s).magnitudeSquared_eq_normSq (frequency i)
end MorphingTPTSVF

namespace TPTSVF4Pole

theorem lowPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = (channel i s).lowPassMagnitudeSquared (channel i p) := rfl

theorem lowPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = Complex.normSq ((channel i s).lowPassResponse (frequency i)) := by
  rw [lowPassMagnitudeSquared_channel, hp i]
  exact (channel i s).lowPassMagnitudeSquared_eq_normSq (frequency i)

theorem bandPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.bandPassMagnitudeSquared p i = (channel i s).bandPassMagnitudeSquared (channel i p) := rfl

theorem bandPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.bandPassMagnitudeSquared p i = Complex.normSq ((channel i s).bandPassResponse (frequency i)) := by
  rw [bandPassMagnitudeSquared_channel, hp i]
  exact (channel i s).bandPassMagnitudeSquared_eq_normSq (frequency i)

theorem highPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = (channel i s).highPassMagnitudeSquared (channel i p) := rfl

theorem highPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = Complex.normSq ((channel i s).highPassResponse (frequency i)) := by
  rw [highPassMagnitudeSquared_channel, hp i]
  exact (channel i s).highPassMagnitudeSquared_eq_normSq (frequency i)
end TPTSVF4Pole

namespace LinkwitzRileyCrossover

theorem lowPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = (channel i s).lowPassMagnitudeSquared (channel i p) := rfl

theorem lowPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.lowPassMagnitudeSquared p i = Complex.normSq ((channel i s).lowPassResponse (frequency i)) := by
  rw [lowPassMagnitudeSquared_channel, hp i]
  exact (channel i s).lowPassMagnitudeSquared_eq_normSq (frequency i)

theorem highPassMagnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = (channel i s).highPassMagnitudeSquared (channel i p) := rfl

theorem highPassMagnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.highPassMagnitudeSquared p i = Complex.normSq ((channel i s).highPassResponse (frequency i)) := by
  rw [highPassMagnitudeSquared_channel, hp i]
  exact (channel i s).highPassMagnitudeSquared_eq_normSq (frequency i)
end LinkwitzRileyCrossover

namespace TPTLadder

theorem magnitudeSquared_channel {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (i : Fin n) :
    s.magnitudeSquared p i = (channel i s).magnitudeSquared (channel i p) := rfl

theorem magnitudeSquared_channels_prepared {n : ℕ} (s : Snapshot (Fin n → ℝ))
    (p : ResponseFrequency (Fin n → ℝ)) (frequency : Fin n → ℝ)
    (hp : ∀ i, (channel i p).Represents (frequency i)) (i : Fin n) :
    s.magnitudeSquared p i = Complex.normSq ((channel i s).response (frequency i)) := by
  rw [magnitudeSquared_channel, hp i]
  exact (channel i s).magnitudeSquared_eq_normSq (frequency i)
end TPTLadder
end DSP
