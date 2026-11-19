import DSP.DelayBuffer.History
import DSP.Transfer.IIR
import DSP.Transfer.Bilinear

namespace DSP.DelayBuffer

variable {capacity : ℕ} {K : Type} [Field K]

theorem series_delay (u : ℕ → K) (n : ℕ) :
    IIR.series (IIR.delay n u) = PowerSeries.X ^ n * IIR.series u := by
  ext t
  simpa [IIR.series] using (IIR.coeff_delay (1 : K) n t u).symm

theorem ser_delay (u : ℕ → K) (n : ℕ) :
    Bilinear.ser (IIR.delay n u) = Bilinear.delay ^ n * Bilinear.ser u := by
  have h := congrArg Bilinear.embed (series_delay u n)
  simpa [Bilinear.ser, IIR.series, Bilinear.delay] using h

/-- Every reader inherits the same q^N transfer, without a filter-specific
proof about wraparound. No convergence assumption is needed for these series. -/
theorem read_series (s : ℕ → DelayBuffer capacity K) (u : ℕ → K)
    (h0 : (s 0).history = fun _ => 0)
    (hs : ∀ n, s (n + 1) = (s n).push (u n)) (age : Fin capacity) :
    Bilinear.ser (fun n => (s n).read age) =
      Bilinear.delay ^ (age.val + 1) * Bilinear.ser u := by
  simp_rw [read_trajectory s u h0 hs]
  exact ser_delay u _

/-- A geometric history is a delay mode. Multiplying each history sample by z
is precisely what a push does when q*z=1 and the new input has amplitude x. -/
theorem history_mode (s : DelayBuffer capacity K) (q z x : K) (hq : q * z = 1)
    (hs : ∀ i, s.read i = q ^ (i.val + 1) * x) :
    ∀ i, (s.push x).read i = z * s.read i := by
  intro i
  by_cases hi : i.val = 0
  · have he : i = ⟨0, by omega⟩ := Fin.ext hi
    rw [he, read_push_zero, hs]
    norm_num only [Fin.val_mk, Nat.zero_add, pow_one]
    calc
      x = (q * z) * x := by rw [hq, one_mul]
      _ = z * (q * x) := by ring
  · rw [read_push_succ s x i (by omega), hs, hs]
    have he : i.val - 1 + 1 = i.val := by omega
    simp only [he, pow_succ]
    linear_combination -(q ^ i.val * x) * hq

end DSP.DelayBuffer
