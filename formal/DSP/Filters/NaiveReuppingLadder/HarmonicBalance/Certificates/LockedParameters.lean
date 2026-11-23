import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
import Mathlib.Analysis.Real.Pi.Bounds

/-! Exact stimulus and coefficient bounds for one locked-response fixture.
The decimal intervals are proposals, validated by rational checks and proved
Taylor remainders. No floating-point result is used as a hypothesis. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedParameters
open LeanCert.Core DSP.IntervalArithmetic Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0

noncomputable def delta : ℝ := (3/85)*Real.pi
noncomputable def forcing (n : ℕ) : Sample ℝ := ⟨Real.cos ((n:ℝ)*delta)/4⟩

structure Parameters (s : NaiveReuppingLadder ℝ) : Prop where
  alpha : s.alpha = 2*Real.sin (Real.pi/192)*
    (Real.sqrt (1+Real.sin (Real.pi/192)^2)-Real.sin (Real.pi/192))
  feedback : s.baseFeedback = (39/10)*(7*Real.sqrt 7-1)/48
  gain : s.saturation.gain = 5
  normalization : s.saturation.tanhGain = Real.tanh 5
  damping : s.dampingScale = 0

def piBox : IntervalRat := ⟨3.14159265358979323846, 3.14159265358979323847, by norm_num⟩
def cutoffSinBox : IntervalRat := ⟨0.01636173162648678164295, 0.01636173162648678164302, by norm_num⟩
def cutoffRootBox : IntervalRat := ⟨1.00013384417377716184699, 1.00013384417377716184700, by norm_num⟩
def sqrtSevenBox : IntervalRat := ⟨2.645751311064590590501615, 2.645751311064590590501616, by norm_num⟩
def tanhFiveBox : IntervalRat := ⟨0.99990920426259513121089, 0.99990920426259513121110, by norm_num⟩

theorem pi_mem : Real.pi ∈ piBox := ⟨le_of_lt Real.pi_gt_d20, le_of_lt Real.pi_lt_d20⟩

theorem cutoffSin_mem : Real.sin (Real.pi/192) ∈ cutoffSinBox := by
  have h := IntervalRat.mem_sinComputable (IntervalRat.mem_scale (1/192) pi_mem) 16
  have hc : contains cutoffSinBox (IntervalRat.sinComputable (IntervalRat.scale (1/192) piBox) 16) = true := by decide +kernel
  convert contains_sound hc h using 1 ; norm_num ; congr 1 ; ring

theorem cutoffRoot_mem : Real.sqrt (1+Real.sin (Real.pi/192)^2) ∈ cutoffRootBox := by
  have hc : sqrtCheck ((IntervalRat.singleton 1).add (cutoffSinBox.mul cutoffSinBox)) cutoffRootBox = true := by decide +kernel
  apply sqrtCheck_sound _ _ hc
  simpa only [pow_two, Rat.cast_one] using
    IntervalRat.mem_add (IntervalRat.mem_singleton 1) (IntervalRat.mem_mul cutoffSin_mem cutoffSin_mem)

theorem alpha_mem : 2*Real.sin (Real.pi/192)*
    (Real.sqrt (1+Real.sin (Real.pi/192)^2)-Real.sin (Real.pi/192)) ∈ coefficients.alpha := by
  have hc : contains coefficients.alpha (IntervalRat.scale 2
    (cutoffSinBox.mul (cutoffRootBox.sub cutoffSinBox))) = true := by decide +kernel
  have h := contains_sound hc (IntervalRat.mem_scale 2
    (IntervalRat.mem_mul cutoffSin_mem (IntervalRat.mem_sub cutoffRoot_mem cutoffSin_mem)))
  convert h using 1 ; norm_num ; ring

theorem feedback_mem : (39/10)*(7*Real.sqrt 7-1)/48 ∈ coefficients.feedback := by
  have hs : Real.sqrt 7 ∈ sqrtSevenBox := sqrtCheck_sound (IntervalRat.singleton 7) sqrtSevenBox
    (by decide +kernel) 7 (by simpa using IntervalRat.mem_singleton 7)
  have hc : contains coefficients.feedback (IntervalRat.scale (39/480)
    ((IntervalRat.scale 7 sqrtSevenBox).sub (IntervalRat.singleton 1))) = true := by decide +kernel
  have h := contains_sound hc (IntervalRat.mem_scale (39/480)
    (IntervalRat.mem_sub (IntervalRat.mem_scale 7 hs) (IntervalRat.mem_singleton 1)))
  convert h using 1 ; norm_num ; ring

theorem inverseNormalization_mem : (Real.tanh 5)⁻¹ ∈ coefficients.inverseNormalization := by
  have ht : Real.tanh 5 ∈ tanhFiveBox := DSP.IntervalTanh.check_sound _ _ 24
    (show DSP.IntervalTanh.check (IntervalRat.singleton 5) tanhFiveBox 24 = true by decide +kernel)
    5 (by simpa using IntervalRat.mem_singleton 5)
  exact inverseCheck_sound _ _ (by decide +kernel) _ ht

theorem valid (s : NaiveReuppingLadder ℝ) (h : Parameters s) : coefficients.Valid s := by
  have hb : 1-s.alpha ∈ coefficients.beta := by
    have hc : contains coefficients.beta ((IntervalRat.singleton 1).sub coefficients.alpha) = true := by decide +kernel
    exact contains_sound hc (IntervalRat.mem_sub (by simpa using IntervalRat.mem_singleton 1) (h.alpha ▸ alpha_mem))
  exact ⟨h.alpha ▸ alpha_mem, hb, h.feedback ▸ feedback_mem,
    by rw [h.gain]; norm_num [IntervalRat.mem_def, coefficients, interval],
    h.normalization ▸ inverseNormalization_mem⟩

theorem cosDelta_mem : Real.cos delta ∈ cosDelta := by
  have hc : contains cosDelta (IntervalRat.cosComputable (IntervalRat.scale (3/85) piBox) 18) = true := by decide +kernel
  convert contains_sound hc (IntervalRat.mem_cosComputable (IntervalRat.mem_scale (3/85) pi_mem) 18) using 1 ; norm_num [delta]

theorem sinDelta_mem : Real.sin delta ∈ sinDelta := by
  have hc : contains sinDelta (IntervalRat.sinComputable (IntervalRat.scale (3/85) piBox) 18) = true := by decide +kernel
  convert contains_sound hc (IntervalRat.mem_sinComputable (IntervalRat.mem_scale (3/85) pi_mem) 18) using 1 ; norm_num [delta]

private theorem carrierChecks : ∀ n : Fin 169,
    contains (carrier (n.val+1) 0)
      (((carrier n.val 0).mul cosDelta).sub ((carrier n.val 1).mul sinDelta)) = true ∧
    contains (carrier (n.val+1) 1)
      (((carrier n.val 1).mul cosDelta).add ((carrier n.val 0).mul sinDelta)) = true := by decide +kernel

theorem carrier_mem (n : ℕ) (hn : n < 170) :
    Real.cos ((n:ℝ)*delta) ∈ carrier n 0 ∧ Real.sin ((n:ℝ)*delta) ∈ carrier n 1 := by
  induction n with
  | zero => norm_num [carrier, carrierData, interval, IntervalRat.mem_def]
  | succ n ih =>
    obtain ⟨hcos, hsin⟩ := ih (by omega)
    obtain ⟨hc, hs⟩ := carrierChecks ⟨n, by omega⟩
    have he : ((n+1:ℕ):ℝ)*delta = (n:ℝ)*delta+delta := by push_cast; ring
    rw [he, Real.cos_add, Real.sin_add]
    exact ⟨contains_sound hc (IntervalRat.mem_sub (IntervalRat.mem_mul hcos cosDelta_mem)
      (IntervalRat.mem_mul hsin sinDelta_mem)),
      contains_sound hs (IntervalRat.mem_add (IntervalRat.mem_mul hsin cosDelta_mem)
        (IntervalRat.mem_mul hcos sinDelta_mem))⟩

private theorem inputChecks : ∀ n : Fin 170,
    contains (LockedData.input n.val) (IntervalRat.scale (1/4) (carrier n.val 0)) = true := by decide +kernel

theorem forcing_mem (n : ℕ) (hn : n < 170) : (forcing n).x ∈ LockedData.input n := by
  have h := contains_sound (inputChecks ⟨n,hn⟩) (IntervalRat.mem_scale (1/4) (carrier_mem n hn).1)
  convert h using 1
  norm_num [forcing]
  ring

theorem forcing_periodic : Function.Periodic forcing 170 := by
  intro n
  change Sample.mk _ = Sample.mk _
  congr 1
  have he : ((n+170:ℕ):ℝ)*delta = (n:ℝ)*delta+3*(2*Real.pi) := by push_cast; dsimp [delta]; ring
  rw [he]
  exact congrArg (fun x : ℝ => x/4) (Real.cos_add_nat_mul_two_pi ((n:ℝ)*delta) 3)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedParameters
