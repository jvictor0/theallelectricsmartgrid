import DSP.Filters.NaiveReuppingLadder
import DSP.IfLess.Real
import Mathlib.Analysis.InnerProductSpace.Basic
import Mathlib.Tactic

namespace DSP.NaiveReuppingLadder.NoiseResponse

private def request {R : Type} (s : NaiveReuppingLadder R) (k : R) := {s with feedback := k}

section Transfer
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R] [Tanh R]

private theorem step_request (s : NaiveReuppingLadder R) (k : R) (x : Sample R) :
    (request s k).step x = request (s.step x) k := rfl

/-- State at the nth sample boundary under fixed controls. -/
def trajectory (x : ℕ → Sample R) (s : NaiveReuppingLadder R) : ℕ → NaiveReuppingLadder R
  | 0 => s
  | n+1 => (trajectory x s n).step (x n)

private theorem run_request (x : ℕ → Sample R) (s : NaiveReuppingLadder R) (k : R) (n : ℕ) :
    trajectory x (request s k) n = request (trajectory x s n) k := by
  induction n with
  | zero => rfl
  | succ n ih => rw [trajectory,ih,step_request];rfl

variable [Trig R] [SquareRoot R]

/-- At fixed cutoff and drive, equality of the prepared base feedback is sufficient
    for equality of the full output trajectory, including both live detectors.
    This is structural and imposes no algebraic laws on the sample arithmetic. -/
theorem same_base_waveform (state : NaiveReuppingLadder R) (cut gain a b : R)
    (h : (computeCoefficients ⟨cut,a,gain⟩ state).baseFeedback =
         (computeCoefficients ⟨cut,b,gain⟩ state).baseFeedback)
    (x : ℕ → Sample R) :
    (fun n => (trajectory x (computeCoefficients ⟨cut,a,gain⟩ state).reset n).output) =
    (fun n => (trajectory x (computeCoefficients ⟨cut,b,gain⟩ state).reset n).output) := by
  have prepared : computeCoefficients ⟨cut,b,gain⟩ state =
      request (computeCoefficients ⟨cut,a,gain⟩ state) (b*3.9) := by
    dsimp only [computeCoefficients,updateFeedbackCoefficients,request] at h ⊢
    rw [h]
  rw [prepared]
  funext n
  have reset_request (z : NaiveReuppingLadder R) (k : R) :
      (request z k).reset = request z.reset k := rfl
  rw [reset_request,run_request]
  rfl

end Transfer

/-- For ordered real coefficients, two requests at or above the computed cap
    prepare the same feedback. The cap only depends on the cutoff and gain. -/
theorem capped_base [Tanh ℝ] [FMA ℝ] [Trig ℝ] [SquareRoot ℝ]
    (s : NaiveReuppingLadder ℝ) (cut gain a b : ℝ)
    (ha : (computeCoefficients ⟨cut,a,gain⟩ s).kSafe ≤ a*3.9)
    (hb : (computeCoefficients ⟨cut,a,gain⟩ s).kSafe ≤ b*3.9) :
    (computeCoefficients ⟨cut,a,gain⟩ s).baseFeedback =
    (computeCoefficients ⟨cut,b,gain⟩ s).baseFeedback := by
  let k := (computeCoefficients ⟨cut,a,gain⟩ s).kSafe
  change minValue (a*3.9) k = minValue (b*3.9) k
  have eq (u : ℝ) (h : k ≤ u) : minValue u k = k := by
    change (if k < u then k else u) = k
    split_ifs with lt
    · rfl
    · exact le_antisymm (le_of_not_gt lt) h
  rw [eq _ ha,eq _ hb]

/-- For a pair with the same physical curve, half the distance between their
    fitted curves is an unavoidable worst-case error, in particular for log-Hz RMS. -/
theorem unavoidable {E : Type*} [PseudoMetricSpace E] (a b physical : E) :
    dist a b / 2 ≤ max (dist a physical) (dist b physical) := by
  have tri := dist_triangle a physical b
  rw [dist_comm physical b] at tri
  linarith [le_max_left (dist a physical) (dist b physical),
    le_max_right (dist a physical) (dist b physical)]

/-- Replacing physically equivalent displayed curves by the midpoint of two fits
    saves at least a quarter of their squared separation in the old worst squared
    error. Apply this to the Hilbert space of visible dB on normalized log Hz. -/
theorem midpoint_error {E : Type*} [NormedAddCommGroup E] [InnerProductSpace ℝ E]
    (a b physical : E) :
    ‖(1/2 : ℝ) • (a+b)-physical‖^2 + ‖a-b‖^2/4 ≤
      max (‖a-physical‖^2) (‖b-physical‖^2) := by
  have h := parallelogram_law_with_norm ℝ (a-physical) (b-physical)
  have sub : (a-physical)-(b-physical)=a-b := by abel
  rw [sub] at h
  have half : (1/2 : ℝ) • (a+b)-physical =
      (1/2 : ℝ) • ((a-physical)+(b-physical)) := by module
  rw [half,norm_smul,Real.norm_eq_abs,abs_of_nonneg (by norm_num : 0 ≤ (1/2:ℝ))]
  linarith [le_max_left (‖a-physical‖^2) (‖b-physical‖^2),
    le_max_right (‖a-physical‖^2) (‖b-physical‖^2)]

theorem packed_cap_arithmetic :
    (2^249 < (1000:ℕ)^25) ∧ (1000:ℕ) < 2^10 ∧
    (4027/10000 : ℚ)^2 < 5533678186074059 / 34110785558937600 ∧
    (3958177/297289728 : ℚ) < (116/1000)^2 := by norm_num

end DSP.NaiveReuppingLadder.NoiseResponse
