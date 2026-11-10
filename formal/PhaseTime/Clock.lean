import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring

namespace PhaseTime

/-! Coordinates are exact master cycles, time is exact engine-sample time.
Transport rebasing is distinct from genuine backwards phase travel. -/
structure Clock where
  phase : Rat
  musical : Rat
  offset : Rat
  running : Bool
  deriving Inhabited, DecidableEq

def Clock.Valid (s : Clock) : Prop := s.phase = s.musical + s.offset

-- @cpp clock_init: PhaseClock initialization.
def initialClock : Clock := ⟨0, 0, 0, false⟩

-- @cpp clock_run: running transport follows the modulated musical clock.
def Clock.advance (s : Clock) (musical : Rat) : Clock :=
  ⟨musical + s.offset, musical, s.offset, true⟩

-- @cpp clock_rebase: a transport start changes the origin, not the tape position.
def Clock.resume (s : Clock) (musical : Rat) : Clock :=
  ⟨s.phase, musical, s.phase - musical, true⟩

-- @cpp clock_free: stopped transport continues advancing the delay clock.
def Clock.freeRun (s : Clock) (increment : Rat) : Clock :=
  ⟨s.phase + increment, 0, s.phase + increment, false⟩

theorem initial_valid : initialClock.Valid := by simp [Clock.Valid, initialClock]
theorem advance_valid (s : Clock) (p : Rat) : (s.advance p).Valid := by rfl
theorem resume_valid (s : Clock) (p : Rat) : (s.resume p).Valid := by
  dsimp [Clock.Valid, Clock.resume]; ring
theorem free_valid (s : Clock) (d : Rat) : (s.freeRun d).Valid := by
  simp [Clock.Valid, Clock.freeRun]
theorem resume_continuous (s : Clock) (p : Rat) : (s.resume p).phase = s.phase := rfl
theorem advance_travel (s : Clock) (p : Rat) (h : s.Valid) :
    (s.advance p).phase - s.phase = p - s.musical := by
  dsimp [Clock.advance]; rw [h]; ring
theorem advance_offset (s : Clock) (p : Rat) : (s.advance p).offset = s.offset := rfl
theorem free_travel (s : Clock) (d : Rat) : (s.freeRun d).phase - s.phase = d := by
  dsimp [Clock.freeRun]; ring
theorem stopped_progress (s : Clock) (d : Rat) (hd : 0 < d) : s.phase < (s.freeRun d).phase := by
  dsimp [Clock.freeRun]; linarith
theorem restart_then_advance (s : Clock) (p d : Rat) :
    ((s.resume p).advance (p + d)).phase = s.phase + d := by
  dsimp [Clock.resume, Clock.advance]; ring
theorem genuine_rewind (s : Clock) (p : Rat) (hs : s.Valid) (hp : p < s.musical) :
    (s.advance p).phase < s.phase := by
  have := advance_travel s p hs; linarith

-- Tempo controls travel, never a multiplier on recorded phase coordinates.
theorem tempo_changes_only_travel (s : Clock) (d₁ d₂ : Rat) :
    (s.advance (s.musical + d₂)).phase - (s.advance (s.musical + d₁)).phase = d₂ - d₁ := by
  dsimp [Clock.advance]; ring

end PhaseTime
