import DSP.Filters.SchroederAllPass
import DSP.DelayBuffer.Transfer
import DSP.DelayBuffer.Energy
import DSP.Filters.NormalizedAllPass.Proofs

namespace DSP.SchroederAllPass

variable {capacity : ℕ}

section Transfer
variable {K : Type} [Field K]
open Bilinear

def response (g q : K) (N : ℕ) : K := (q ^ N - g) / (1 - g * q ^ N)

/-- Arbitrary input streams from rest, derived through the shared buffer theorem. -/
theorem trajectory_series (c : Coefficients capacity K) (s : ℕ → State capacity K)
    (u : ℕ → K) (h0 : (s 0).buffer.history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = step c (s n) ⟨u n⟩) :
    (1 - const c.gain * delay ^ (c.age.val + 1)) * ser (fun n => (s (n + 1)).y) =
      (delay ^ (c.age.val + 1) - const c.gain) * ser u := by
  let y := fun n => (s (n + 1)).y
  let w := fun n => c.gain * y n + u n
  have hb : ∀ n, (s (n + 1)).buffer = (s n).buffer.push (w n) := by
    intro n
    dsimp [w, y]
    rw [hs n]
    simp [step]
  have hd := DelayBuffer.read_series (fun n => (s n).buffer) w h0 hb c.age
  have hy : ser y = -(const c.gain * ser u) + ser (fun n => (s n).buffer.read c.age) := by
    have he : ∀ n, y n = (s n).buffer.read c.age - c.gain * u n := by
      intro n
      dsimp [y]
      rw [hs n]
      simp [step]
      ring
    rw [ser_congr he, ser_sub, ser_mul]
    ring
  have hw : ser w = const c.gain * ser y + ser u := by
    dsimp [w]
    rw [ser_add, ser_mul]
  rw [hw] at hd
  change (1 - const c.gain * delay ^ (c.age.val + 1)) * ser y = _
  linear_combination hy + hd

theorem stream_denominator_ne_zero (c : Coefficients capacity K) :
    1 - const c.gain * delay ^ (c.age.val + 1) ≠ (0 : Stream K) := by
  have h := embed_ne_zero (1 - PowerSeries.C c.gain * PowerSeries.X ^ (c.age.val + 1))
    (by simp)
  simpa [const, delay] using h

theorem realizes (c : Coefficients capacity K) (s : State capacity K)
    (h0 : s.buffer.history = fun _ => 0) :
    Realizes (step c) s (·.y) (response (const c.gain) delay (c.age.val + 1)) := by
  intro u
  have h := trajectory_series c (Step.states (step c) (fun n => ⟨u n⟩) s) u h0
    (fun _ => rfl)
  unfold response
  rw [div_mul_eq_mul_div, eq_div_iff (stream_denominator_ne_zero c)]
  exact (mul_comm _ _).trans h

/-- The same realized recurrence evaluated on a complex (or other field) mode.
The history premise describes the circulating write amplitude. -/
theorem mode_transfer (c : Coefficients capacity K) (s : State capacity K) (x q : K)
    (hh : ∀ i, s.buffer.read i = q ^ (i.val + 1) *
      (c.gain * (step c s ⟨x⟩).y + x))
    (hd : 1 - c.gain * q ^ (c.age.val + 1) ≠ 0) :
    (step c s ⟨x⟩).y = response c.gain q (c.age.val + 1) * x := by
  have h := hh c.age
  simp only [step, fma_eq] at h ⊢
  unfold response
  rw [div_mul_eq_mul_div, eq_div_iff hd]
  linear_combination h

end Transfer

theorem response_unit (g : ℝ) (q : ℂ) (N : ℕ) (hg : |g| < 1)
    (hq : Complex.normSq q = 1) : Complex.normSq (response (g : ℂ) q N) = 1 := by
  have h := NormalizedAllPass.response_unit (-g) (q ^ N) (by simpa using hg)
    (by simp [map_pow, hq])
  simpa [response, NormalizedAllPass.response, sub_eq_add_neg, add_comm] using h

/-- Raw buffer energy needs the factor 1-g²; this realization does not store
the normalized lattice state. The identity itself also holds at g=±1. -/
theorem step_energy (c : Coefficients capacity ℝ) (s : State capacity ℝ) (x : Sample ℝ) :
    (1 - c.gain ^ 2) * (step c s x).y ^ 2 +
        DelayBuffer.energy (step c s x).buffer (c.age.val + 1) 1 =
      (1 - c.gain ^ 2) * x.x ^ 2 + DelayBuffer.energy s.buffer (c.age.val + 1) 1 := by
  have h := DelayBuffer.lossless_energy s.buffer
    (c.gain * (-c.gain * x.x + s.buffer.read c.age) + x.x) c.age
  simp only [step, fma_eq]
  linear_combination h

noncomputable def energy (c : Coefficients capacity ℝ) (s : State capacity ℝ) : ℝ :=
  DelayBuffer.energy s.buffer (c.age.val + 1) 1 / (1 - c.gain ^ 2)

theorem energy_nonneg (c : Coefficients capacity ℝ) (s : State capacity ℝ)
    (hg : |c.gain| < 1) : 0 ≤ energy c s := by
  apply div_nonneg (DelayBuffer.energy_nonneg _ _ _)
  have h := (sq_lt_one_iff_abs_lt_one c.gain).mpr hg
  linarith

theorem step_energy_normalized (c : Coefficients capacity ℝ) (s : State capacity ℝ)
    (x : Sample ℝ) (hg : |c.gain| < 1) :
    (step c s x).y ^ 2 + energy c (step c s x) = x.x ^ 2 + energy c s := by
  have hp := (sq_lt_one_iff_abs_lt_one c.gain).mpr hg
  have hd : 1 - c.gain ^ 2 ≠ 0 := by linarith
  have h := step_energy c s x
  unfold energy
  field_simp
  nlinarith [h]

/-- Fixed gain and tap: total output plus residual storage equals total input
plus initial storage. Retuning the storage metric is deliberately not assumed. -/
theorem trajectory_energy (c : Coefficients capacity ℝ) (s : ℕ → State capacity ℝ)
    (u : ℕ → ℝ) (hg : |c.gain| < 1)
    (hs : ∀ n, s (n + 1) = step c (s n) ⟨u n⟩) (N : ℕ) :
    (∑ n ∈ Finset.range N, (s (n + 1)).y ^ 2) + energy c (s N) =
      (∑ n ∈ Finset.range N, u n ^ 2) + energy c (s 0) := by
  induction N with
  | zero => simp
  | succ N ih =>
    rw [Finset.sum_range_succ, Finset.sum_range_succ]
    have h := step_energy_normalized c (s N) ⟨u N⟩ hg
    rw [← hs N] at h
    dsimp only at h
    linarith

end DSP.SchroederAllPass
