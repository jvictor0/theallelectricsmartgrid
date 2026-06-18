## 1. Refactor Config Dropdown Rows

- [x] 1.1 Add a file-local typed dropdown row component in `ConfigPage.hpp` that owns label, combo box, item population, current selection, and change notification.
- [x] 1.2 Replace direct `ControllerSection` combo-box handling with the typed row component while preserving section labels, layout, callbacks, and controller-shape choices.
- [x] 1.3 Verify Launchpad, Encoder, WrldBldr, Generic, and KMix sections still expose the same MIDI input/output/shape controls as before.

## 2. Add Audio Device Configuration

- [x] 2.1 Add audio input and audio output device-name fields to `Configuration`.
- [x] 2.2 Populate audio input/output rows from JUCE audio device types/devices and initialize them from saved configuration or current device state.
- [x] 2.3 Track the audio input/output selections present when the config page opens and expose whether either selection changed.

## 3. Apply Audio Changes On Back

- [x] 3.1 Update `MainComponent::OnBackButtonClicked()` to read pending audio selections before destroying `ConfigPage`.
- [x] 3.2 Restart JUCE audio only when the selected audio input or output changed, preserving the existing channel counts and default behavior when no explicit device is selected.
- [x] 3.3 Ensure MIDI/controller/stereo-only edits still save config without forcing an audio restart.

## 4. Persist Config JSON

- [x] 4.1 Save audio input and audio output device names through `SaveConfig()` in the existing config JSON structure.
- [x] 4.2 Load audio input and audio output device names through `LoadConfig()` while accepting older JSON that does not contain those fields.
- [x] 4.3 Confirm existing MIDI controller JSON save/load remains unchanged.

## 5. Verification

- [x] 5.1 Build the JUCE target or run the closest available compile check for `JUCE/SmartGridOne`.
- [x] 5.2 Manually inspect the config-page flow for visible MIDI rows, audio rows, stereo checkbox behavior, Back save behavior, and no config-page layout regressions.
- [x] 5.3 Test config JSON round-trip for selected audio input/output names and missing-field backwards compatibility.
