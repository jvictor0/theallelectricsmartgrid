## Context

`JUCE/SmartGridOne/Source/ConfigPage.hpp` currently owns the entire configuration UI for MIDI controller inputs, outputs, controller shapes, and stereo mode. The page stores raw `juce::ComboBox` members in `ControllerSection` and repeats population, current-value lookup, and apply logic across the component.

`MainComponent::OnBackButtonClicked()` is the natural boundary for finalizing config-page changes: it destroys the config page and saves config JSON. JUCE audio currently starts with `setAudioChannels(4, 7)`, while config persistence stores only the nonagon config, stereo flag, and file config.

## Goals / Non-Goals

**Goals:**

- Keep the refactor local to the JUCE config-page/configuration surface.
- Replace ad hoc dropdown handling in `ConfigPage.hpp` with a small typed JUCE component that owns label, combo population, current selection, and change callback behavior.
- Preserve all existing MIDI controller selection, controller-shape selection, KMix output-only behavior, Generic no-output behavior, WrldBldr behavior, and stereo checkbox behavior.
- Add audio input and audio output rows to the config page.
- Persist selected audio input/output names in config JSON and restore them on startup.
- Restart JUCE audio on config back only when the selected audio input or output changed.

**Non-Goals:**

- No DSP, MIDI transport, controller protocol, grid, patch, or file-page behavior changes.
- No project-wide UI component framework.
- No new source files unless the implementation proves impossible to keep readable in `ConfigPage.hpp`.
- No C-style casts or deviations from the repository formatting rules.

## Decisions

1. Define a local typed dropdown component in `ConfigPage.hpp`.

   Use a `struct` such as `ConfigDropdownRow : juce::Component` with public members/functions, `paint`/`resized` overrides where useful, and HammerCase helper methods for population and selection. This keeps the refactor in the file requested by the user while making each row responsible for its own label, combo box, item IDs, and selection state.

   Alternative considered: keep `ControllerSection` as a bag of labels and raw combo boxes. That would preserve behavior but would not solve the messy dropdown ownership and would make audio rows another special case.

2. Keep controller semantics in `ControllerSection`, but make dropdown widgets reusable.

   `ControllerSection` should still decide which controller routes exist for Launchpad, Encoder, Generic, WrldBldr, and KMix. The reusable row should not know about Nonagon route calls. This preserves the current routing behavior while reducing widget duplication.

   Alternative considered: introduce a polymorphic hierarchy per controller type. That is larger than the requested file-local cleanup and would make the refactor escape the current page's needs.

3. Store audio device names in `Configuration`.

   Add `juce::String m_audioInputDeviceName` and `juce::String m_audioOutputDeviceName` to the existing `Configuration` struct. `MainComponent::SaveConfig()` writes them under the existing `nonagon_config` object alongside `stereo`, and `LoadConfig()` treats missing fields as empty/default selections for backwards compatibility.

   Alternative considered: persist JUCE's full `AudioDeviceManager` XML state. That would be more complete but introduces a broader data model change than the requested input/output dropdowns.

4. Let `MainComponent` own JUCE audio restart.

   `ConfigPage` can expose whether audio selections changed and the selected names. `MainComponent::OnBackButtonClicked()` should check this before destroying the page. When changed, update `Configuration`, call `shutdownAudio()`, then reopen audio with the selected input/output device names through `deviceManager` or the appropriate `AudioAppComponent`/`AudioDeviceManager` path.

   Alternative considered: restart audio immediately from the config page's combo callback. Back-button commit is less disruptive and matches the user's request to reset JUCE on back if changed.

## Risks / Trade-offs

- Audio device APIs differ by platform or device type -> keep the selected values as device names and use JUCE's enumerated `AudioIODeviceType`/`AudioIODeviceSetup` path defensively, falling back to existing defaults if a saved device is unavailable.
- Selecting unavailable saved devices could leave the combo blank -> include a `None`/default row and avoid crashing or forcing invalid selections.
- Restarting audio can briefly interrupt sound -> only do it when the audio device names changed, not for MIDI/controller/stereo edits.
- Preserving Generic/KMix special cases is easy to regress during cleanup -> cover those cases in tasks and manual verification.
