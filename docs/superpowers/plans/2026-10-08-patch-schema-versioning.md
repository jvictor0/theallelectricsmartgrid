# Patch Schema Versioning Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development to implement and review this task. Preserve all existing uncommitted encoder refactor work.

**Goal:** Save top-level patch `version: 1` and upgrade legacy v0 patches before publishing them to the audio thread.

**Architecture:** A small JSON-only `PatchFormat` unit owns the current version and v0-to-v1 transformation. `StateInterchange::ParseForLoad` runs it while holding the load arena's write ownership, retrying parse plus conversion if that arena exhausts. The live synth consumes only the converted tree; its serializer stamps the current version.

**Tech Stack:** C++17, arena-backed JSON, doctest, OpenSpec, Python recording integration tests.

## Global Constraints

- Top-level field is `version`, an integer. Absent means zero; current version is one.
- Migration runs on the message/loading thread before `m_loadRequested` publishes a tree to the audio thread. Disk open, version-history open, startup open, and queued retries all share this boundary.
- Preserve existing uncommitted work and leave new edits uncommitted. No unrelated DSP changes or old bipolar-depth reinterpretation.
- Follow repository C++ style. No newly introduced default arguments. Keep helpers in a public struct.
- Convert in memory on open. Existing files are rewritten only through the normal save flow.

### Task 1: Version patch serialization and migrate legacy encoder trees before publication

**Files:**
- Create `private/src/PatchFormat.hpp` for the migration and current version constant.
- Modify `private/src/StateInterchange.hpp` to upgrade between parsing and `FinishWrite`.
- Modify `private/src/TheNonagonSquiggleBoy.hpp` to write the version in `ToJSON`.
- Create `private/test/unit/patch_format.cpp` for migration and handoff regressions.
- Create `private/test/system/sys_patch_version.cpp` for live load/save round trips.
- Update only expectations affected by the new root field in existing tests, particularly `private/test/unit/streaming_recorder.cpp`, `private/test/system/sys_patch_roundtrip.cpp`, and `scripts/tests/test_sgrec.py`.
- Update `docs/encoder-system.md` and `openspec/specs/encoder-parameter-system/spec.md`; extend other recording specs only if the resulting contract needs clarification.

**Interfaces:**
```cpp
struct PatchFormat
{
    static constexpr int x_currentVersion = 1;
    static JSON Upgrade(JsonArena& arena, JSON patch);
};
```
`Upgrade` returns a converted v1 object, the unchanged v1 object, or JSON null for invalid/unsupported input or allocation failure. `arena.Failed()` distinguishes arena exhaustion for the caller's retry. Use the existing `ForEachSmartGridOneParam.hpp` parameter catalogue to recognize known Voice parameters (banks Source, FilterAndAmp, PanningAndSequencing, VoiceLFOs) and known shared parameters, without constructing a synth. Do not add a second manually maintained parameter-name list.

- [x] Write and run migration tests before production changes. Start at the existing `StateInterchange::ParseForLoad` boundary so tests compile before the new header exists. An unversioned input must become version 1 before audio consumption:
  ```cpp
  StateInterchange interchange;
  JSON patch = interchange.ParseForLoad(R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[[0.1,0.2,0.3],[0.4,0.5,0.6]]}}}})");
  DOCTEST_REQUIRE(patch.Get("version").IntegerValue() == 1);
  JSON encoders = patch.Get("squiggleBoy");
  DOCTEST_CHECK(encoders.Get("Harmonics1").IsNull());
  DOCTEST_CHECK(encoders.Get("Harmonics1Fire").Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.5));
  ```
  Use explicit independent values for Water/Fire/Earth across scenes, and one representative from each Voice family. Exercise real nested modulator→gesture and gesture→modulator chains, sparse null children, signed values, and `active[scene * 16 + trio]` selection. Assert shared parameter names stay unsuffixed, their old scene-by-one-track values flatten, and their active flags select track zero. Unknown root fields and unknown encoder keys remain intact.
- [x] Implement `PatchFormat::Upgrade` with strict top-level object and integer version dispatch. Missing and explicit zero run the same upgrade; integer one returns unchanged. Reject negative/future/noninteger versions, including explicit null, rather than treating them as zero. There is no broad patch-schema validation in this change. Reject ambiguous legacy/suffixed Voice root collisions instead of emitting duplicate names.
- [x] Build a fresh root and changed encoder objects in the same arena, preserving unrelated fields and replacing `version` and `squiggleBoy` exactly once. `JSON::SetNew` appends rather than replacing, so never append a replacement key onto an existing object. Use a focused object-member traversal inside `PatchFormat`; do not redesign the JSON library.
- [x] For each present legacy Voice root, produce `<name>Water`, `<name>Fire`, `<name>Earth`, with trio indexes 0, 1, 2. Recursively transform its `values.values[scene][trio]` into eight scalar scene values and its `active[scene * 16 + trio]` into eight booleans. Preserve values in patch units without polarity remapping. Apply the same recursion to `modulators` and `gestures`, preserving null slots and array indexes. Missing scene/track entries become zero and missing active entries false; absent fields remain absent for partial-load semantics. Shared encoder roots keep their names and select track zero. Already suffixed roots remain intact; already scalar shared values/8-entry activation arrays may be preserved to avoid damaging unversioned files produced during this worktree's transition. Unknown encoder roots remain untouched.
- [x] Integrate conversion into the existing arena retry loop:
  ```cpp
  JSON parsed;
  do
  {
      m_loadArena.Reset();
      parsed = m_loadArena.Loads(text);
      if (!parsed.IsNull())
      {
          parsed = PatchFormat::Upgrade(m_loadArena, parsed);
      }
      if (!m_loadArena.Failed())
      {
          break;
      }
      m_loadArena.GrowAndReset();
  }
  while (true);
  m_loadArena.FinishWrite(parsed);
  ```
  Preserve existing busy-storage and pending-request behavior. Tests must demonstrate the handed-off tree is fully upgraded before any audio frame, arena growth triggered by conversion (not just parsing), pending retry conversion, no load publication on rejected version, and no mutation of an arena held by a recorder reader.
- [x] Stamp serializer output using `rootJ.SetNew("version", a.Integer(PatchFormat::x_currentVersion));`. Verify ordinary saves and recording snapshots carry the same patch version; recording container version stays 5.
- [x] Exercise a literal legacy patch through `SynthRig::LoadPatch`, verify distinct live trio values/gestures and shared values, then save and reload its v1 JSON and confirm semantic stability. Include the existing real legacy patch fixture in validation. Extend existing save-arena allocation checks if necessary to prove migration does not run on audio; the handoff test must already prove conversion is complete before audio gets it.
- [x] Update tests which explicitly enumerate top-level keys or match serialized patch bytes. Keep legacy recording fixtures and reader support unchanged. Explain any intentional root-type rejection change in the existing malformed-patch test.
- [x] Document version 0/1, recursive mapping, strict version dispatch, memory-only upgrade, and message-thread boundary. Replace this refactor's previous statement that old-patch upgrading is out of scope.
- [x] Run targeted red/green tests, full standalone tests, Python recording tests with all C++ fixture exports, strict spec validation, and desktop compilation. Inspect the final diff for unrelated edits and pass it through independent review.

**Validation commands:** The existing configured build is `/tmp/encoder-refactor-review/build`; coordinate its use with the parent agent. Full build: `cmake --build /tmp/encoder-refactor-review/build -j6`. Python: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts/tests -p test_sgrec.py`. Spec: `openspec validate encoder-parameter-system --type spec --strict`. The coordinator will perform full integration and desktop checks after source changes are ready. The implementer should use a separate small focused build or the existing separate `/tmp/encoder-refactor-fixes/focused` build for red/green validation.

## Completion and verification

Implemented and independently reviewed on 2026-10-08. Scoped review, repair review, and final combined-worktree review approved with no open findings. All changes remain uncommitted.

- Full standalone suite: 551 cases and 90,496,877 assertions passed.
- Python recording integration: 50 tests passed with all four C++ fixture exports.
- Extracted recording patch reload: 1 case and 20 assertions passed.
- AddressSanitizer migration tests: 9 cases and 98 assertions passed.
- Universal macOS Release build succeeded.
- Strict encoder-parameter-system spec validation passed.
- Final malformed-root test cleanup was rebuilt and verified separately: 1 case and 9 assertions passed; production source was unchanged after the full suite.
- Final diff whitespace check passed.

Legacy loading during recording and Python replay have component coverage; there is no dedicated combined legacy-load-during-recording replay case.
