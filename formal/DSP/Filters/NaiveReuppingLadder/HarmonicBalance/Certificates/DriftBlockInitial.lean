import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCell
import DSP.Analysis.PolarTube.DecodeEnclosure

/-! The centered replay starts on the exact curved chart, not an affine
approximation. The initial secant matrix is supplied by the mean-value
theorem with a checked Jacobian enclosure over the entire input cell. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockInitial
open LeanCert.Core DSP.IntervalArithmetic Enclosure PolarTube
open DriftBlockCellData DriftProfileCellData
open scoped Matrix
set_option maxRecDepth 100000
set_option maxHeartbeats 0

def seed : State := ![5/2,0,0,0]
def coordinates (θ : ℝ) (r : Normal) : State := ![θ,r 0,r 1,r 2]
def displacement (θ : ℝ) (r : Normal) : State := coordinates θ r-seed
def path (θ : ℝ) (r : Normal) (t : ℝ) : State := seed+t • displacement θ r
def centerPoint : State := DriftChart.inverse *ᵥ liftDecode DriftChart.profile seed
def point (θ : ℝ) (r : Normal) : State := DriftChart.inverse *ᵥ liftDecode DriftChart.profile (coordinates θ r)
def radialBox : IntervalRat := ⟨-1/500000,1/500000,by norm_num⟩
def cosineBox : IntervalRat := IntervalTrig.widen (base 0) angle
def sineBox : IntervalRat := IntervalTrig.widen (base 1) angle
def rawJacobianBox : MatrixBox := decodeJacobianBox (ranges 0) (ranges 1) radialBox cosineBox sineBox

theorem inverse_mem : ∀ i j, DriftChart.inverse i j ∈ inverseBox i j := by
  have h : ∀ i j, (inverseBox i j).lo ≤ DriftChartData.inverseRat i j ∧
      DriftChartData.inverseRat i j ≤ (inverseBox i j).hi := by decide +kernel
  exact fun i j => ⟨Rat.cast_le.mpr (h i j).1,Rat.cast_le.mpr (h i j).2⟩

theorem center_mem : ∀ i, centerPoint i ∈ center 0 i := by
  have hc : ∀ i, contains (center 0 i) (matrixVector inverseBox
      (decodeBox (pointRanges 0) (fun _ => IntervalRat.singleton 0) (base 0) (base 1)) i) = true := by decide +kernel
  have hp (j : Fin 3) : DriftChart.profile ((5/2:ℝ):Real.Angle) j ∈ pointRanges 0 j :=
    DriftProfileCell.point_ranges_mem 0 j
  have hraw := decodeBox_mem DriftChart.profile seed (pointRanges 0)
    (fun _ => IntervalRat.singleton 0) (base 0) (base 1) hp
    (by intro j; fin_cases j <;> simpa [seed] using IntervalRat.mem_singleton 0)
    DriftProfileCell.base_mem.1 DriftProfileCell.base_mem.2
  exact fun i => contains_sound (hc i) (mem_matrixVector _ _ _ _ inverse_mem hraw i)

theorem path_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (t : ℝ) (ht : t ∈ Set.Icc (0:ℝ) 1) :
    path θ r t 0 ∈ angle ∧ ∀ j : Fin 3, path θ r t j.succ ∈ radialBox := by
  have hθ' := hθ
  norm_num [angle, interval, IntervalRat.mem_def] at hθ'
  constructor
  · norm_num [path,displacement,coordinates,seed,angle,interval,IntervalRat.mem_def]
    constructor <;> nlinarith [ht.1,ht.2]
  · intro j
    have hj := abs_le.mp (hr j)
    have he : path θ r t j.succ = t*r j := by fin_cases j <;> simp [path,displacement,coordinates,seed]
    rw [he]
    norm_num [radialBox,IntervalRat.mem_def]
    constructor <;> nlinarith [ht.1,ht.2]

theorem jacobian_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (t : ℝ) (ht : t ∈ Set.Icc (0:ℝ) 1) :
    ∀ i j, (DriftChart.inverse * decodeJacobian DriftChart.profile
      (fun θ => DriftChart.profileDerivative θ) (path θ r t)) i j ∈ matrices 0 i j := by
  have hpath := path_mem θ r hθ hr t ht
  have hmid : (angle.midpoint:ℝ) = 5/2 := by norm_num [angle,interval,IntervalRat.midpoint]
  have hcos : Real.cos (path θ r t 0) ∈ cosineBox :=
    IntervalTrig.widen_mem _ _ _ _ DriftProfileCell.base_mem.1
      ((Real.abs_cos_sub_cos_le _ _).trans (by simpa only [hmid] using IntervalTrig.midpoint_distance angle _ hpath.1))
  have hsin : Real.sin (path θ r t 0) ∈ sineBox :=
    IntervalTrig.widen_mem _ _ _ _ DriftProfileCell.base_mem.2
      ((Real.abs_sin_sub_sin_le _ _).trans (by simpa only [hmid] using IntervalTrig.midpoint_distance angle _ hpath.1))
  have hj := decodeJacobianBox_mem DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
    (path θ r t) (ranges 0) (ranges 1) radialBox cosineBox sineBox
    (DriftProfileCell.profile_mem _ hpath.1) (DriftProfileCell.profileDerivative_mem _ hpath.1)
    (hpath.2 0) hcos hsin
  have hc : ∀ i j, contains (matrices 0 i j) (matrixProduct inverseBox rawJacobianBox i j) = true := by decide +kernel
  exact fun i j => contains_sound (hc i j) (mem_matrixProduct _ _ _ _ inverse_mem hj i j)

theorem initial_difference (θ : ℝ) (r : Normal) (hθ : θ ∈ angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    ∃ matrix : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, matrix i j ∈ matrices 0 i j) ∧
      point θ r-centerPoint = matrix *ᵥ displacement θ r := by
  let curve := fun t => DriftChart.inverse *ᵥ liftDecode DriftChart.profile (path θ r t)
  have hd (t : ℝ) (j : Fin 4) : HasDerivAt (fun v => path θ r v j) (displacement θ r j) t := by
    simpa only [path,Pi.add_apply,Pi.smul_apply,smul_eq_mul,id_eq,one_mul] using
      ((hasDerivAt_id t).mul_const (displacement θ r j)).const_add (seed j)
  obtain ⟨matrix,hm,he⟩ := initial_secant curve
    (fun t => DriftChart.inverse*decodeJacobian DriftChart.profile
      (fun θ => DriftChart.profileDerivative θ) (path θ r t)) (matrices 0) (displacement θ r)
    (fun t _ => hasDerivAt_matrix_decode DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
      DriftChart.inverse (path θ r) t (displacement θ r) (hd t)
      (hasDerivAt_pi.mp (DriftChart.profile_hasDerivAt (path θ r t 0))))
    (jacobian_mem θ r hθ hr)
  have h0 : curve 0 = centerPoint := by simp [curve,path,centerPoint]
  have h1 : curve 1 = point θ r := by
    have hp : path θ r 1 = coordinates θ r := by simp [path,displacement]
    simp only [curve,hp,point]
  exact ⟨matrix,hm,by simpa only [h0,h1] using he⟩

theorem displacement_bound (θ : ℝ) (r : Normal) (hθ : θ ∈ angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) : ∀ j, |displacement θ r j| ≤ (width j:ℝ) := by
  have hmid := IntervalTrig.midpoint_distance angle θ hθ
  norm_num [angle,interval,IntervalRat.midpoint,IntervalRat.width] at hmid
  intro j
  fin_cases j
  · simpa [displacement,coordinates,seed,width] using hmid
  · simpa [displacement,coordinates,seed,width] using hr 0
  · simpa [displacement,coordinates,seed,width] using hr 1
  · simpa [displacement,coordinates,seed,width] using hr 2

theorem whole_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) : ∀ i, point θ r i ∈ whole 0 i := by
  obtain ⟨matrix,hm,he⟩ := initial_difference θ r hθ hr
  have hc : ∀ i, IntervalTaylor.radiusCheck (center 0 i) (whole 0 i)
    (IntervalTaylor.firstOrderRadius (matrices 0 i) width) = true := by decide +kernel
  exact fun i => centered_row_mem _ _ _ _ (hc i) _ _ (center_mem i) _ _ (hm i)
    (displacement_bound θ r hθ hr) (congrFun he i)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockInitial
