import PhaseTime.History
import Mathlib.Algebra.Order.Floor.Ring
import Mathlib.Data.Rat.Floor

namespace PhaseTime

-- @cpp periodic_axis: known cycle origin and period; no inferred start history.
def cycleStart (now origin period : Rat) : Rat :=
  origin + (Int.floor ((now - origin) / period) : Rat) * period

theorem cycle_start_bounds (now origin period : Rat) (hp : 0 < period) :
    cycleStart now origin period ≤ now ∧ now < cycleStart now origin period + period := by
  have hlo := (le_div_iff₀ hp).mp (Int.floor_le ((now - origin) / period))
  have hhi := (div_lt_iff₀ hp).mp (Int.lt_floor_add_one ((now - origin) / period))
  dsimp [cycleStart]
  constructor <;> nlinarith

def periodicPoint (now origin period x : Rat) : Rat :=
  let q := cycleStart now origin period + x * period
  if q ≤ now then q else q - period

theorem periodic_point_bounds (now origin period x : Rat) (hp : 0 < period)
    (hx : 0 ≤ x ∧ x < 1) :
    now - period < periodicPoint now origin period x ∧ periodicPoint now origin period x ≤ now := by
  have hb := cycle_start_bounds now origin period hp
  have hxp : 0 ≤ x * period := mul_nonneg hx.1 (le_of_lt hp)
  have hxl : x * period < period := by nlinarith [hx.2]
  dsimp only [periodicPoint]
  split <;> constructor <;> linarith

theorem periodic_point_phase (now origin period x : Rat) :
    periodicPoint now origin period x = cycleStart now origin period + x * period ∨
    periodicPoint now origin period x = cycleStart now origin period + x * period - period := by
  dsimp only [periodicPoint]; split <;> simp_all

theorem periodic_waveform (L : Rat → Rat) (now origin period x : Rat)
    (hL : ∀ p, L (p - period) = L p) :
    L (periodicPoint now origin period x) = L (cycleStart now origin period + x * period) := by
  dsimp only [periodicPoint]; split
  · rfl
  · exact hL _

-- @cpp read_head: wrap a projected phase into one selected loop behind the write head.
def wrapBehind (write projected period gap : Rat) : Rat :=
  let top := write - gap
  projected - (Int.floor ((projected - top) / period) + 1 : Int) * period

theorem read_head_bounds (write projected period gap : Rat) (hp : 0 < period) :
    write - gap - period ≤ wrapBehind write projected period gap ∧
    wrapBehind write projected period gap < write - gap := by
  have hlo := (le_div_iff₀ hp).mp (Int.floor_le ((projected - (write - gap)) / period))
  have hhi := (div_lt_iff₀ hp).mp (Int.lt_floor_add_one ((projected - (write - gap)) / period))
  simp only [wrapBehind, Int.cast_add, Int.cast_one]
  constructor <;> nlinarith

theorem read_head_behind (write projected period gap : Rat) (hp : 0 < period) (hg : 0 ≤ gap) :
    wrapBehind write projected period gap < write := by
  have := (read_head_bounds write projected period gap hp).2; linarith

def readHead (write speed delay period gap : Rat) : Rat :=
  wrapBehind write (write * speed - delay) period gap

theorem read_head_restart_continuous (s : Clock) (musical speed delay period gap : Rat) :
    readHead (s.resume musical).phase speed delay period gap =
    readHead s.phase speed delay period gap := rfl

end PhaseTime
