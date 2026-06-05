# Glossary

Terms and concepts used in the Smart Grid One project. Updated as we document the architecture.

---

## Theory of Time and sequencer

- **Theory of Time** — The global clock. A map **R → S¹** (wall-clock time to main loop phase) plus six hierarchical **time loops** (divisions). Implemented in `private/src/TheoryOfTime.hpp`. See [Theory of Time](theory-of-time.md).

- **time loop** — One of six divisions of the main loop. Each has a **parent** (`m_parentIndex`), a **winding multiplier** (`m_parentMult`), an integer **position** and **loop size**, and a **gate** (first vs second half of the cycle). Together they form a tree **S¹ → (S¹)^6**.

- **master loop** — The root time loop (index `x_numLoops - 1`, i.e. 5). It has no parent; its position is driven directly by the phase-modulated phasor. All other loops derive position from their parent.

- **phasor** — Phase on the circle [0, 1), or the corresponding unwound value. "True" phasor = unmodulated clock phase; "phase-modulated" phasor = true phase + LFO-derived offset, used for position and gates.

- **CircleTracker** — Tracks a phase on S¹ and an integer winding count (`m_phase`, `m_winding`). Used for the Theory of Time's global phase so positions and monodromy stay consistent across wraps. (`private/src/CircleTracker.hpp`)

- **phase-modulation LFO** — LFO that modulates the main clock phase. It is **phase-driven**: driven by the six time loops' *independent* (unmodulated) phasors, so it stays synchronized with the Theory of Time. Implemented with **PolyXFader**.

- **PolyXFader** — Internal LFO/phasor shaper used for the Theory of Time's phase-modulation LFO. Takes phase inputs from the six time loops and produces a single modulation value. (`private/src/PolyXFader.hpp`)

- **monodromy** — For a time loop (with `external == true`), the number of times its **gate** has **changed state** (flipped) since a chosen ancestor loop was at zero — or since the clock started if **no reset** is selected (`resetIx == -1`). So it counts half-cycles / state changes, not full revolutions. Exposed as `TheoryOfTime::MonodromyNumber(clockIx, resetIx)`. The arp uses this as `m_totalIndex` when the clock loop’s gate changes; `m_resetSelect` defaults to -1 (no reset).

- **LameJuis** — Esoteric layered sequencer: maps the six Theory of Time gate bits to pitch via a **lens** (read/co-mute), a **sheaf** F^M_x(U), and an **index arp**. Each of 3 trios has one LameJuis lane; the performer assigns a lens and selects a strategy (e.g. Percentile or ClosestModOne) to pick a note from the sheaf using the arp. See [LameJuis](lamejuis.md).

---

## LameJuis

- **lens** — A 6-bit mask: bit 1 = "read" (must agree for equivalence), bit 0 = "co-mute" (ignore). Defines equivalence x ~_U y and the sheaf F^M_x(U) = { M(y) | y ~_U x }. Set per lane via "co-mutes" in the UI (`!m_coMutes[i]` = read dimension i).

- **index arp** — Arpeggiator that turns the **monodromy** (state-change count of a chosen clock loop since reset) into a **point in a range**. Uses a **rhythm** pattern (`m_rhythm`, length 8) to gate steps; **m_index** = physical step among on steps; **m_motiveIndex** = rhythm page; output = f(m_index, m_motiveIndex) scaled to [m_min, m_max]. That value is passed as `m_choiceArg` to the chosen section choice strategy (e.g. Percentile or ClosestModOne). (`IndexArp`, `NonagonIndexArp` in `private/src/IndexArp.hpp`)

- **LogicOperation** — One of 6 "simple functions" I⁶ → {0,1} that build M. For each of the 6 bits: **Muted** (ignore), **Normal** (use), **Inverted** (use inverted). The output is determined by a lookup table **m_rhs[countHigh]**: for each count of high (active, possibly inverted) bits, the performer chooses whether the operation outputs true or false. Default is `m_rhs[j] = (j % 2 == 1)` ("odds pass," equivalent to parity/Xor — a Walsh function). Output goes to one of 3 **accumulators**.

- **accumulator** — One of 3 targets for the logic operations. Has an **interval** (octave, fifth, major third, etc.) in volt-per-octave. The pitch M(x) is the sum over accumulators of (interval × number of high operations targeting that accumulator). So M(x) is a just-intonation ratio as a product of simple intervals raised to small integer exponents.

- **sheaf** — F^M_x(U) = { M(y) | y ~_U x }; the set of pitches available at time x for lens U. A section choice strategy (e.g. Percentile or ClosestModOne) selects the final note using the index-arp output as `m_choiceArg`.

- **Nonagon** — The sequencer as a whole; often used to mean the combination of Theory of Time, LameJuis, note writer, and related UI. See [The Nonagon](nonagon.md).

---

## Multi-Phasor Gate and trigger

- **Multi-Phasor Gate** — Per-voice gate and trigger logic driven by the master phasor. Decides when to emit a **trigger** (start a note) and when to turn the gate off after half a voice period. Uses **NonagonTrigLogic** to build **m_trigs** and **m_newTrigCanStart** from LameJuis and the index arp; then combines those with phasor-based timing and mute to set **m_ahdControl[i].m_trig** and **m_gate[i]**. **m_gate** is not used for envelopes (those use **AHDControl**); it is used for note-off (clear output gate, record note end) and UI display. (`MultiPhasorGateInternal` in `private/src/MultiPhasorGate.hpp`)

- **NonagonTrigLogic** — Builds the Multi-Phasor Gate input for the 9 voices: **m_trigs[i]** (trigger on pitch-changed and/or sub-trigger, per trio), **m_newTrigCanStart[i]** (running and not early-muted), **m_mute[i]**, and **interrupt** (lower-index voice can cancel a trigger). Uses **m_trigOnPitchChanged**, **m_trigOnSubTrigger**, **m_interrupt**, **m_unisonMaster**. See [Multi-Phasor Gate](multi-phasor-gate.md).

- **AHD** — Attack–Hold–Decay envelope control. **AHDControl** carries **m_trig** (start note), **m_samples** (elapsed), **m_envelopeTimeSamples** (period), **m_release**. The Multi-Phasor Gate sets these so the envelope follows the gate timing. (`private/src/AHD.hpp`)

- **PhasorBounds** — Tracks distance along the circle from a gate start to the current master phasor, scaled by a voice's **m_phasorDenominator**, so the gate can turn off at half a voice period (phase ≥ 0.5). Uses **CircleDistanceTracker**.

---

## DSP and Voices

- **SquiggleBoyVoice** — A single voice in the DSP engine containing a source machine (Dual Wave Shaping VCO, Physical Modeling, Thru, or Sample), a filter machine, an amp section, a pan section, and a sub-oscillator.
- **Dual Wave Shaping VCO** — Source machine with dual wavetable oscillators and Vector Phase Shaping (VPS). Uses a random LFO to lazily mix between two randomly generated additive wavetables, swapping them out when inaudible. Implemented in `DualWaveShapingVCO.hpp`.
- **Physical Modeling Source Machine** — Noise-excited source machine that runs white noise through sample-rate reduction and a morphable SVF, then drives `CombFilterWithOnePole` with AHD modulation. Implemented in `PhysicalModelingSource.hpp`.
- **Sample Source Machine** — Source machine that reads one per-voice `AudioBufferBank`, scans or blends across the WAV files in that directory with `SampleBankPosition`, and grain-resynthesizes playback from a Theory of Time-driven `PhasorPlayHead`. Implemented by `SampleSource` in `DualSampleSource.hpp`.
- **PhasorPlayHead** — Normalized sample read-head helper that maps a selected Theory of Time loop, start, length, and read speed into a wrapped phase used for sample playback. (`private/src/PhasorPlayHead.hpp`)
- **Vector Phase Shaping (VPS)** — Distorts the phase of a wavetable using two parameters: **v** (vertical inflection) and **d** (horizontal inflection/symmetry).
- **LOD (Level of Detail)** — Controls the harmonic complexity (`m_morphHarmonics`) of the randomly generated additive wavetables.
- **Thru Source Machine** — Routes external audio through the voice's filter and amp sections.
- **DirectoryExplorer** — Runtime tree browser for selecting sample directories from the `samples/` root. UI navigation is sent as `DirectoryExplorer::MessageType` commands through `IoTaskThread`.
- **IoTaskThread** — Background worker that executes file-system tasks (directory navigation, sample bank loading) off the audio/UI control path and applies results during acknowledgment.
- **Filter Architecture** — The separation of stateful DSP code from stateless frequency response analysis via the UIState paradigm. See [Filter Architecture](filter-architecture.md).
- **Ladder Filter Machine** — A 4-pole transistor ladder low-pass filter followed by a 4-pole SVF high-pass filter.
- **SVF Filter Machine** — A 2-pole SVF low-pass filter followed by a 2-pole SVF high-pass filter, with saturation stages.
- **Quad Panning** — Distributes the 9 voices across four channels using a single global phase oscillator (`m_panPhase`) and per-voice offsets, driven by a Lissajous LFO.
- **Lissajous LFO** — Generates 2D coordinates (X, Y) from a single phase input to position voices in the quadraphonic field.
- **Phase-Driven AHD** — Attack-Hold-Decay envelopes that progress based on the phase of the Theory of Time clock, allowing them to stretch, compress, or reverse with tempo modulation.
- **Quad Delay** — A time-warped phase vocoder delay that uses an inverse buffer (`m_writeHeadInverse`) to implement `D(F⁻¹(t + d)) = X(F⁻¹(t))`. Preserves pitch while stretching/compressing time.
- **Phase Vocoder** — Grain-based spectral time-warping/resynthesis method (`Resynthesizer`) that uses phase-delta tracking between analysis frames to preserve pitch during variable-rate playback.
- **PVDR** — Internal phase-vocoder phase relationship tracker (`Resynthesizer::PVDR`) that assigns leader/follower bin relationships and phase deltas so shifted/unison synthesis stays coherent.
- **Quad Reverb** — A quadraphonic algorithmic reverb using parallel all-pass filters for diffusion, a pre-feedback all-pass, and a modulated main delay line with internal damping and saturation.
- **Partial Machine** — Global send effect that sums its quad input to mono, tracks spectral partials with `SpectralModelGeneric`, and resynthesizes them into a quad field with frequency-dependent reduction, panning, unison, and pitch controls. See [Partial Machine](partial-machine.md).
- **FrequencyDependentParameter** — Four-lane parameter model used by the Partial Machine. With identical lane values it behaves like a scalar; with per-lane modulation, each spectral partial interpolates parameter values from its frequency, creating evolving frequency-dependent behavior.
- **Send LFOs** — `ManyGangedRandomLFO`s that generate smooth, random walks (using an `ErfTable`) to modulate effects sends and parameters. Not phase-synced to the Theory of Time.
- **Multiband Saturator** — Splits the final mixdown into 4 bands using Linkwitz-Riley crossovers, applies arctan saturation (`ProcessAndSaturate`) to each band independently to prevent intermodulation distortion, and sums them back together.
- **BankedEncoderCell** — An extended state cell that supports polyphonic modulation routing, target states for gestures, and Scene A/B crossfading.
- **Track** — A group of voices that share the same base parameter value, but may have independent modulated values (e.g., 3 trios acting as 3 tracks for the 9 voices).
- **Gesture** — A macro control mapped to a physical analog input. Interpolates a parameter between its base value and a target state (`m_gestureWeights`).
- **SceneManager** — Stores scene selection (`m_scene1`, `m_scene2`) and global scene blend (`m_blendFactor`), and emits change flags used to update scene-dependent processing. See [Scene Manager](scene-manager.md).
- **StateSaver** — Scene-aware JSON persistence layer that serializes/deserializes registered runtime state and restores active scene values. See [State Saver](state-saver.md).
- **Deep Vocoder** — Experimental spectral quantizer: analyzes incoming audio partials and snaps sequencer note targets to strong atoms using a V-shaped magnitude threshold.
- **SmartBusColor (ColorBus)** — Per-route LED color bus used by controller UIs. Launchpad/Twister paths write UI color state into `m_colorBus`, then MIDI writers emit device-specific LED updates.
- **ScopeWriter** — Lock-safe multi-voice circular signal capture buffer used by oscilloscope/analyzer/melody-roll visualizers.
- **WrldBLDRMidi** — Yaeltex Wrld.Bldr MIDI mapping and writer layer translating MIDI channels/CC/note events into routed SmartGrid messages and LED SysEx updates.
- **QuadLaunchpadTwister** — Legacy 4-launchpad + encoder integration (`TheNonagonSquiggleBoyQuadLaunchpadTwister`) with separate top-left/top-right/bottom-left/bottom-right routes plus encoder route.
- **MidiSender** — Dedicated realtime MIDI output scheduler thread with per-route output handlers and timestamped queue dispatch.
- **EncoderComponent** — JUCE widget for one encoder cell: ring/value display, mod glyph badge, and short-name segment display.
- **BasicPadGrid / EncoderGrid** — JUCE grid widgets used for pad and encoder surfaces; composed into the full UI by `WrldBuildrComponent` holder classes.
- **MessageInBus** — Timestamp-aware input queue that converts routed `BasicMidi` to `MessageIn` and dispatches visible messages to integration `Apply(...)` handlers.
- **PadUI / PadUIGrid** — Lightweight UI adapters that read pad LED colors from `SmartBusColor` and push pad press/release events into `MessageInBus`.
- **SmartBusGeneric / SmartBusColor** — Atomic grid-state transport with epoch-based change tracking used for efficient LED/state propagation.

---

*More entries will be added as we document the parameter system, LED subsystem, UI, and configuration.*
