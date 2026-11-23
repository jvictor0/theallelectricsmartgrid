import DSP.Analysis.Coboundary
import DSP.ResponseMeasurement

/-! Block measurements are exact regroupings of samples. This supplies the
bridge from a correction for the stroboscopic map to coherent and total-power
measurements, without a continuous-time averaging approximation. -/

noncomputable section
namespace DSP.Coboundary
open Finset
variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

def blockAverage (signal : ℕ → E) (length block : ℕ) : E :=
  average (fun j => signal (block*length+j)) length

omit [NormedSpace ℝ E] in
theorem sum_blocks (signal : ℕ → E) (length count : ℕ) :
    ∑ n ∈ range (count*length), signal n =
      ∑ block ∈ range count, ∑ j ∈ range length, signal (block*length+j) := by
  induction count with
  | zero => simp
  | succ count ih => rw [Nat.succ_mul, sum_range_add, sum_range_succ, ih]

theorem average_blocks (signal : ℕ → E) (length count : ℕ) :
    average (blockAverage signal length) count = average signal (count*length) := by
  simp only [average, blockAverage, ← smul_sum, smul_smul, ← sum_blocks, Nat.cast_mul]
  rw [mul_inv]

theorem average_split (signal : ℕ → E) (c : E) (n r : ℕ) (hn : 0 < n) :
    average signal (n+r)-c =
      ((n:ℝ)/(n+r:ℕ)) • (average signal n-c)+
      ((n+r:ℕ):ℝ)⁻¹ • ∑ j ∈ range r, (signal (n+j)-c) := by
  have hn0 : (n:ℝ) ≠ 0 := by positivity
  rw [average_sub_const signal c (n+r) (by omega), sum_range_add, smul_add,
    average_sub_const signal c n hn, smul_smul]
  have he : ((n:ℝ)/(n+r:ℕ))*(n:ℝ)⁻¹ = ((n+r:ℕ):ℝ)⁻¹ := by field_simp
  rw [he]

/-- An incomplete final block has a vanishing influence on bounded signals. -/
theorem average_tail_bound (signal : ℕ → E) (c : E) (bound : ℝ)
    (hb : ∀ k, ‖signal k‖ ≤ bound) (n r : ℕ) (hn : 0 < n) :
    ‖average signal (n+r)-c‖ ≤ ‖average signal n-c‖+
      (r:ℝ)*(bound+‖c‖)/(n+r:ℕ) := by
  have htotal : (0:ℝ) < (n+r:ℕ) := by positivity
  have hratio : (n:ℝ)/(n+r:ℕ) ≤ 1 := by
    apply (div_le_one htotal).mpr
    exact_mod_cast Nat.le_add_right n r
  have hs : ‖∑ j ∈ range r, (signal (n+j)-c)‖ ≤ (r:ℝ)*(bound+‖c‖) := by
    calc
      _ ≤ ∑ j ∈ range r, ‖signal (n+j)-c‖ := norm_sum_le _ _
      _ ≤ ∑ _j ∈ range r, (bound+‖c‖) := sum_le_sum (fun j _ =>
        (norm_sub_le _ _).trans (add_le_add (hb (n+j)) le_rfl))
      _ = _ := by simp [mul_add]
  rw [average_split signal c n r hn]
  calc
    _ ≤ ‖((n:ℝ)/(n+r:ℕ)) • (average signal n-c)‖+
        ‖((n+r:ℕ):ℝ)⁻¹ • ∑ j ∈ range r, (signal (n+j)-c)‖ := norm_add_le _ _
    _ = ((n:ℝ)/(n+r:ℕ))*‖average signal n-c‖+
        ((n+r:ℕ):ℝ)⁻¹*‖∑ j ∈ range r, (signal (n+j)-c)‖ := by
      rw [norm_smul, norm_smul, Real.norm_of_nonneg (by positivity), Real.norm_of_nonneg (by positivity)]
    _ ≤ 1*‖average signal n-c‖+((n+r:ℕ):ℝ)⁻¹*((r:ℝ)*(bound+‖c‖)) := by gcongr
    _ = _ := by ring

/-- A certified block-average error plus an explicit last-block correction
bounds every observation window, without assuming convergence. -/
theorem window_bound (signal : ℕ → E) (c : E) (bound error : ℝ)
    (hb : ∀ k, ‖signal k‖ ≤ bound) (length count remainder : ℕ)
    (hl : 0 < length) (hc : 0 < count)
    (he : ‖average (blockAverage signal length) count-c‖ ≤ error) :
    ‖average signal (count*length+remainder)-c‖ ≤ error+
      (remainder:ℝ)*(bound+‖c‖)/(count*length+remainder:ℕ) := by
  rw [average_blocks] at he
  exact (average_tail_bound signal c bound hb (count*length) remainder (Nat.mul_pos hc hl)).trans
    (by linarith)

/-- An eventual block-error envelope remains valid for arbitrary sample
windows. This conclusion is weaker than convergence and does not smuggle it
in as an assumption. -/
theorem eventually_bound_of_blocks (signal : ℕ → E) (length : ℕ) (hl : 0 < length)
    (c : E) (bound error : ℝ) (hb : ∀ n, ‖signal n‖ ≤ bound)
    (he : ∀ tolerance > 0, ∀ᶠ N in Filter.atTop,
      ‖average (blockAverage signal length) N-c‖ < error+tolerance) :
    ∀ tolerance > 0, ∀ᶠ n in Filter.atTop, ‖average signal n-c‖ < error+tolerance := by
  intro tolerance ht
  have hdiv : Filter.Tendsto (fun n : ℕ => n/length) Filter.atTop Filter.atTop := by
    apply Filter.tendsto_atTop.mpr
    intro N
    filter_upwards [Filter.eventually_ge_atTop (N*length)] with n hn
    exact (Nat.le_div_iff_mul_le hl).mpr hn
  have htail : Filter.Tendsto (fun n : ℕ => (length:ℝ)*(bound+‖c‖)/(n:ℝ))
      Filter.atTop (nhds 0) := by
    simpa only [div_eq_mul_inv, mul_zero] using
      (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).const_mul ((length:ℝ)*(bound+‖c‖))
  filter_upwards [hdiv.eventually (he (tolerance/2) (by linarith)),
    htail.eventually (gt_mem_nhds (show (0:ℝ) < tolerance/2 by linarith)),
    Filter.eventually_ge_atTop length] with n hprefix hlast hn
  have hq : 0 < n/length := Nat.div_pos hn hl
  have hid : (n/length)*length+n%length = n := by
    simpa only [Nat.add_comm, Nat.mul_comm] using Nat.mod_add_div n length
  have h := window_bound signal c bound (error+tolerance/2) hb length (n/length) (n%length) hl hq hprefix.le
  rw [hid] at h
  have hle : (n%length:ℕ)*(bound+‖c‖)/(n:ℝ) ≤ (length:ℝ)*(bound+‖c‖)/(n:ℝ) := by
    gcongr
    · exact add_nonneg ((norm_nonneg (signal 0)).trans (hb 0)) (norm_nonneg c)
    · exact_mod_cast (Nat.mod_lt n hl).le
  exact h.trans_lt (by linarith)

/-- Convergence at block boundaries gives convergence for every window length
when individual samples are bounded. This closes the finite-block/time-limit
interface without assuming whole-period observation windows. -/
theorem tendsto_of_blocks (signal : ℕ → E) (length : ℕ) (hLength : 0 < length)
    (bound : ℝ) (hb : ∀ n, ‖signal n‖ ≤ bound) (c : E)
    (ht : Filter.Tendsto (average (blockAverage signal length)) Filter.atTop (nhds c)) :
    Filter.Tendsto (average signal) Filter.atTop (nhds c) := by
  have hdiv : Filter.Tendsto (fun n : ℕ => n/length) Filter.atTop Filter.atTop := by
    apply Filter.tendsto_atTop.mpr
    intro N
    filter_upwards [Filter.eventually_ge_atTop (N*length)] with n hn
    exact (Nat.le_div_iff_mul_le hLength).mpr hn
  have hprefix : Filter.Tendsto (fun n => ‖average signal ((n/length)*length)-c‖)
      Filter.atTop (nhds 0) := by
    have h := ((ht.comp hdiv).sub_const c).norm
    simpa only [Function.comp_def, average_blocks, sub_self, norm_zero] using h
  have htail : Filter.Tendsto (fun n : ℕ => (length:ℝ)*(bound+‖c‖)/(n:ℝ))
      Filter.atTop (nhds 0) := by
    simpa only [div_eq_mul_inv, mul_zero] using
      (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).const_mul ((length:ℝ)*(bound+‖c‖))
  rw [tendsto_iff_dist_tendsto_zero]
  simp only [dist_eq_norm]
  apply squeeze_zero' (Filter.Eventually.of_forall (fun _ => norm_nonneg _)) _
    (by simpa only [zero_add] using hprefix.add htail)
  filter_upwards [Filter.eventually_ge_atTop length] with n hn
  have hq : 0 < n/length := Nat.div_pos hn hLength
  have he : (n/length)*length+n%length = n := by
    simpa only [Nat.add_comm, Nat.mul_comm] using Nat.mod_add_div n length
  have h := average_tail_bound signal c bound hb ((n/length)*length) (n%length) (Nat.mul_pos hq hLength)
  rw [he] at h
  apply h.trans
  gcongr
  · exact add_nonneg ((norm_nonneg (signal 0)).trans (hb 0)) (norm_nonneg c)
  · exact_mod_cast (Nat.mod_lt n hLength).le

/-- The complex signal whose ordinary average is the coherent peak phasor. -/
def demodulated (z : ℂ) (signal : ℕ → ℝ) (n : ℕ) : ℂ := 2*(signal n:ℂ)*star z^n

theorem coherent_eq_average (z : ℂ) (signal : ℕ → ℝ) (N : ℕ) :
    DSP.ResponseMeasurement.coherent z signal N = average (demodulated z signal) N := by
  simp only [DSP.ResponseMeasurement.coherent, DSP.ResponseMeasurement.average, average,
    demodulated, Complex.real_smul, mul_assoc, ← mul_sum]
  ring

theorem coherent_block (z : ℂ) (signal : ℕ → ℝ) (length block : ℕ) (hz : z^length = 1) :
    blockAverage (demodulated z signal) length block =
      DSP.ResponseMeasurement.coherent z (fun j => signal (block*length+j)) length := by
  rw [coherent_eq_average]
  unfold blockAverage
  congr 1
  funext j
  simp only [demodulated, pow_add, mul_comm block length, pow_mul, ← star_pow, hz, star_one,
    one_pow, one_mul]

theorem coherent_blocks (z : ℂ) (signal : ℕ → ℝ) (length count : ℕ) (hz : z^length = 1) :
    DSP.ResponseMeasurement.coherent z signal (count*length) =
      average (fun block => DSP.ResponseMeasurement.coherent z
        (fun j => signal (block*length+j)) length) count := by
  rw [coherent_eq_average, ← average_blocks]
  congr 1
  funext block
  exact coherent_block z signal length block hz

theorem meanSquare_blocks (signal : ℕ → ℝ) (length count : ℕ) :
    DSP.ResponseMeasurement.meanSquare signal (count*length) =
      average (fun block => DSP.ResponseMeasurement.meanSquare
        (fun j => signal (block*length+j)) length) count := by
  have h := average_blocks (fun n => signal n^2) length count
  simpa only [average, blockAverage, DSP.ResponseMeasurement.meanSquare, smul_eq_mul] using h.symm

end DSP.Coboundary
