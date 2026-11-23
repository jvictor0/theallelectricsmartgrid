import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData

/-! Exact forcing for the drifting block. The physical coefficient bounds
are shared with the locked fixture; this stimulus has period 54 samples. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockParameters
open LeanCert.Core DSP.IntervalArithmetic DriftBlockCellData TrigPolynomial
set_option maxRecDepth 100000
set_option maxHeartbeats 0

def delta : ℝ := Real.pi/27
def forcing (n : ℕ) : Sample ℝ := ⟨Real.cos ((n%54:ℕ)*delta)/4⟩

theorem base_mem : Real.cos delta ∈ forcingBase 0 ∧ Real.sin delta ∈ forcingBase 1 := by
  have hi : delta ∈ IntervalTrig.pi.scale (1/27) := by
    convert IntervalRat.mem_scale (1/27) IntervalTrig.pi_mem using 1
    norm_num [delta]; ring
  have hc : contains (forcingBase 0) (IntervalTrig.cosine (IntervalTrig.pi.scale (1/27)) 30) = true := by decide +kernel
  have hs : contains (forcingBase 1) (IntervalTrig.sine (IntervalTrig.pi.scale (1/27)) 30) = true := by decide +kernel
  exact ⟨contains_sound hc (IntervalTrig.cosine_mem _ _ _ hi),
    contains_sound hs (IntervalTrig.sine_mem _ _ _ hi)⟩

theorem carriers_checked : carrierCheck forcingBase forcingCarrier 53 = true := by decide +kernel

theorem input_mem (n : ℕ) : (forcing n).x ∈ DriftBlockCellData.input n := by
  have hc : ∀ i : Fin 54, contains (DriftBlockCellData.input i.val)
      ((forcingCarrier i.val 0).scale (1/4)) = true := by decide +kernel
  have hm := (carrierCheck_sound forcingBase forcingCarrier 53 carriers_checked delta
    base_mem.1 base_mem.2 (n%54) (by omega)).1
  have h := contains_sound (hc ⟨n%54, Nat.mod_lt _ (by norm_num)⟩)
    (IntervalRat.mem_scale (1/4) hm)
  norm_num only [Rat.cast_div, Rat.cast_one, Rat.cast_ofNat] at h
  simpa only [forcing, DriftBlockCellData.input, Nat.mod_mod, div_eq_mul_inv, one_mul, mul_comm] using h

theorem forcing_eq_sinusoid (n : ℕ) : (forcing n).x = Real.cos ((n:ℝ)*delta)/4 := by
  have he : (n:ℝ)*delta = ((n%54:ℕ):ℝ)*delta+((n/54:ℕ):ℝ)*(2*Real.pi) := by
    have hn := congrArg (fun k : ℕ => (k:ℝ)) (Nat.mod_add_div n 54)
    push_cast at hn
    dsimp only [delta]
    nlinarith [congrArg (fun x : ℝ => x*Real.pi) hn]
  rw [he, Real.cos_add_nat_mul_two_pi]
  rfl

theorem forcing_periodic : Function.Periodic forcing 54 := by
  intro n
  simp only [forcing, Nat.add_mod_right]

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockParameters
