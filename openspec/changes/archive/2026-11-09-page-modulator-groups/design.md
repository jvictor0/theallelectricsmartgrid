## Context

The manager separates group slots from globally indexed source values. Some source/history metadata still uses legacy slot numbers, so the larger random catalogue must have its own capacity while groups remain 15 slots.

## Goals / Non-Goals

**Goals:** Independent random pairs for each page family, fixed three-/four-lane groups, source-declared downsampling, and internal VCO morph randomness.

**Non-Goals:** Patch format changes, runtime group reassignment, mixed lane counts, main/remote-main updates, or a redesign of the modulation overview panel.

## Decisions

- Keep source identifiers distinct from group slots. Preserve existing non-random source identities and append the additional random identities; expand catalogue/history lookup capacity independently of the 15-slot matrix.
- Use explicit ForEach group bindings and page-derived parameter assignments. All source-machine pages share Source; voice mixer gain and sends use Shape; quad mixer controls use their corresponding effect group.
- Keep other sources in their existing local slots in every applicable group. Leave voice/quad slots 0/1 unassigned; give slots 2/3 unique independent random pairs. Global bindings and random presets remain unchanged.
- Source metadata declares whether to downsample. Only envelopes and waveform LFOs advance decimators; other sources publish their current raw value at the host boundary. Modulation noise draws occur only on host samples.
- Own the two morph random generators inside DualWaveShapingVCO. Use Standard(6, 0.1) for both internal morph generators and update wavetable visibility checks to read those internal sources.
- Additional random sources get encoder visualizers and history routing; retain the existing overview panel layout.

## Risks / Trade-offs

- Larger source catalogue could be confused with the fixed slot count → separate capacities and test source IDs beyond slot 14.
- New bindings change audible random-slot meanings in existing patches → retain serialization and apply the explicitly requested mappings without migration.
- Random generator ownership changes affect table replacement → test the visibility path against internal morph outputs.
- Significant unforeseen dependency or broken agent infrastructure → stop and ask instead of expanding scope.
