# LameJuis

**LameJuis** is an esoteric, layered sequencer that turns the six gate bits from the [Theory of Time](theory-of-time.md) into polyphonic pitch. The implementation is in `private/src/LameJuis.hpp`, `private/src/HarmonicSheaf.hpp`, and `private/src/IndexArp.hpp`; the Nonagon wires the six time-loop gates into LameJuis and uses one LameJuis **lane** per **trio** (three voices share one lane's pitch logic).

For fixed accepted configuration, the six gate bits **x** in **I⁶** determine the set of available notes. The selected note also depends on the index arp's choice argument, derived from the whole-cycle gate-step index. Modulating the Theory of Time therefore moves both the gate bits and the arp through this polyphonic process. Live loop-rhythm edits are held until the edited loop's next tick.

---

## 1. The map M and the sheaf F^M_x(U)

Let **M : I⁶ → pitch**. Here "pitch" is represented as **volt-per-octave** (or equivalently log₂ of a just-intonation ratio). Composing M with the Theory of Time would give a single melody; we want many interlocking melodies, so we introduce a **lens** and a **sheaf**.

- **Lens U** — A subset of the 6 dimensions, represented as a 6-bit bitstring (`HarmonicSheaf::Lens`, extending `HarmonicSheaf::BitVector`).  
  - **Read bits** (bits set to 1 in U): dimensions we "read"; two time slices must agree on these to be equivalent under lens U.  
  - **Co-mute bits** (bits set to 0 in U): dimensions we "co-mute" (ignore for equivalence); they can vary.  
  In the UI this is set per trio as "co-mutes": lens bit *i* = 1 when *i* is **not** co-mute, i.e. when we read dimension *i* (`Lane::CoMuteState::GetLens()` sets `lens.Set(i, !m_coMutes[i])`).
- **Equivalence** — For **x**, **y** in I⁶, define **x ~_U y** iff **x** and **y** agree on the read bits. In code: `Lens::Equivalent(a,b)` is `(a.m_bits ^ b.m_bits) & m_bits == 0`.
- **Harmonic Sheaf** — Define **F^M_x(U) = { M(y) | y ~_U x }**. So at time **x**, for a given lens U, we take all time slices equivalent to **x** under U and collect their M-values. This set is chosen statelessly from **x**; if **x** jumps (e.g. from time modulation), the set changes accordingly.

---

## 2. Trios and lens assignment

There are **9 voices** in **3 trios** of 3 voices each. Each trio is assigned **one LameJuis lane** (there are 3 lanes, one per trio). The performer assigns a **lens U** to that lane via the co-mute UI: which of the 6 dimensions are "read" vs "co-mute". At each time **x** in I⁶, the trio must pick a note from **F^M_x(U)**. That choice is made by the **index arp** (see below) and a **section choice strategy**.

---

## 3. Index arp: clock, reset, rhythm, and range

The **index arp** (`IndexArp`, used per voice inside `NonagonIndexArp`) turns the signed gate-step index of a chosen clock loop into a **point in a range** that is then used to pick a note from **F^M_x(U)**.

### 3.1 Clock and reset

- The performer chooses a **clock loop** and optionally a **reset loop** (an ancestor of the clock for the reset to be meaningful; or no reset, i.e. reset index -1).
- When `AnyTick(clockLoop)` reports a whole-cycle crossing in the microblock, the Nonagon sets `m_totalIndex` from `GetGateStepIndex(clockLoop, 0, resetLoop)`.
- Without reset, this is the signed absolute full-cycle coordinate. An ancestor reset reduces it modulo the number of clock cycles in that reset period; a self reset returns zero. Reverse motion decreases the index; a multi-cycle seek reports a crossing even if the final gate bit is unchanged. See [Theory of Time](theory-of-time.md#shared-integer-position).

### 3.2 Gate sequencer (rhythm)

This per-voice arp rhythm is separate from the per-loop Theory of Time rhythm that provides each LameJuis input bit. A loop tick clocks the arp even if that loop's gate value repeats; editing the loop rhythm does not create an extra tick.

- Each voice's arp has a **rhythm** pattern: `m_rhythm[0..m_rhythmLength-1]` with `m_rhythmLength` default 8 (`IndexArp::x_rhythmLength`). Only some steps are "on"; the rest gate the voice off.
- From **m_totalIndex** we derive:
  - **m_rhythmIndex** = `PhaseUtils::FloorMod(m_totalIndex, m_rhythmLength)` — position on the rhythm loop.
  - **m_motiveIndex** = `PhaseUtils::FloorDiv(m_totalIndex, m_rhythmLength)` — which "page" or cycle through the rhythm.
- A **trigger** happens only when `m_rhythm[m_rhythmIndex]` is true and the clock has just advanced (we're in the `m_clock` / `m_triggered` path). Then we compute **m_index**: the **physical index** among the **on** steps (0 to NumNotes()-1), i.e. how many rhythm steps that are on have been passed up to and including the current step.

### 3.3 Point in range

- The arp exposes a **range** per voice: **m_offset**, **m_interval**, **m_pageInterval**, **m_min**, **m_max**, plus **m_invert**, **m_retro**, **m_cycle**.
- **Output** is computed as  
`GetOutput(m_index, m_motiveIndex)` =  
`m_offset + m_index * m_interval + m_motiveIndex * m_pageInterval`,  
then optionally wrapped (cycle) or inverted, then scaled from [0,1] to **[m_min, m_max]**.
- So the **index** (physical step among on steps) and **motive index** (rhythm page) together determine a single float in a range. That float is passed to LameJuis as **m_choiceArg** and interpreted by the chosen strategy (e.g. percentile or closest-mod-octave).

- **When the Nonagon updates the index arp** — `AnyChangeInMicroBlock()` causes the Nonagon to refresh arp inputs and run the arp and LameJuis. The selected clock's `AnyTick` drives clock updates; no clock selection sets the total index to zero. Read updates follow crossing dimensions selected by the lens. The total and motive indices stay signed 64-bit values; bounded output mapping uses double until its final float result.

---

## 4. Section choice strategies

Once we have the set **F^M_x(U)** (all M(y) for y ~_U x), we select a note from it using a **section choice strategy** (`HarmonicSheaf::SectionChoiceStrategy`) with the index-arp output as **m_choiceArg**.

Each lane has a **strategy** (toggled in the UI) and an optional **base strategy** (defaults to `None`). The `Lane::Chooser` first runs the base strategy to get a base section value, then adds that to `m_choiceArg` and runs the main strategy. This two-stage approach allows composing strategies.

The available strategies (`HarmonicSheaf::SectionChooser`):

- **None** — Returns a zero section. Used as the default base strategy (no offset).
- **Lowest** — Returns the section with the lowest evaluated pitch in the equivalence class.
- **GCD** — Returns the component-wise minimum of all sections in the equivalence class.
- **Closest** — Finds the section whose evaluated pitch is closest to `m_choiceArg`.
- **ClosestModOne** — Like Closest, but compares pitches modulo one octave (V/O). The result is placed in the same octave as `m_choiceArg`, with ±1 octave adjustment if that is closer. This is the default strategy.
- **Percentile** — Sorts all sections in the equivalence class by pitch, then indexes into the sorted list using the fractional part of `m_choiceArg` as a percentile in [0, 1). The integer part of `m_choiceArg` is added as an octave offset.

The result is a single pitch (volt-per-octave) per voice; that pitch is then used by the rest of the synth (e.g. V/O output, possible octave shift from the UI). Whether a **trigger** is emitted (note on) for that pitch is decided by the [Multi-Phasor Gate](multi-phasor-gate.md) (pitch-changed vs sub-trigger, mutes, interrupt).

---

## 5. The map M: logic operations and accumulators

**M(x)** is not a single ratio; it is computed by a **matrix** of **logic operations** feeding **accumulators**, whose outputs are combined additively in volt-per-octave (i.e. multiplicatively as ratios).

### 5.1 Structure

- There are **6 logic operations** and **3 accumulators**. Each active operation outputs to **one** of the 3 accumulators (selected by a switch: Down/Middle/Up → target 2/1/0).
- For a given **x** in I⁶ (the 6 gate bits), each operation evaluates to **0 or 1**. The **Section** for **x** stores, for each accumulator, how many active operations target it (`m_total[acc]`) and how many of those are high (`m_high[acc]`). The **pitch** in volt-per-octave is
**pitch = Σ_acc accumulators[acc].m_intervalValue * m_high[acc]**  
So in ratio space this is a product of simple factors: each accumulator has an **interval** (e.g. octave, fifth, major third) and an **exponent** 0 or 1 (or more generally 0..m_total[acc] when several ops target the same acc). So **M(x)** is a just-intonation ratio expressed as a product of these simple intervals raised to 0/1 (or small integer) exponents.

### 5.2 Logic operations

Each **LogicOperation** (the "simple functions" in the user's description) does the following:

- **Input**: the 6-bit time slice **x** (and a fixed configuration of the operation).
- **Per-bit treatment** — For each of the 6 dimensions we have a **MatrixSwitch**: **Muted** (ignore), **Normal** (use the bit), **Inverted** (use the bit inverted). So we get an effective 6-bit vector: only "active" (non-muted) bits matter, and some are flipped. This is implemented as `m_active` (which bits are used) and `m_inverted` (which of those are inverted). `GetTotalAndHigh` does `inputVector &= m_active`, `inputVector ^= m_inverted`, then counts **countTotal** = number of active bits and **countHigh** = number of 1s in the result.
- **RHS lookup** — A row with no accepted non-muted inputs is always false, even when **m_rhs[0]** is true. Otherwise its output is **m_rhs[countHigh]**: a boolean lookup table indexed by how many of the (active, possibly inverted) bits are high. For each **k** in 0..6 the performer can choose whether the operation outputs true or false when exactly **k** bits are high.
- **Generalized Walsh** — The default is `m_rhs[j] = (j % 2 == 1)`, so **only odd** counts pass. That is parity (Xor), i.e. a Walsh function. By changing the RHS table, the performer can select which counts (0..6) pass; these behave like **generalized Walsh functions** on the 6-bit input (with the given active/inverted mask).
- **RHS grid lighting** — The RHS page (`LameJuisRHSPage`) is six operations by seven count columns. Toggling a cell still edits **m_rhs[k]**. A column **flashes** when count **k** is reachable in the **active trio's** sheaf fiber: there exists an assignment of that trio's co-muted bits which, paired with the current read (non-co-muted) bits, yields **countHigh == k**. Each co-muted active bit independently contributes 0 or 1, so the lit columns are the interval **[base, base + f]** where **base** is countHigh from the read ∩ active bits and **f** is the number of active ∩ co-muted bits. If nothing relevant is co-muted this degenerates to the single current count. Columns with **k > countTotal** stay dim (impossible from the matrix switches alone).

An operation owns its six matrix elements, active and inverted masks, RHS table, and output target. Its **m_countTotal** counts accepted non-muted **input bits**. An accumulator owns an interval; each section's **m_total[acc]** counts active **operation rows** targeting it. An empty row contributes to no accumulator, regardless of its stored target.

So **M(x)** is built from up to 6 active boolean functions; each contributes 0 or 1 to one of 3 accumulators; the accumulators have fixed intervals (octave, fifth, third, etc.); and the final pitch is the sum in V/O of (interval × exponent) per accumulator.

### 5.3 Extra Timbre Modulators

In addition to pitch, the logic matrix provides **extra timbre modulators**. For each of the 3 accumulators, the matrix computes the ratio of operations that evaluated to high versus the total number of active operations targeting that accumulator (`m_high[acc] / m_total[acc]`). An accumulator with no active rows yields zero. The Nonagon captures these three values in [0, 1] as each voice's `m_extraTimbre` when that voice triggers and holds them until its next trigger. These can be routed to DSP parameters (like filter cutoff or wavefolder depth) to provide rhythmic modulation that is perfectly synchronized with the pitch sequence.

---

## 6. Edit acceptance, cache updates, and note timing

The Nonagon supplies each input's gate value and a separate `m_ticked` flag from `AnyTick(i)`. A modulated loop crossing counts even when consecutive rhythm steps have the same gate value.

- A co-mute or matrix element for input **i** is accepted only on **i**'s tick. A faster input cannot accept a slower input's pending edit.
- A row accepts its RHS table and output target on a tick from any input that is non-muted in either its accepted or requested matrix. The row then recomputes its active-input count, high count, and output. Accumulator interval changes are accepted whenever LameJuis processes.
- When the last non-muted input's mute is accepted, the row becomes false and the sheaf rebuild removes it from all accumulator totals immediately, before that frame's lane selection. RHS and target edits while the row stays empty remain pending. A requested unmute reactivates the row only when that input ticks; that tick also accepts the pending RHS and target.
- Accepted matrix, RHS, target, or interval changes invalidate the 64-section cache. Co-mutes select a fiber through that cache. At initialization the grid's coordinate converter receives the initial lane lens, so the grid is correct even before any co-mute edit.

A channel selects a new section only on its existing read flag or arp trigger. Section equality compares both `m_high` and `m_total` for every accumulator; the selected result also compares evaluated pitch. Thus a denominator-only change can request another note at the same pitch, on the next permitted channel update. This includes zero-high sections such as 0/4 and 0/3, even though both timbre ratios are zero. Cache rebuilding and equality checks do not add reads or move them off the rhythmic grid. The existing trigger, mute, and interrupt controls still determine whether a requested note starts.

## 7. Statelessness

Because:

- with fixed rhythms, the Theory of Time gates are determined by each loop's current whole-cycle step,
- the gate-step index (and hence **m_totalIndex**) is a pure function of time when the selected clock ticks,
- the index arp maps that to a point in a range,
- the lens and M define **F^M_x(U)** purely from **x**,
- and the section choice strategy selects deterministically from **F^M_x(U)**,

the pitch-selection mapping is deterministic for a fixed accepted configuration and choice argument. Pending edits, accepted configuration, and held channel selections are stateful as described above. Live Theory of Time rhythm edits take effect only at that loop's next modulated tick, so the output gate is explicitly held between ticks. Modulating the Theory of Time (e.g. phase modulation, different clock/reset, or different topology) only changes **x** and the index over time; the logic remains consistent.

---

## Related

- [Theory of Time](theory-of-time.md) — supplies the 6 gate bits and gate-step indices.
- [Glossary](glossary.md) — **LameJuis**, **lens**, **index arp**, **LogicOperation**, **accumulator**, **sheaf**.
- [Documentation index](index/README.md) — Nonagon and trios.
