import DSP.Filters.NaiveReuppingLadder.HarmonicBalance

/-! Fundamental-plus-third Galerkin reduction of the exact ladder equations.
Only the stage waveforms are truncated: tanh and the output retain all modes.
This describes the odd, single-phase branch, not every possible response.
The residual identities do not assert existence, attraction, or an error bound
between a numerical root and an actual trajectory. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.FundamentalThird
open DSP.Harmonics
open scoped ComplexConjugate

def mode (n : ℤ) : Unit → ℤ := fun _ => n

@[simp] theorem mode_eq (n m : ℤ) : mode n = mode m ↔ n = m := by
  constructor
  · exact fun h => congr_fun h ()
  · exact congrArg mode

@[simp] theorem neg_mode (n : ℤ) : -mode n = mode (-n) := rfl

structure Phasors where
  fundamental : ℂ
  third : ℂ

def waveform (p : Phasors) : Waveform Unit :=
  realMode p.fundamental (mode 1) + realMode p.third (mode 3)

def kept : Finset (Unit → ℤ) := {mode 1, mode (-1), mode 3, mode (-3)}

theorem waveform_coeff (p : Phasors) (k : Unit → ℤ) :
    coeff (waveform p) k =
      p.fundamental/2 * (if k = mode 1 then 1 else 0) +
      conj p.fundamental/2 * (if k = mode (-1) then 1 else 0) +
      (p.third/2 * (if k = mode 3 then 1 else 0) +
      conj p.third/2 * (if k = mode (-3) then 1 else 0)) := by
  simp only [waveform, coeff_add, coeff_realMode, neg_mode]
  rfl

@[simp] theorem waveform_first (p : Phasors) :
    coeff (waveform p) (mode 1) = p.fundamental/2 := by
  simp [waveform_coeff]

@[simp] theorem waveform_third (p : Phasors) :
    coeff (waveform p) (mode 3) = p.third/2 := by
  simp [waveform_coeff]

theorem waveform_supported (p : Phasors) (k : Unit → ℤ) (h : k ∉ kept) :
    coeff (waveform p) k = 0 := by
  simp only [kept, Finset.mem_insert, Finset.mem_singleton, not_or] at h
  simp [waveform_coeff, h.1, h.2.1, h.2.2.1, h.2.2.2]

def extract (f : Waveform Unit) : Phasors :=
  ⟨2 * coeff f (mode 1), 2 * coeff f (mode 3)⟩

def project (f : Waveform Unit) : Waveform Unit := waveform (extract f)

theorem project_coeff (f : Waveform Unit) (k : Unit → ℤ) (h : k ∈ kept) :
    coeff (project f) k = coeff f k := by
  simp only [kept, Finset.mem_insert, Finset.mem_singleton] at h
  rcases h with rfl | rfl | rfl | rfl
  · simp [project, extract]
  · rw [← neg_mode, coeff_conj, coeff_conj]
    simp [project, extract]
  · simp [project, extract]
  · rw [← neg_mode, coeff_conj, coeff_conj]
    simp [project, extract]

theorem project_waveform (p : Phasors) : project (waveform p) = waveform p := by
  have h : extract (waveform p) = p := by
    cases p
    simp [extract]
    constructor <;> ring
  exact congrArg waveform h

theorem project_idempotent (f : Waveform Unit) : project (project f) = project f :=
  project_waveform _

theorem projection_residual_energy (f : Waveform Unit) :
    phasePower (f - project f) =
      ∑' k, if k ∈ kept then 0 else ‖coeff f k‖ ^ 2 := by
  rw [omitted_residual_energy (f - project f) kept (by
    intro k hk; rw [coeff_sub, project_coeff f k hk, sub_self])]
  apply tsum_congr
  intro k
  by_cases hk : k ∈ kept
  · simp [hk]
  · simp [hk, project, waveform_supported _ _ hk]

/-- Four pairs of complex peak phasors: sixteen real unknowns at fixed omega. -/
structure StagePhasors where
  stage1 : Phasors
  stage2 : Phasors
  stage3 : Phasors
  stage4 : Phasors

def orbit (p : StagePhasors) : Orbit Unit :=
  ⟨waveform p.stage1, waveform p.stage2, waveform p.stage3, waveform p.stage4⟩

/-- The defect in a single relaxation equation; applicable to every stage. -/
def residual (α : ℝ) (ω : Phase Unit) (x v : Waveform Unit) : Waveform Unit :=
  x - ((1-α) • delay ω x + α • v)

theorem residual_coeff (α : ℝ) (ω : Phase Unit) (x v : Waveform Unit)
    (k : Unit → ℤ) :
    coeff (residual α ω x v) k =
      denominator α ω k * coeff x k - (α : ℂ) * coeff v k := by
  simp only [residual, coeff_sub, coeff_add, coeff_smul, coeff_delay,
    Complex.ofReal_sub, Complex.ofReal_one, denominator]
  ring

/-- Enforce the two positive complex equations; realness supplies negatives. -/
def Projected (α : ℝ) (ω : Phase Unit) (x v : Waveform Unit) : Prop :=
  ∀ n ∈ ({1, 3} : Finset ℤ),
    denominator α ω (mode n) * coeff x (mode n) = (α : ℂ) * coeff v (mode n)

theorem projected_iff_residual (α : ℝ) (ω : Phase Unit) (x v : Waveform Unit) :
    Projected α ω x v ↔ ∀ k ∈ kept, coeff (residual α ω x v) k = 0 := by
  constructor
  · intro h k hk
    have h1 : coeff (residual α ω x v) (mode 1) = 0 := by
      rw [residual_coeff, sub_eq_zero]; exact h 1 (by simp)
    have h3 : coeff (residual α ω x v) (mode 3) = 0 := by
      rw [residual_coeff, sub_eq_zero]; exact h 3 (by simp)
    simp only [kept, Finset.mem_insert, Finset.mem_singleton] at hk
    rcases hk with rfl | rfl | rfl | rfl
    · exact h1
    · rw [← neg_mode, coeff_conj, h1]; simp
    · exact h3
    · rw [← neg_mode, coeff_conj, h3]; simp
  · intro h n hn
    rw [← sub_eq_zero, ← residual_coeff]
    apply h
    simp only [Finset.mem_insert, Finset.mem_singleton] at hn
    rcases hn with rfl | rfl <;> simp [kept]

theorem projected_tail (α : ℝ) (ω : Phase Unit) (x v : Waveform Unit)
    (h : Projected α ω x v) :
    phasePower (residual α ω x v) =
      ∑' k, if k ∈ kept then 0 else ‖coeff (residual α ω x v) k‖ ^ 2 :=
  omitted_residual_energy _ kept ((projected_iff_residual α ω x v).mp h)

def ReducedBalance (s : NaiveReuppingLadder ℝ) (ω : Phase Unit)
    (u : Waveform Unit) (p : StagePhasors) : Prop :=
  Projected s.alpha ω (waveform p.stage1)
    (u - s.baseFeedback • delay ω (waveform p.stage4)) ∧
  Projected s.alpha ω (waveform p.stage2) (saturated s (waveform p.stage1)) ∧
  Projected s.alpha ω (waveform p.stage3) (saturated s (waveform p.stage2)) ∧
  Projected s.alpha ω (waveform p.stage4) (saturated s (waveform p.stage3))

/-- This is exactly a projection of full balance, not a new audio recurrence. -/
theorem full_implies_reduced (s : NaiveReuppingLadder ℝ) (ω : Phase Unit)
    (u : Waveform Unit) (p : StagePhasors) (h : Balance s ω u (orbit p)) :
    ReducedBalance s ω u p := by
  rcases (equations_iff_balance s ω u (orbit p)).mpr h with ⟨h1,h2,h3,h4⟩
  exact ⟨fun n _ => (onePole_equation_iff _ _ _ _).mp h1 (mode n),
    fun n _ => (onePole_equation_iff _ _ _ _).mp h2 (mode n),
    fun n _ => (onePole_equation_iff _ _ _ _).mp h3 (mode n),
    fun n _ => (onePole_equation_iff _ _ _ _).mp h4 (mode n)⟩

/-- The actual two complex stage equations in peak-phasor convention.
The nonlinear coefficient still integrates tanh of the COMPLETE H1+H3 wave. -/
theorem projected_phasors (α : ℝ) (ω : Phase Unit) (p : Phasors) (v : Waveform Unit) :
    Projected α ω (waveform p) v ↔
      denominator α ω (mode 1) * p.fundamental = (2:ℂ) * α * coeff v (mode 1) ∧
      denominator α ω (mode 3) * p.third = (2:ℂ) * α * coeff v (mode 3) := by
  simp only [Projected, Finset.mem_insert, Finset.mem_singleton, forall_eq_or_imp,
    forall_eq, waveform_first, waveform_third]
  constructor
  · rintro ⟨h1,h3⟩; constructor <;> [linear_combination 2*h1; linear_combination 2*h3]
  · rintro ⟨h1,h3⟩; constructor <;> [linear_combination h1/2; linear_combination h3/2]

end DSP.NaiveReuppingLadder.HarmonicBalance.FundamentalThird
