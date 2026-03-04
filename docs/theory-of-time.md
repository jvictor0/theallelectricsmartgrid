# Theory of Time

The **Theory of Time** is the global clock that every other part of the system follows. It is implemented in `private/src/TheoryOfTime.hpp` and used by the [Nonagon](index/README.md#major-components) (sequencer).

---

## Overview

- The system maintains a **global phase**: a value in [0, 1), or equivalently a point on the circle **S¹** (represented as **R/Z**).
- In algebraic topology terms: the total phasor is a map **R → S¹** from wall-clock time to the sequencer’s main loop.
- Time can advance at a uniform rate or can be **phase-modulated**. There is both a **true phase** (unmodulated) and a **phase-modulated phase**; the latter is what defines “where we are in time” for the rest of the system.

---

## Micro block and rolling buffer

For every micro block (8 samples), the Theory of Time computes all samples in that block **plus** the first sample of the next block. Arrays are sized `x_microBlockBufferSize = 9`; slot 8 holds the first sample of the next micro block (computed in the current block). At the start of each micro block, `RolloverMicroblockBuffer` copies slot 8 into slot 0—the first sample of this block was computed in the previous block. The Nonagon then runs at that sample, and `Process(j)` for j=1..8 computes the rest of this block and the first sample of the next. This ensures that interpolation anywhere inside a micro block always has accurate boundary samples.

---

## True phase and phase-modulated phase

- **True phase** comes from the chosen clock source and is stored in the input phasor (`input.m_phasor`). It is advanced each control frame (e.g. internal tempo, external clock, PLL, or Tick2Phasor).
- This true phase is fed into a **phase-modulation LFO** ([PolyXFader](glossary.md#polyxfader)). The LFO is **phase-driven**: it is driven by the *unmodulated* loop phasors (`m_phasorIndependent` of each of the six time loops), so it is always synchronized with the Theory of Time.
- The LFO output is applied as a **phase offset** (`input.m_phaseOffset`). The **phase-modulated phase** is:
  - `directPhasor = input.m_phasor + input.m_phaseOffset` (then wrapped into [0, 1)).
- The **CircleTracker** `m_globalPhase` tracks this modulated phase and maintains:
  - `m_phase` — current phase in [0, 1)
  - `m_winding` — integer number of full revolutions (used for continuous unwound time and for monodromy).
- Only the **master loop** (see below) receives the modulated phase for position and gates; the **independent** (unmodulated) phase is still used where continuity is needed (e.g. display, LFO drive).

---

## Six divisions: time loops

From the main loop we compute **6 divisions**, forming a tree of maps **S¹ → S¹** given by multiplication by an integer (mod 1). Each division is a **time loop** (`TimeLoop`).

- There are **6 time loops** (`TheoryOfTimeBase::x_numLoops == 6`), stored in `m_loops[0]` … `m_loops[5]`.
- The **master loop** is the root of the tree: `m_loops[x_numLoops - 1]` (index 5). It has no parent (`GetParent()` is null).
- Each non-master loop has:
  - **Parent**: `m_parentIndex` — index of its parent loop.
  - **Winding multiplier**: `m_parentMult` — the integer “mult” for the map from parent’s S¹ to this loop’s S¹ (e.g. ×2, ×3).

For each loop we compute both **pre-** and **post-modulation** positions. Parameters (parent index and parent mult) are **only allowed to change when the parent is at zero** (i.e. when `GetParent()->m_top` is true). That way positions remain continuous when the topology or multipliers change. This is enforced in `TimeLoop::HandleInput()`.

---

## Integer positions and LCM logic

To avoid floating-point issues, **integer positions** are used so that all gates change on the same sample when they mathematically should.

- The **master loop** gets its position from the modulated phasor:
  - `m_position = floor(directPhasor * m_loopSize)`
  - `m_prevPosition` is kept for edge detection; if the jump is larger than 1 step (e.g. after a seek), it is corrected so transitions stay one-step.
- **Child loops** do not call `ProcessDirectly`; they get their position from the parent in `SetMembersFromParent()`:
  - `m_position = GetParent()->m_position % m_loopSize`
  - `m_prevPosition = GetParent()->m_prevPosition % m_loopSize`
- So integer position flows from the master down the tree.

**Loop size and LCM:**

- Each loop has a **loop size** `m_loopSize` (number of integer steps in a full cycle). Parent and child sizes must be consistent with `m_parentMult`.
- `SetLoopSizes()` computes sizes as follows:
  1. Initialize `loopSizes[i] = 1` for all loops.
  2. **Upward pass** (child → parent): for each loop that has a parent,
    - `loopSizes[parentIndex] = lcm(loopSizes[parentIndex], loopSizes[child] * m_parentMult)`.
  3. **Downward pass**: for each non-master loop,
    - `loopSizes[i] = loopSizes[parentIndex] / m_parentMult`.
  4. **Doubling**: for every loop, `SetLoopSize(2 * loopSizes[i])` is called.

So the parent’s loop size is the LCM of all (child_loopSize × child_parentMult), ensuring that when we divide by `m_parentMult` we get an integer. The factor of **2** is so that each loop has **two distinct states per cycle** (see gate, below): first half and second half of the circle.

---

## Gate: two states per loop

Each time loop has **two states** depending on whether the position lies in the first or second half of the cycle:

- **Gate**: `m_gate = (m_position < m_loopSize / 2)`.
  - So phase in **[0, 0.5)** → one state (e.g. gate “on”), **[0.5, 1)** → the other (gate “off”).
- In the notation **I = {0, 1}** for the two values of `m_gate`, we get a composition:
  - **R → S¹ → (S¹)^6 → I^6**
  - i.e. at each time you get a point on a 6-dimensional hypercube (a 6-bit pattern).
- `m_gateChanged` is true when `m_gate` changes from the previous frame.

---

## Monodromy (relative state-change count)

The Theory of Time exposes **relative monodromy numbers**: how many times the **gate** of a given loop has **changed state** (flipped) since a chosen **ancestor** loop was at zero.

- With `external == true` (as used by the arp), the recurrence counts in **half-cycles** (each gate period): the divisor is `m_loopSize / 2`, so the result is the number of gate transitions since the reset ancestor was at zero, not the number of full revolutions.
- **No reset**: When `resetIx` is **-1** (no reset loop selected), no loop has `m_index == -1`, so the recurrence never hits the “at reset” base case. At the master loop we use `GlobalWinding()` — the number of times the main phasor has wrapped since the clock started. So the returned value is the **total** number of state changes (half-cycles) of the clock loop since the Theory of Time started running. The arp’s `m_resetSelect` is initialized to -1, so “no reset” is the default.
- **API**: `TheoryOfTime::MonodromyNumber(clockIx, resetIx)` returns this count for loop `clockIx` relative to loop `resetIx` (or since start when `resetIx == -1`).
- **Use**: The arp only samples this when the clock loop’s gate changes (`m_gateChanged`). It assigns `m_arpInput.m_totalIndex[j] = MonodromyNumber(clockSelect[j], resetSelect[j])`, so the arp’s index is the number of state changes (gate flips) of the clock loop since the reset loop was at zero — or since the clock started if no reset is selected.

---

## Statelessness

A guiding principle of the clock design is **statelessness**: the sequencer and LFO state are a **pure function** of the map **R → S¹** and its six divisions. Given the same global phase and topology, the system reproduces the same time loops, gates, and monodromy. There is no separate “sequencer state” that has to be kept in sync with the clock; it is derived from the clock.

---

## Related

- [Glossary](glossary.md) — **time loop**, **master loop**, **phasor**, **CircleTracker**, **PolyXFader**, **monodromy**, **LameJuis**.
- [Documentation index](index/README.md) — overview of major components.
- `docs/tex/TheoryOfTime.tex` — mathematical treatment (maps R → S¹, S¹ → S¹, partition to I^n).

