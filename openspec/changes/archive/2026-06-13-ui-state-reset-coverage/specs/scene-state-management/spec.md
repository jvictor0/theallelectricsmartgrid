## ADDED Requirements

### Requirement: Reset and Edit Operations Stay Coherent Across Scenes and Patches
Encoder reset/shift operations and discrete grid-cell edits SHALL leave the system in a state where the published UI surface and the underlying computed/stored values agree, and that agreement SHALL survive scene switches and patch save/load round-trips. Specifically: a shift-press reset performed while a given scene pair is active resets the current track's modulation for the active scene(s) only and SHALL NOT corrupt the stored values of inactive scenes; and any value altered by a reset or edit SHALL be serialized and restored by a save/load round-trip exactly as it stands after the edit (encoder state through `EncoderBankBank`, discrete grid/cell state through `StateSaver`; see encoder-parameter-system and the Theory of Time topology grid in phasor-timebase).

#### Scenario: Shift-reset in one scene leaves other scenes intact
- **WHEN** a parameter is modulated in both scene 1 and scene 2, scene 1 is the active scene (blend 0), and the performer shift-presses the encoder to reset it
- **THEN** the scene-1 modulation is cleared and the published value converges to the scene-1 base value after settle frames
- **AND** switching the active scene to scene 2 republishes scene 2's still-modulated value (scene 2 was not affected by the scene-1 reset)

#### Scenario: Post-reset state round-trips through a patch
- **WHEN** an encoder is modulated, then shift-reset, then the patch is saved, the system reset to defaults, and the patch reloaded
- **THEN** the reloaded encoder has no modulation and its published value equals the base value after settle frames, matching the state immediately after the reset

#### Scenario: Discrete grid-cell state round-trips and tracks scenes as registered
- **WHEN** a discrete grid cell backed by the `StateSaver` registry (such as a Theory of Time topology multiplier) is edited, the patch is saved, the system reset, and the patch reloaded
- **THEN** the cell's stored value is restored to its edited value
- **AND** the cell's published LED color reflects the restored value
