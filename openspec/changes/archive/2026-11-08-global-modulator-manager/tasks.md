## 1. Source and group model

- [x] 1.1 Define global source IDs, fixed group IDs, group slots, and parameter assignments with ForEach metadata.
- [x] 1.2 Add manager-owned source filtering and host-frame group gathering; separate encoder group binding from bank mode.

## 2. Integration

- [x] 2.1 Redirect DSP source publication and gesture inputs without changing DSP ownership or processing order.
- [x] 2.2 Route UI source metadata and affecting masks through group bindings while preserving patch storage and loading.

## 3. Validation and completion

- [x] 3.1 Update source-injection fixtures and add coverage for source reuse, independent groups, child inheritance, and patch compatibility.
- [x] 3.2 Run the core tests, relevant UI validation, and OpenSpec validation; review the final diff.
- [x] 3.3 Verify implementation and artifacts are ready to archive and land.

Validation: core and passthrough suites, visualizer suite, performance smoke/protocol suites, encoder source/model audit, strict OpenSpec change/spec checks, and diff whitespace checks passed. Patch save/load function bodies are unchanged.
