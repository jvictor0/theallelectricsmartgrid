# The Encoder System

The Smart Grid One relies entirely on a software-defined parameter system. All "knob" state lives in software, allowing for complete recall, deep modulation, macro control (gestures), and A/B scene morphing.

The core implementation spans `private/src/Encoder.hpp`, `private/src/EncoderBank.hpp`, and `private/src/EncoderBankBank.hpp`.

Ownership lives in `EncoderBankBank`: it owns a flat array of `BankedEncoderCell` instances indexed by `EncoderIndex(param, trio) = param * 3 + trio`. Voice parameters have separate Water, Fire, and Earth cells; Quad and Global parameters use only the trio-zero slot. Each `EncoderBankInternal` starts with null base cells and receives raw pointers through `PlaceEncoder(...)`. This makes encoder swapping explicit and keeps base cells empty until they are placed.

Serialization follows ownership: `EncoderBankBank::ToJSON()` iterates the full encoder array and writes each named parameter, and `FromJSON()` does the inverse by looking up each name and updating the encoder state.

## Construction

The shared `SmartGridOneContext` is a constructor dependency throughout the
encoder ownership chain. It provides the scene manager, recorder, and parameter
event logger:

- `EncoderBankBank(numBanks, numModes, numEncoders, context)` stores the context used by every `CreateEncoder(...)` call.
- `SmartGridOneEncoders(context)` initializes the fixed three-trio, three-voices-per-trio layout during construction.
- `SquiggleBoyWithEncoderBank(context)` constructs that encoder system for the Nonagon's trio/voice layout.

The context must outlive these objects. Encoder cells serialize through
`EncoderBankBank`; the discrete-control `State`/`StateSaver` storage stays separate.

## Parameter recording

`SetAndRecordValue` updates a stored scene value in one named cell and emits EncoderSet while
recording. The event converts normalized storage through `ToValue`, matching
ordinary patch JSON before smoothing or modulation. `SetActive` records gesture
activation; when activation inherits a parent's value, that copied value also
passes through `SetAndRecordValue`. Voice root names include a Water, Fire, or
Earth suffix; Quad and Global names stay unsuffixed. Each nested path hop identifies a gesture or modulator index.

The recorder groups these assignments with StateChange, GestureSet (fader), and
BlendSet events. Format-v5 recording replay uses flat scene arrays and activation
flags, including
neutral nested nodes first created after the header snapshot. Encoder events no
longer carry a track field; the root name identifies the trio. FromJSON writes
raw values and activation flags; one PatchLoad event captures the whole load,
including replacement/removal of children. Gestures are always leaves; normal
modulators may nest or have gesture leaves. See the exact
[recording protocol and limitations](streaming-recording-format.md).

## Base Structure: Trios and Voices

Tracks remain a performer-facing choice, but encoder cells no longer contain a
track dimension. Each cell stores eight scene values and one banked base value.
Its bank mode determines the number of independent output channels:

- Water, Fire, and Earth each have their own Voice mode with three local channels.
- Source, Filter and Amp, Panning and Sequencing, and Voice LFOs each have three
  bank instances, one per trio: twelve Voice banks altogether.
- Delay, Reverb, Partial Machine, and Quad LFOs share the four-channel Quad mode.
- Theory of Time, Mastering, Inputs, and Deep Vocoder share the one-channel Global mode.

There are twenty bank instances and five modes. `SetTrack(trio)` selects the
corresponding instance of the current Voice-bank family. It remembers the trio
while a Quad or Global bank is selected, without changing that selected bank.
`GetValue(param, voice)` and `GetValueNoSlew(param, voice)` map voice 0–8 to
`trio = voice / 3` and local channel `voice % 3`. Quad and Global reads use their
channel directly. UI and MIDI consume local channels starting at zero.

A base value and gesture target are shared by the voices of one cell. Modulation
sources and computed outputs vary per local voice. Each trio owns independent
modulation depth and gesture subtrees, so editing or resetting Water does not
edit Fire or Earth. Whole-patch reset, scene copy, and gesture deletion traverse
the named owner array across all trios, including hidden parameters.

## Base Encoders vs. Banked Encoders

- `StateEncoderCell`: The base class that holds the actual numerical state. It stores `m_values[scene]` in the normalized range `[0, 1]` and publishes the scene blend through one state pointer. Scene operations must read stored values, independently of a derived cell's modulated UI getter.
- `BankedEncoderCell`: Extends the state cell to support deep, polyphonic modulation and macro gestures. 

Every state encoder has an `m_bipolar` flag. Scene storage, gesture targets,
computed outputs, slew state, and UI/MIDI knob coordinates stay in `[0, 1]`.
DSP `GetValue` and `GetValueNoSlew` getters automatically return `2u - 1` for a
bipolar encoder and `u` for a unipolar encoder. Switch indexing still uses the
normalized position. The parameter declaration's trailing `bipolar` flag and
default value configure this behavior; defaults are in the exposed knob domain
and are converted to normalized state at creation. Existing top-level parameters
remain unipolar. Gesture target cells inherit their parent's polarity.

JSON saves the **knob position**, in `[-1, 1]` for bipolar encoders and `[0, 1]`
for unipolar encoders. Loading converts signed values back to normalized storage.
Polarity comes from the parameter declaration or child role, not a saved field.
A named cell saves `values.values` as eight numbers; a gesture also saves `active`
as eight booleans. There is no inner track array.

Patch JSON has a top-level integer `version`. Current saves, including recording
snapshots, write version 1; a missing version means version 0. On the message
thread, `StateInterchange::ParseForLoad` upgrades version 0 in memory before
publishing the tree to audio. Opening a legacy patch does not rewrite its disk
file; the upgraded form is written by a normal save. It splits each legacy
Voice root into Water, Fire,
and Earth roots. For each scene, trio indexes 0, 1, and 2 select the old
scene-by-track value; gesture activation selects `active[scene * 16 + trio]`.
The mapping recurses through modulators and gestures, retaining null slots and
signed patch values. Shared parameters retain their names and select track zero.
Missing scene or track values become zero, and missing activation flags become
false. Missing fields stay absent for partial loads. Unknown root fields and
encoder names pass through unchanged. Already suffixed Voice roots stay intact,
and already scalar shared values or eight-entry shared activation arrays are
preserved during the transition. Explicit version 0 follows the same
upgrade; version 1 is used unchanged. Negative, future, noninteger, and null
versions are rejected, as are duplicate top-level `version` or `squiggleBoy`
keys and legacy Voice roots that collide with a suffixed root. Recording container format version 5 is independent of patch version.

## Deep Modulation

Every `BankedEncoderCell` can act as a modulation destination.
- Up to 15 internal "LFOs" or envelopes (e.g., the PolyXFader LFOs, AHD envelopes) can be routed to any parameter.
- The **Modulation Depth** is itself implemented as a full `BankedEncoderCell`. This means you can modulate the modulation depth (e.g., using an LFO to slowly fade in an envelope's effect on the filter cutoff).
- Depth cells are bipolar, with normalized position `0.5` meaning no modulation.
  Their signed knob position `b` maps to effective depth
  `sign(b) * (9^abs(b) - 1) / 8`. Signed positions `-1, -0.5, 0, 0.5, 1`
  therefore produce depths `-1, -0.25, 0, 0.25, 1`. The parent applies this curve
  after the child's recursive normalized computation, once per modulation route.
- **Polyphonic Modulation**: While the base knob value is shared across a track, the modulation sources (like voice-specific envelopes) are polyphonic. Therefore, the resulting modulated parameter values are calculated independently for every voice.
- **Crossfade Modulation**: To prevent clipping and dead spots, modulation is applied as a crossfade. A modulation depth of `1.0` means the parameter is 100% controlled by the modulator, completely overriding the base knob value.

Negative depths crossfade toward the inverted source, `1 - m`. For each voice,
let `d` be each curved depth multiplied by its source amplitude and `W = sum(abs(d))`.
The output is `p * max(0, 1 - W) + (sum(d * m) + sum(max(0, -d))) / max(1, W)`.
The negative-depth offset keeps the result normalized, while total absolute
weight above one produces a normalized modulation mix. Source amplitude changes
invalidate the cached result even when the waveform value stays constant.

Depth reset, activity detection, and garbage collection use the neutral position
`0.5`. Non-neutral saved scenes and active nested modulation remain preserved,
even when the current scene blend produces zero depth. Bipolar encoder rings
show a center marker; encoder-set messages and MIDI values remain normalized.

Saving writes the signed knob position, so repeated save/load must not reapply
the exponential depth curve. Changing a top-level parameter to bipolar also
requires updating its DSP callers to consume the signed domain.

## Gestures (Macros)

**Gestures** are macro controls mapped to physical analog inputs (Like sliders and joysticks).
- A gesture allows you to define a "Target State" for a parameter.
- When the gesture is at `0.0`, the parameter sits at its base knob value.
- When the gesture is at `1.0`, the parameter moves to the Target State.
- Like modulation depths, the Target State is implemented as a leaf `BankedEncoderCell` (hidden from the normal UI), with one target and activation flag per scene for its owning trio.
- If you physically turn an encoder while a gesture is active, the system correctly updates the base knob or the Target State to match your physical action, ensuring the UI and physical knobs never fall out of sync.

## Scene Morphing

The entire state of all encoders (base values, modulation depths, and gesture targets) is stored across **8 persistent Scenes** (`SceneManager::x_numScenes`, in `SceneManager.hpp`). Each scene is a complete snapshot of the synthesizer.
- At any moment two of the eight scenes are *active*: `m_scene1` and `m_scene2`. A global `m_blendFactor` crossfades between just those two.
- `GetSceneValue` reads each parameter's per-scene array and interpolates `values[m_scene1]` and `values[m_scene2]` by `m_blendFactor`.
- Because every parameter is continuously interpolating between the two active scenes, moving the scene crossfader smoothly morphs every aspect of the sound engine simultaneously.

## UI State and Parameter Slew

- `EncoderBankUIState`: Manages the communication between the deep software state and the physical hardware/screen UI. It handles the rendering of LED rings and the processing of delta increments from physical endless encoders.
- **Parameter Slew**: We only calculate the parameter values every eight samples on control frames, the DSP engine applies a slew filter (`ParamSlew` or `FixedSlew`) to the final, post-modulation parameter values before using them in audio calculations. During oversampled blocks, this slew rate is adjusted automatically.

## Frequency-Dependent Quad Parameters

The Partial Machine uses a Quad bank differently from Delay and Reverb. Its four lanes are interpreted as anchors for `FrequencyDependentParameter`, not just four speaker channels. If all four lanes share the same base value, the parameter behaves like a normal scalar. If modulation produces different values per lane, each spectral partial interpolates a unique value from its frequency, allowing the same knob to create frequency-dependent attack, decay, density, bandwidth, panning, unison, pitch, and volume behavior.

See [Partial Machine](partial-machine.md) for the DSP-side mapping.

## Machine-Specific Parameters

Each parameter in the Voice banks can specify which source and filter machines it applies to via bit vectors (`MachineFlags`). Parameters are defined in `ForEachSmartGridOneParam.hpp` with `sourceMachines` and `filterMachines` arguments (e.g. `MachineFlags::x_dualWaveShapingVCOOnly` for parameters that only affect the Dual Wave Shaping VCO source).

When the user selects a different source machine (e.g. Thru) or filter machine, `UpdateEncodersForMachine()` runs and swaps the actual encoder pointers in the grid. Parameters that do not apply are removed by placing `nullptr` in those positions, which leaves the cell empty and disconnected. This keeps the encoder grid relevant to the active machine while preserving encoder state in the owner array. The update is triggered on machine change (config page) and track change (since each trio can have different machines). Only the selected trio's four Voice banks are rebuilt; the other trios retain their cells and placements.

## Related
- [DSP Overview](dsp-overview.md)
- [Theory of Time](theory-of-time.md)
