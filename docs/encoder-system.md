# The Encoder System

The Smart Grid One relies entirely on a software-defined parameter system. All "knob" state lives in software, allowing for complete recall, deep modulation, macro control (gestures), and A/B scene morphing.

The core implementation spans `private/src/Encoder.hpp`, `private/src/EncoderBank.hpp`, and `private/src/EncoderBankBank.hpp`.

## Base Structure: Tracks and Voices

To accommodate polyphony and quadraphonic effects, the parameter system is structured hierarchically.
- **Voices**: The synthesizer engine runs 9 simultaneous voices. For quadraphonic effects (like the Quad Delay), there are 4 independent channels (effectively acting as voices in the parameter system).
- **Tracks**: Voices are grouped into tracks. For example, the 9 voices are grouped into 3 trios. The parameter system addresses these as 3 tracks, with 3 voices per track.
- **Banks**: Parameters are grouped into Banks (e.g., "Source", "Filter", "Delay"). Each bank defines its **Bank Mode**, which dictates the number of tracks and voices per track.
  - **Voice Banks** (3 tracks × 3 voices): Parameters shared across a trio, but affecting all 9 voices.
  - **Quad Banks** (1 track × 4 voices): Used for the Quad Delay and Quad Reverb, allowing independent modulation for each speaker channel.
  - **Global Banks** (1 track × 1 voice): Used for global settings like the Theory of Time clock or mastering EQ.

**Crucially, the base value of a knob is identical for all voices within a track. However, modulation and gestures can apply *different* offsets to each voice within that track.**

## Base Encoders vs. Banked Encoders

- `StateEncoderCell`: The base class that holds the actual numerical state. It stores a value in the normalized range `[0, 1]`.
- `BankedEncoderCell`: Extends the state cell to support deep, polyphonic modulation and macro gestures. 

## Deep Modulation

Every `BankedEncoderCell` can act as a modulation destination.
- Up to 15 internal "LFOs" or envelopes (e.g., the PolyXFader LFOs, AHD envelopes) can be routed to any parameter.
- The **Modulation Depth** is itself implemented as a full `BankedEncoderCell`. This means you can modulate the modulation depth (e.g., using an LFO to slowly fade in an envelope's effect on the filter cutoff).
- **Polyphonic Modulation**: While the base knob value is shared across a track, the modulation sources (like voice-specific envelopes) are polyphonic. Therefore, the resulting modulated parameter values are calculated independently for every voice.
- **Crossfade Modulation**: To prevent clipping and dead spots, modulation is applied as a crossfade. A modulation depth of `1.0` means the parameter is 100% controlled by the modulator, completely overriding the base knob value.

## Gestures (Macros)

**Gestures** are macro controls mapped to physical analog inputs (Like sliders and joysticks).
- A gesture allows you to define a "Target State" for a parameter.
- When the gesture is at `0.0`, the parameter sits at its base knob value.
- When the gesture is at `1.0`, the parameter moves to the Target State.
- Like modulation depths, the Target State is implemented as a full `BankedEncoderCell` (though hidden from the normal UI) and is polyphonic.
- If you physically turn an encoder while a gesture is active, the system correctly updates the base knob or the Target State to match your physical action, ensuring the UI and physical knobs never fall out of sync.

## Scene Morphing

The entire state of all encoders (base values, modulation depths, and gesture targets) is duplicated across two **Scenes** (`SceneManager.hpp`).
- Scene 1 and Scene 2 act as complete snapshots of the synthesizer.
- A global `m_blendFactor` crossfades between the two scenes.
- Because every parameter is continuously interpolating between Scene 1 and Scene 2, moving the scene crossfader smoothly morphs every aspect of the sound engine simultaneously.

## UI State and Parameter Slew

- `EncoderBankUIState`: Manages the communication between the deep software state and the physical hardware/screen UI. It handles the rendering of LED rings and the processing of delta increments from physical endless encoders.
- **Parameter Slew**: We only calculate the parameter values every eight samples on control frames, the DSP engine applies a slew filter (`ParamSlew` or `FixedSlew`) to the final, post-modulation parameter values before using them in audio calculations. During oversampled blocks, this slew rate is adjusted automatically.

## Machine-Specific Parameters

Each parameter in the Voice banks can specify which source and filter machines it applies to via bit vectors (`MachineFlags`). Parameters are defined in `ForEachSmartGridOneParam.hpp` with `sourceMachines` and `filterMachines` arguments (e.g. `MachineFlags::x_dualWaveShapingVCOOnly` for parameters that only affect the Dual Wave Shaping VCO source).

When the user selects a different source machine (e.g. Thru) or filter machine, `UpdateEncodersForMachine()` runs and updates each encoder's connection status and short name. Parameters that don't apply to the current machine are shown as disconnected (`"---"`). This keeps the encoder grid relevant to the active machine. The update is triggered on machine change (config page) and track change (since each track can have different machines).

## Related
- [DSP Overview](dsp-overview.md)
- [Theory of Time](theory-of-time.md)