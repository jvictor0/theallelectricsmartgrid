import DSP.Analysis.PolarTube.Encode
import DSP.Analysis.IntervalArithmetic

/-! Interval enclosures for the full output chart's first and mixed second
variations. Radius and reciprocal bounds and the Fourier derivative bounds
are explicit inputs, to be proved over the same output cell. -/

namespace DSP.PolarTube
open LeanCert.Core DSP.IntervalArithmetic
abbrev StateBox := Fin 4 → IntervalRat

def phaseFirstBox (z v : StateBox) (inverseSquare : IntervalRat) : IntervalRat :=
  (((z 0).mul (v 1)).sub ((z 1).mul (v 0))).mul inverseSquare

def radiusFirstBox (z v : StateBox) (inverseRadius : IntervalRat) : IntervalRat :=
  (((z 0).mul (v 0)).add ((z 1).mul (v 1))).mul inverseRadius

def phaseSecondBox (z l r : StateBox) (inverseSquare : IntervalRat) : IntervalRat :=
  let cross := ((l 0).mul (r 1)).add ((l 1).mul (r 0))
  let first := (((z 0).mul (z 1)).scale 2).mul ((l 0).mul (r 0))
  let second := (((z 1).mul (z 1)).sub ((z 0).mul (z 0))).mul cross
  let third := (((z 0).mul (z 1)).scale 2).mul ((l 1).mul (r 1))
  ((first.add second).sub third).mul (inverseSquare.mul inverseSquare)

def radiusSecondBox (z l r : StateBox) (inverseRadius : IntervalRat) : IntervalRat :=
  let cross := ((l 0).mul (r 1)).add ((l 1).mul (r 0))
  let first := ((z 1).mul (z 1)).mul ((l 0).mul (r 0))
  let second := ((z 0).mul (z 1)).mul cross
  let third := ((z 0).mul (z 0)).mul ((l 1).mul (r 1))
  ((first.sub second).add third).mul ((inverseRadius.mul inverseRadius).mul inverseRadius)

def encodeFirstBox (dp : Fin 3 → IntervalRat) (z v : StateBox)
    (inverseSquare inverseRadius : IntervalRat) : StateBox :=
  let dφ := phaseFirstBox z v inverseSquare
  ![dφ, (radiusFirstBox z v inverseRadius).sub ((dp 0).mul dφ),
    (v 2).sub ((dp 1).mul dφ), (v 3).sub ((dp 2).mul dφ)]

def encodeMixedBox (dp ddp : Fin 3 → IntervalRat) (z l r m : StateBox)
    (inverseSquare inverseRadius : IntervalRat) : StateBox :=
  let lφ := phaseFirstBox z l inverseSquare
  let rφ := phaseFirstBox z r inverseSquare
  let mφ := (phaseFirstBox z m inverseSquare).add (phaseSecondBox z l r inverseSquare)
  let mρ := (radiusFirstBox z m inverseRadius).add (radiusSecondBox z l r inverseRadius)
  let correction j := (((ddp j).mul lφ).mul rφ).add ((dp j).mul mφ)
  ![mφ, mρ.sub (correction 0), (m 2).sub (correction 1), (m 3).sub (correction 2)]

noncomputable section

theorem phaseFirstBox_mem (z v : State) (zb vb : StateBox) (inverseSquare : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hv : ∀ i, v i ∈ vb i)
    (hi : (z 0^2+z 1^2)⁻¹ ∈ inverseSquare) :
    (head v/head z).im ∈ phaseFirstBox zb vb inverseSquare := by
  rw [localPhase_derivative_formula]
  have h := IntervalRat.mem_mul (IntervalRat.mem_sub (IntervalRat.mem_mul (hz 0) (hv 1))
    (IntervalRat.mem_mul (hz 1) (hv 0))) hi
  convert h using 1
  · rfl
  · dsimp [head]
    ring

theorem radiusFirstBox_mem (z v : State) (zb vb : StateBox) (inverseRadius : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hv : ∀ i, v i ∈ vb i)
    (hi : ‖head z‖⁻¹ ∈ inverseRadius) :
    radiusVariation (head z) (head v) ∈ radiusFirstBox zb vb inverseRadius := by
  exact IntervalRat.mem_mul (IntervalRat.mem_add (IntervalRat.mem_mul (hz 0) (hv 0))
    (IntervalRat.mem_mul (hz 1) (hv 1))) hi

theorem phaseSecondBox_mem (z l r : State) (zb lb rb : StateBox) (inverseSquare : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hl : ∀ i, l i ∈ lb i) (hr : ∀ i, r i ∈ rb i)
    (hi : (z 0^2+z 1^2)⁻¹ ∈ inverseSquare) :
    phaseHessian (head z) (head l) (head r) ∈ phaseSecondBox zb lb rb inverseSquare := by
  have hxy := IntervalRat.mem_scale 2 (IntervalRat.mem_mul (hz 0) (hz 1))
  have h := IntervalRat.mem_mul (IntervalRat.mem_sub
    (IntervalRat.mem_add (IntervalRat.mem_mul hxy (IntervalRat.mem_mul (hl 0) (hr 0)))
      (IntervalRat.mem_mul (IntervalRat.mem_sub (IntervalRat.mem_mul (hz 1) (hz 1))
        (IntervalRat.mem_mul (hz 0) (hz 0)))
        (IntervalRat.mem_add (IntervalRat.mem_mul (hl 0) (hr 1)) (IntervalRat.mem_mul (hl 1) (hr 0)))))
    (IntervalRat.mem_mul hxy (IntervalRat.mem_mul (hl 1) (hr 1)))) (IntervalRat.mem_mul hi hi)
  convert h using 1
  · rfl
  · dsimp [phaseHessian,head]
    push_cast
    simp only [div_eq_mul_inv,← inv_pow]
    ring

theorem radiusSecondBox_mem (z l r : State) (zb lb rb : StateBox) (inverseRadius : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hl : ∀ i, l i ∈ lb i) (hr : ∀ i, r i ∈ rb i)
    (hne : head z ≠ 0) (hi : ‖head z‖⁻¹ ∈ inverseRadius) :
    radiusHessian (head z) (head l) (head r) ∈ radiusSecondBox zb lb rb inverseRadius := by
  rw [radiusHessian_formula _ _ _ hne]
  have h := IntervalRat.mem_mul (IntervalRat.mem_add
    (IntervalRat.mem_sub (IntervalRat.mem_mul (IntervalRat.mem_mul (hz 1) (hz 1))
      (IntervalRat.mem_mul (hl 0) (hr 0)))
      (IntervalRat.mem_mul (IntervalRat.mem_mul (hz 0) (hz 1))
        (IntervalRat.mem_add (IntervalRat.mem_mul (hl 0) (hr 1)) (IntervalRat.mem_mul (hl 1) (hr 0)))))
    (IntervalRat.mem_mul (IntervalRat.mem_mul (hz 0) (hz 0)) (IntervalRat.mem_mul (hl 1) (hr 1))))
    (IntervalRat.mem_mul (IntervalRat.mem_mul hi hi) hi)
  convert h using 1
  · rfl
  · dsimp [head]
    simp only [div_eq_mul_inv,← inv_pow]
    ring

theorem encodeFirstBox_mem (dp : ℝ → Normal) (anchor : ℂ) (z v : State)
    (dpb : Fin 3 → IntervalRat) (zb vb : StateBox) (inverseSquare inverseRadius : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hv : ∀ i, v i ∈ vb i)
    (hp : ∀ j, dp (localPhase anchor (head z)) j ∈ dpb j)
    (his : (z 0^2+z 1^2)⁻¹ ∈ inverseSquare) (hir : ‖head z‖⁻¹ ∈ inverseRadius) :
    ∀ i, encodeVariation dp anchor z v i ∈ encodeFirstBox dpb zb vb inverseSquare inverseRadius i := by
  have hφ := phaseFirstBox_mem z v zb vb inverseSquare hz hv his
  have hρ := radiusFirstBox_mem z v zb vb inverseRadius hz hv hir
  intro i
  fin_cases i
  · exact hφ
  · exact IntervalRat.mem_sub hρ (IntervalRat.mem_mul (hp 0) hφ)
  · exact IntervalRat.mem_sub (hv 2) (IntervalRat.mem_mul (hp 1) hφ)
  · exact IntervalRat.mem_sub (hv 3) (IntervalRat.mem_mul (hp 2) hφ)

theorem encodeMixedBox_mem (dp ddp : ℝ → Normal) (anchor : ℂ) (z l r m : State)
    (dpb ddpb : Fin 3 → IntervalRat) (zb lb rb mb : StateBox) (inverseSquare inverseRadius : IntervalRat)
    (hz : ∀ i, z i ∈ zb i) (hl : ∀ i, l i ∈ lb i) (hr : ∀ i, r i ∈ rb i) (hm : ∀ i, m i ∈ mb i)
    (hp : ∀ j, dp (localPhase anchor (head z)) j ∈ dpb j)
    (hpp : ∀ j, ddp (localPhase anchor (head z)) j ∈ ddpb j)
    (hne : head z ≠ 0) (his : (z 0^2+z 1^2)⁻¹ ∈ inverseSquare) (hir : ‖head z‖⁻¹ ∈ inverseRadius) :
    ∀ i, encodeMixedVariation dp ddp anchor z l r m i ∈
      encodeMixedBox dpb ddpb zb lb rb mb inverseSquare inverseRadius i := by
  have hlφ := phaseFirstBox_mem z l zb lb inverseSquare hz hl his
  have hrφ := phaseFirstBox_mem z r zb rb inverseSquare hz hr his
  have hmφ := IntervalRat.mem_add (phaseFirstBox_mem z m zb mb inverseSquare hz hm his)
    (phaseSecondBox_mem z l r zb lb rb inverseSquare hz hl hr his)
  have hmρ := IntervalRat.mem_add (radiusFirstBox_mem z m zb mb inverseRadius hz hm hir)
    (radiusSecondBox_mem z l r zb lb rb inverseRadius hz hl hr hne hir)
  have hc j := IntervalRat.mem_add (IntervalRat.mem_mul (IntervalRat.mem_mul (hpp j) hlφ) hrφ)
    (IntervalRat.mem_mul (hp j) hmφ)
  have he := phaseHessian_formula (head z) (head l) (head r) (head m)
  intro i
  fin_cases i
  · simpa [encodeMixedVariation,encodeMixedBox,he] using hmφ
  · simpa [encodeMixedVariation,encodeMixedBox,he] using IntervalRat.mem_sub hmρ (hc 0)
  · simpa [encodeMixedVariation,encodeMixedBox,he] using IntervalRat.mem_sub (hm 2) (hc 1)
  · simpa [encodeMixedVariation,encodeMixedBox,he] using IntervalRat.mem_sub (hm 3) (hc 2)

end
end DSP.PolarTube
