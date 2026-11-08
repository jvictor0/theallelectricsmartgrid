## Context

EncoderBankBank currently couples bank modes to 15-slot ModulatorValues. EncoderMatrix already binds a direct source pointer per root, and child encoders inherit their parent's state. Source DSP objects publish values before encoder processing and remain in their current owners.

## Goals / Non-Goals

**Goals:** Globally indexed QuadFloat source snapshots, filtering once per source, fixed enum-addressed groups and static parameter assignments, unchanged existing sound and patches.

**Non-Goals:** Dynamic assignment UI, assignment persistence, patch migration, DSP object relocation, variable lane widths, or a new matrix evaluator.

## Decisions

- Keep ModulatorValues as the encoder-facing snapshot and change tracker. Move decimators into ModulatorManager, processing every engine sample. Gather raw values, filtered values, and unfiltered amplitudes on host/matrix samples.
- Use separate ForEach source, group-slot, and parameter-group metadata. Initial groups reproduce Water/Fire/Earth/Quad/Global slots. Source identities remain independent of group assignments.
- Give base encoders a group values pointer independent of bank mode. Children copy that fixed binding from their parent; prepared matrix roots cache it. Fixed storage does not move during processing.
- Retain group-local previous snapshots, gesture state, and blend flags for prepared/editing evaluation equivalence. Broadcast shared gesture controls to groups.
- Resolve UI slot identities through groups while preserving source history identities. Patch serialization continues to store only existing local depth slots and values.

## Risks / Trade-offs

- Changing source timing could alter feedback or random sequences → preserve existing producer order and engine/host timing.
- Unused legacy slots can have persisted depths → preserve their zero values, unit amplitudes, and existing connection behavior.
- Mode-based UI assumptions could label reused sources incorrectly → route group metadata through UI state and translate local masks to global source identities.
- A significant unforeseen dependency or broken agent infrastructure → stop and ask the user rather than expanding scope.

## Migration Plan

Deploy the static default mappings with the implementation. Existing patches load unchanged. Reverting the implementation requires no data conversion.

## Open Questions

None for the agreed initial static groups.
