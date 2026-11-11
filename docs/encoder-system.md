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
`GetValue(param, voice)` maps voice 0–8 to
`trio = voice / 3` and local channel `voice % 3`. Quad and Global reads use their
channel directly. UI and MIDI consume local channels starting at zero.

A base value and gesture target are shared by the voices of one cell. Modulation
sources and computed outputs vary per local voice. Each trio owns independent
modulation depth and gesture subtrees, so editing or resetting Water does not
edit Fire or Earth. Whole-patch reset, scene copy, and gesture deletion traverse
the named owner array across all trios, including hidden parameters.

## Base Encoders vs. Banked Encoders

- `StateEncoderCell`: The base class that holds the actual numerical state. It stores a per-scene target `m_targets[scene]` and current value `m_values[scene]` in the normalized range `[0, 1]`, and publishes the scene blend of the current values through one state pointer. Scene operations must read stored values, independently of a derived cell's modulated UI getter.
- `BankedEncoderCell`: Extends the state cell to support deep, polyphonic modulation and macro gestures. 

Every state encoder has an `m_bipolar` flag. Scene storage, gesture targets,
computed outputs, and UI/MIDI knob coordinates stay in `[0, 1]`.
The DSP `GetValue` getter automatically returns `2u - 1` for a
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
- Every parameter retains fifteen modulation depth positions for allocation, connection, and persistence. Skins do not determine source availability or gate processing.
- `ForEachSmartGridOneModulator.hpp` defines typed source skins as `F(enumType, enumValue, mode, color, glyph, shortName)`. Voice skins are shared by Water, Fire, and Earth; missing skins are Off/None. Global defines only random sources at positions 2 and 3 and noise at 14, so positions 6, 7, and 11 have no skin. Existing stored depths at those positions remain intact.
- Every page group's independent random pair uses RND1/RND2 and the Berry/Lime colors at local slots 2/3. All populated local slots follow the [modulator palette](#modulator-palette) below.
- Mixer audio modulators use a configurable trailing scope window (100 ms by default). Mixer envelope modulators use four VU needles reading the corresponding follower’s accepted RMS history, independently of the existing level/peak/saturation meters.
- Sub and Voice modulation views reuse the existing Sub and PostAmp scopes, normalized on read. Sub capture precedes all volume gains; Voice modulation precedes track gain, matching PostAmp. `ForEachSourceModulationVisualizer.hpp` assigns source views by the owning trio's source machine: Dual waveshaping reuses its VCO scopes; Physical Modeling uses the existing noise view and a separate internal-envelope history; Sample/Thru have no source views.
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

### Modulator palette

The fifteen local modulation positions have a fixed, authored color convention.
These are **group-local slots**, not the numeric values of source enums. Each
source's color is assigned explicitly in `ForEachSmartGridOneModulator.hpp`;
`ForEachSmartGridOneModulatorGroupSlot.hpp` binds those sources to slots. There is
no runtime index-to-color formula. Every populated occurrence of a slot uses the
same color, including each page's independent random pair. Unassigned slots stay
Off/None.

| Slot | Voice source | Quad source, where assigned | Color | sRGB |
| --- | --- | --- | --- | --- |
| 0 | Source output 1 | Dry envelope | Coral red | `#FF0047` |
| 1 | Source output 2 | Dry audio | Cyan | `#00DBFF` |
| 2 | RND1 | RND1 | Berry | `#C2257F` |
| 3 | RND2 | RND2 | Lime | `#76C70E` |
| 4 | Modulation envelope | Wet envelope | Gold | `#FFC628` |
| 5 | Amp envelope | Wet audio | Violet | `#8D5FFF` |
| 6 | LFO1 | LFO1 | Fuchsia | `#FF12D2` |
| 7 | LFO2 | LFO2 | Olive | `#556A00` |
| 8 | SHF1 | — | Ocean | `#19F8BA` |
| 9 | SHF2 | — | Indigo | `#383DF9` |
| 10 | SHF3 | — | SeaGreen | `#00FD1C` |
| 11 | Spread | Spread | Grey | `#8F8F8F` |
| 12 | Sub | Output envelope | Deep red | `#B70000` |
| 13 | Voice | Output audio | Pale lilac | `#FCC4FF` |
| 14 | Noise | Noise | White | `#FDFDFD` |

Global uses the same RND1/RND2 and Noise colors at slots 2/3/14. Quad dry/wet
sources belong to their Delay, Reverb, or Partial Machine group; Quad LFO has no
dry/wet sources. All Quad groups share final-output sources at 12/13.

Functional pairs are **0/1, 2/3, 4/5, 6/7, and 12/13**. Sheafy remains a trio;
Spread stays grey and Noise stays white. Randoms use Berry/Lime and the main
envelopes use Gold/Violet, incorporating the final pair swap. Named constants
live in `SmartGridOneModulatorColors.hpp`, alongside the existing neutral colors
in `Color.hpp`.

This palette was selected by an offline search using Euclidean distance in
[Oklab](https://bottosson.github.io/posts/oklab/), balancing separation across the
palette with strong separation within each functional pair. The weakest pair
distance rises from approximately 0.266 to 0.437 (64%). The thirteen chromatic
colors have distinct Twister hue codes; their closest HSV hue spacing is about
14.2°, rather than equal 27.7° steps. Lightness and saturation also contribute to
separation. This is a constrained design, not a proof of a universal optimum or
a guarantee of identical appearance on different hardware.

Sheafy is anchored to the original Nonagon cell colors: Ocean and SeaGreen move
by less than 0.025 Oklab distance, and Indigo is unchanged. SHF1 is **Ocean**.
`TheNonagonSmartGrid::EquationColor` uses the same definitions as the source
catalogue. Corresponding equation output, matrix, RHS, and interval cells use
these colors, deriving their dim variants from the shared values.

Parameter encoders belonging to a published source use `GetModulatorColor` so
their rings and source visualizations follow the same palette:

| Parameter family | Source/color |
| --- | --- |
| Voice and Quad LFO1/LFO2 controls | LFO1 Fuchsia / LFO2 Olive |
| Mod AHD attack, hold, decay, amplitude | Modulation envelope Gold |
| Amp AHD attack, hold, decay, amplitude | Amp envelope Violet |
| VCO1 harmonics, V, D, incoming phase modulation | Source output 1 Coral red |
| VCO2 harmonics, V, D, incoming phase modulation, pitch offset | Source output 2 Cyan |
| Physical excitation SVF cutoff, resonance, morph | Source output 1 Coral red |
| Physical excitation AHD and amplitude | Source output 2 Cyan |
| Sub volume, drive, envelope rise/fall, shape | Sub Deep red |

Tempo LFO belongs to the clock and has no published modulation source. Shared
source, filter, effect, and mixer controls retain their category colors.

### Modulator badges

Badges identify source roles independently of their fixed palette colors. See the
[rendered size comparison](images/modulator-badges.png) for the new and existing
symbols at 16/24/32 px:

| Source | Glyph |
| --- | --- |
| Sub | One saturated sine cycle, rising first |
| Dual waveshaping source outputs 1/2 | Shared compact additive waveform |
| Physical modeling source output 1 | Existing Noise |
| Physical modeling source output 2 | Existing envelope |
| Voice and main-output audio | Rounded square open at upper right, arrow from center pointing out |
| Dry/send audio | Rounded square open at upper left, arrow pointing inward |
| Wet/return audio | Three horizontal waves |
| All send, return, and main-output envelope followers | VU arc, ticks, and needle |
| Modulation and amp envelope generators | Existing envelope |

The additive badge uses three cosine components:
`cos(θ) + 0.30 cos(2θ) - 0.28 cos(3θ)`. This keeps the waveform recognizable at
small badge sizes without a dense collection of ripples. The phase convention
comes from `RandomWaveTable::GenerateWaveTable` and `FillHarmonicsBandLimited`:
the fundamental and harmonic FFT coefficients are real, and
`DiscreteFourierTransformGeneric::InverseTransform` mirrors the coefficients
conjugately and takes the real output. They therefore generate cosine
harmonics. `WriteDistortedHarmonic` uses imaginary coefficients but is not called
by the current generator. The VCO subsequently phase-warps and negates the
wavetable; the badge is a fixed source-family symbol, not a live waveform.

`ModulatorGlyphPaths.hpp` defines the six new symbols as rounded JUCE paths.
`ModulatorGlyphImages` renders them once into white-on-transparent cached
images, alongside the original seven PNG glyphs. The existing badge painter
selects white or black foreground for the palette background. Existing glyph
IDs, including `None`, keep their numeric values.

Source-pair glyphs resolve through `SmartGridOneModulators::GetSkin` with the
current machines of all three trios. `SquiggleBoyWithEncoderBank::PopulateUIState`
supplies those machines, and each glyph uses the owning source's trio, even when
the visible encoder belongs to a different trio than the navigation selection.
Switching machines updates the glyph on the next UI publication. Sample and
Thru currently produce zero for both source modulation outputs, so their source
pair uses `None`; source colors, routes, and saved depths remain intact.

The native badge test can export a comparison with the existing glyphs, including
16/24/32 px examples, by setting `SMARTGRID_BADGE_PREVIEW` to an absolute PNG path
when running `make visualizers-test`.

## Adjusting all modulation depths from a parent encoder

Hold **Shift** and turn a parameter encoder to adjust its existing modulation
depths together, including depths nested beneath them. The parameter's own base
and gesture targets stay unchanged. This works from the normal bank page and
from the selected parameter at the bottom-right of a modulation page. Shift-turn
on a depth encoder applies the same operation to that depth's descendants.

| Action | Result for each edited depth value |
| --- | --- |
| Shift + counterclockwise | Move toward zero, stopping exactly at zero |
| Shift + clockwise | Move away from zero on its existing positive or negative side, clamping at full depth |
| Either direction at zero | Stay at zero |

Zero is signed depth `0`, stored as normalized `0.5`. The operation changes
magnitude without crossing polarity, and does not create missing modulation
routes. Once a depth reaches zero, edit that depth normally to establish a new
positive or negative amount. Ordinary turns still edit the addressed value and
can cross zero. Absolute encoder-set messages also retain their normal behavior
while Shift is held. Both legacy accelerated turns and WRLD exponential-relative
MIDI turns support Shift.

Only the current parameter tree and active scenes are edited. At scene blend
`t`, the two stored values receive `delta * (1 - t)` and `delta * t`, each moving
relative to its own sign and clamping independently. A scene reaching zero or
full depth does not transfer its unused motion to the other scene. Inactive
scenes and other parameters or trios retain their values.

Gesture editing follows the normal live-weight distribution at each depth:
selected gestures are captured there, and the turn is split between the depth's
base and its active gesture targets. With a single gesture at full weight, only
its target moves; releasing it can therefore restore the unchanged base depth.
This does not rewrite every stored gesture target regardless of its weight.

The recursive edit refreshes modulation dependencies and computed outputs but
does not collect cells while the modulation page may still reference them.
Neutral depths remain connected and editable until normal deselection cleanup.
Regression coverage lives in `private/test/unit/encoder_bipolar.cpp` and
`private/test/unit/encoder_exponential.cpp` under `encoder shift-turn:*`.

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

## UI State and Parameter Smoothing

`EncoderBankBank::Process` runs every engine sample, before `SetEncoderParameters`. The modulator manager advances each declared audio-rate source's second-order Butterworth `DownsamplerGeneric` once, at ratio `SampleTimer::x_oversample`. Ganged random, sheaf, spread, and noise sources bypass filtering. On host frames it gathers source values into the fixed 15-slot groups before change detection and `ProcessHostFrame`. Source values may leave their nominal `[0,1]` range; they are not clamped. Source amplitudes are not decimated. The matrix detects changed source values, amplitudes and gestures, handles scene flags, and evaluates the named owner array, including hidden parameters, through `EncoderMatrix`.

Every voice group shares its track's source pair in slots 0/1, sub in slot 12, and voice in slot 13. The source pair is the two individual VPS outputs in Dual mode, or pre-envelope filtered noise and the internal AHD output in Physical Modeling mode; Sample/Thru clear both. Sub is after its envelope and shaper but before all volume gains; Voice is the amp-stage output before track gain. Bipolar audio maps to `0.5 + 0.5*x` without clipping. These taps are published from the preceding engine sample and downsampled by the manager, preserving causal feedback.

The mixer owns seven independent quad `EnvelopeFollowerGeneric<4>` instances: three full effect sends after feedback mixing, three effect returns before return gain/saturation, and the mastered output before recording headroom gain, output decimation, and monitor volume. Each slews squared audio at engine rate with a default 10 ms time constant and writes separate audio/envelope histories. The seven envelopes publish the square root of the smoothed power once per host frame and bypass additional downsampling. The seven audio sources publish `0.5 + 0.5*x` through manager downsampling. Both retain causal publication and preserve excursions without clipping. Each Quad effect group assigns send envelope/audio to 0/1 and wet envelope/audio to 4/5. All four Quad groups share output envelope/audio at 12/13; QuadLFO leaves 0/1/4/5 empty. Voice and Global group assignments remain unchanged.

The evaluator retains the editable encoder tree and a prepared schedule. Settled simple routes keep scalar depths in compact records; nested modulation and gestures use a child-before-parent graph. Active target ramps, invalidated revisions and roots not yet eligible for preparation use recursive `Compute(true)`. Settled outputs can be reused while their relevant inputs remain unchanged. Prepared evaluation leaves derived child state deferred until an edit, UI publication or rebuild needs `SynchronizePreparedState`; that operation does not advance ramps or replace the root's output history. Each group saves its evaluated source values and amplitudes after matrix processing. Gathering the next frame preserves that snapshot until evaluation completes, so child reconstruction cannot consume startup or future-frame source data. Direct host-frame replay uses the same commit boundary.

Each host pass clamps every top-level parameter's normalized output to `[0,1]` in `EncoderMatrix::Publish`, then publishes `x_oversample` output caches. Child encoder outputs are not clamped. A changed output is interpolated from the preceding clamped `m_outputPrev` to the clamped `m_output`, reaching the latest value on the last engine sample of the host frame; an unchanged output fills every slot with its latest value. Bipolar conversion follows interpolation. This preserves source excursions when the final parameter has headroom. `SmartGridOneEncoders::Process` selects the current slot, and its DSP-facing `GetValue` overloads read that cache. The cell-level `GetValue(channel)` helper computes the equivalent interpolation when read directly. At ratio 1 both return the latest output. Matrix calculations and UI publication use latest values without this output interpolation; visible child state is synchronized before UI publication.

Edits change per-scene targets through `SetTarget`. Manual scene values use a fixed 20 ms one-pole at the 48 kHz host rate, with double accumulator state and a normalized residual threshold of `1e-6`. On reaching the threshold, the value lands exactly on its target and its moving-scene bit clears. Retargeting preserves the accumulator and coefficient, including bursts and reversals. A 20 ms time constant reaches 63.2% in 20 ms and 99% in about 92 ms; a full-scale change settles in about 277 ms. Live modulation, gesture weights, scene blending and output interpolation remain outside this filter.

`TickDistributor::x_maxSamplesToDistribute` is now a conservative 15001-frame convergence budget, also checked in Lean. It does not require processing for that long: only moving scenes execute pole arithmetic, and a cell retires after its own scenes and relevant descendants settle. Its final value is computed and published before the dependency mask refresh and return to prepared evaluation. Target changes and edit/reset callers still force reevaluation through ancestors, preserving nested edits with stationary sources. A settled scene-index or endpoint change needs one fresh evaluation. Interior blend motion updates prepared values without rebuilding topology.

Blend and fader messages set `SettlingSlew` targets. `ProcessInputSlews()` advances them every engine sample through a 500 Hz one-pole low-pass and stops once they settle. Restoring saved blend/fader values snaps the corresponding slew.

Patch JSON stores targets. Loading reuses modulation and gesture cells at matching serialized indices, preserving current values and ramp timing while applying the loaded targets. Newly added paths start at their constructor neutral and ramp; paths absent from the patch are removed. Identical target comparison happens in the saved value domain so bipolar float round-trip rounding does not restart a ramp. An identical settled reload therefore preserves the output; matching paths in a different patch ramp from their current values. Changed topology or activation flags may change the output immediately. The [patch-load model](../formal/Encoder/README.md#patch-load-coverage) proves the logical continuity guarantee and retains the old replacement behavior as a counterexample.

## Frequency-Dependent Quad Parameters

The Partial Machine uses a Quad bank differently from Delay and Reverb. Its four lanes are interpreted as anchors for `FrequencyDependentParameter`, not just four speaker channels. If all four lanes share the same base value, the parameter behaves like a normal scalar. If modulation produces different values per lane, each spectral partial interpolates a unique value from its frequency, allowing the same knob to create frequency-dependent attack, decay, density, bandwidth, panning, unison, pitch, and volume behavior.

See [Partial Machine](partial-machine.md) for the DSP-side mapping.

## Parameter Catalogues and Machine Pages

The encoder layout has three separate catalogues:

- `ForEachSmartGridOneParam.hpp` defines each parameter once as `F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar)`. This row determines its identifier, ownership mode, value metadata, and persisted name. It has no grid coordinate or machine mask.
- `ForEachSmartGridOnePage.hpp` places parameters on named 4×4 pages as `F(page, param, x, y)`. A parameter can appear on several alternative pages, such as `FilterDrive` on the Dual VCO, Thru, Physical Modeling, and Sample source pages. Each page has at most one parameter per coordinate.
- `ForEachSmartGridOneSlot.hpp` routes a logical bank slot and typed machine value to a page and selector color as `F(slot, enumtype, enumval, page, color)`. Source uses `VoiceMachine::SourceMachine`; fixed slots use `SingletonMachine::Unit`. The PanningAndSequencing slot, for example, routes to the `PanAndMelody` page.

`SmartGridOneLayout::BuildPageAndSlotTables` validates references, page coordinates, typed route coverage, and one logical owner slot per parameter at compile time. It derives page and slot instance offsets, the three Voice replicas, slot modes, direct machine-value route tables, and each parameter's owning slot. The 20 `SmartGridOneEncoders::Bank` values come from those offsets, with compile-time assertions preserving their existing numbers. Parameter cells remain in `EncoderBankBank`'s named owner array; each is assigned its derived owner bank even when its page is hidden. Page alternatives contain layout only, not additional parameter cells.

When a source machine changes, `UpdateEncodersForMachine()` uses the precomputed affected-slot list and the machine enum value to select the route for that trio. `BindPage()` updates the route color, then changes the live 4×4 grid only if its page differs. A page change deselects any open modulation view, places sixteen cell pointers or nulls, and refreshes gesture and topology state. An unchanged page keeps its existing cells. The update does not scan the parameter catalogue or rebuild unrelated singleton slots. The selected trio's source slot changes independently of the other trios. Visible-bank reset still visits only the currently placed cells, while named-parameter processing and whole-patch operations include hidden cells.

`VoiceMachine::SourceMachineFlags` in `VoiceMachineEnums.hpp` is separate from this page routing and remains available to source-specific visualizers.

## Related
- [DSP Overview](dsp-overview.md)
- [Theory of Time](theory-of-time.md)

The [control-smoothing validation report](encoder-control-smoothing-validation.md) records
the Lean-first checkpoint, native regressions, recorded-event audio replay and CPU comparisons.
