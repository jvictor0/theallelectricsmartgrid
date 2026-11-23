import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCellData
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftChart

/-! Concrete replay of the proposed profile and its first two derivatives on
the whole phase cell [2.4998,2.5002]. This exercises exact preparation of one
cell; it does not prove dynamic trapping, attraction, or measurement bounds. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCell
open LeanCert.Core DSP.IntervalArithmetic TrigPolynomial DriftProfileCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0

theorem base_mem : Real.cos (5/2:ℝ) ∈ base 0 ∧ Real.sin (5/2:ℝ) ∈ base 1 := by
  have hc : contains (base 0) (IntervalTrig.cosinePoint (5/2) 30) = true := by decide +kernel
  have hs : contains (base 1) (IntervalTrig.sinePoint (5/2) 30) = true := by decide +kernel
  constructor
  · convert contains_sound hc (IntervalTrig.cosinePoint_mem (5/2) 30) using 1
    norm_num
  · convert contains_sound hs (IntervalTrig.sinePoint_mem (5/2) 30) using 1
    norm_num

theorem carrier_checked : carrierCheck base carrier 80 = true := by decide +kernel

theorem carrier_mem (i : Fin 81) :
    Real.cos ((i.val:ℝ)*(angle.midpoint:ℝ)) ∈ carrier i.val 0 ∧
    Real.sin ((i.val:ℝ)*(angle.midpoint:ℝ)) ∈ carrier i.val 1 := by
  have hmid : (angle.midpoint:ℝ) = 5/2 := by norm_num [angle,interval,IntervalRat.midpoint]
  rw [hmid]
  exact carrierCheck_sound base carrier 80 carrier_checked (5/2) base_mem.1 base_mem.2 i.val (by omega)

theorem ranges_checked : ∀ order coordinate : Fin 3,
    contains (ranges order coordinate) ((polynomial order coordinate).harmonicEnclosure angle carrier) = true := by
  decide +kernel

theorem ranges_mem (order coordinate : Fin 3) (θ : ℝ) (hθ : θ ∈ angle) :
    (polynomial order coordinate).eval (θ:Real.Angle) ∈ ranges order coordinate :=
  contains_sound (ranges_checked order coordinate)
    ((polynomial order coordinate).mem_harmonicEnclosure angle carrier carrier_mem θ hθ)

theorem point_ranges_checked : ∀ order coordinate : Fin 3,
    contains (pointRanges order coordinate)
      ((polynomial order coordinate).harmonicEnclosure pointAngle carrier) = true := by
  decide +kernel

theorem point_ranges_mem (order coordinate : Fin 3) :
    (polynomial order coordinate).eval ((5/2:ℝ):Real.Angle) ∈ pointRanges order coordinate := by
  apply contains_sound (point_ranges_checked order coordinate)
  apply (polynomial order coordinate).mem_harmonicEnclosure pointAngle carrier
  · intro i
    convert carrier_mem i using 1 <;> norm_num [pointAngle,angle,interval,IntervalRat.midpoint,IntervalRat.singleton]
  · norm_num [pointAngle,IntervalRat.mem_def,IntervalRat.singleton]

theorem profile_mem (θ : ℝ) (hθ : θ ∈ angle) (j : Fin 3) :
    DriftChart.profile (θ:Real.Angle) j ∈ ranges 0 j := ranges_mem 0 j θ hθ

theorem profileDerivative_mem (θ : ℝ) (hθ : θ ∈ angle) (j : Fin 3) :
    DriftChart.profileDerivative (θ:Real.Angle) j ∈ ranges 1 j := ranges_mem 1 j θ hθ

theorem profileSecondDerivative_mem (θ : ℝ) (hθ : θ ∈ angle) (j : Fin 3) :
    DriftChart.profileSecondDerivative (θ:Real.Angle) j ∈ ranges 2 j := ranges_mem 2 j θ hθ

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCell
