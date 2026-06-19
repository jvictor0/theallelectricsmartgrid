# Hidden Machine-Dependent Parameter Processing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hidden machine-dependent parameters stay wired and semantically live even when the selected trio's machine topology hides them from the visible 4x4 bank.

**Architecture:** Treat `EncoderBankBank::m_encoders[]` as the canonical named parameter set and each `EncoderBankInternal` as a UI projection. Move `BankedEncoderCell::SharedEncoderState` ownership to `EncoderBankBank::BankMode`, wire base cells at creation, let child cells inherit from their parent, then make processing and semantic bulk operations traverse the owner array or a mode-scoped subset instead of only visible bank cells.

**Tech Stack:** C++ headers under `private/src`, doctest system tests under `private/test/system`, `SynthRig` front-door test harness, CMake test binary `private/test/build/smartgrid_tests`.

---

## OpenSpec Source

- Change: `openspec/changes/fix-hidden-machine-parameter-processing`
- Proposal: `openspec/changes/fix-hidden-machine-parameter-processing/proposal.md`
- Design: `openspec/changes/fix-hidden-machine-parameter-processing/design.md`
- Spec delta: `openspec/changes/fix-hidden-machine-parameter-processing/specs/encoder-parameter-system/spec.md`
- Tasks: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

The implementation must preserve:

- No JSON schema changes.
- No UIState shape changes.
- Machine-incompatible parameters remain hidden in the selected bank view.
- DSP reads by parameter index observe up-to-date hidden parameter state.

## Files

- Modify: `private/src/EncoderBankBank.hpp`
  - Add mode-owned shared state.
  - Wire created encoders.
  - Add owner-array traversal helpers.
  - Move processing, clear, copy, full reset, and semantic affecting summaries to owner-array traversal.
- Modify: `private/src/EncoderBank.hpp`
  - Make `PlaceEncoder` a view-placement operation, not the only shared-state wiring source.
  - Keep UI projection and visible-bank summaries bank-local.
- Modify: `private/test/system/sys_gestures.cpp`
  - Temporarily skip the current hidden Detune repro while moving shared state.
  - Add focused hidden-parameter tests for processing, modulation source changes, gesture clear, visible bank-reset scope, and scene copy.
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`
  - Check off tasks only after implementation, review, and verification for the matching plan task.

## Verification Commands

- Build: `cmake --build private/test/build -j 8`
- Focused visible behavior check after shared-state move:
  `private/test/build/smartgrid_tests --test-case="sys_gestures:*"`
- Focused hidden behavior suite:
  `private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent*"`
- Full suite:
  `private/test/build/smartgrid_tests`
- Diff hygiene:
  `git diff --check`

---

### Task 1: Shared-State Wiring Refactor

**Files:**
- Modify: `private/src/EncoderBankBank.hpp`
- Modify: `private/src/EncoderBank.hpp`
- Modify: `private/test/system/sys_gestures.cpp`
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

- [ ] **Step 1: Temporarily skip the existing repro**

In `private/test/system/sys_gestures.cpp`, change:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches gesture weight changes")
```

to:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches gesture weight changes" * doctest::skip(true))
```

- [ ] **Step 2: Move shared-state ownership to `BankMode`**

In `private/src/EncoderBankBank.hpp`, update `BankMode`:

```cpp
struct BankMode
{
    size_t m_numTracks;
    size_t m_numVoices;
    SmartGrid::BankedEncoderCell::ModulatorValues m_modulatorValues;
    SmartGrid::BankedEncoderCell::SharedEncoderState m_sharedEncoderState;
    
    BankMode()
        : m_numTracks(0)
        , m_numVoices(0)
        , m_modulatorValues()
        , m_sharedEncoderState()
    {
    }
};
```

In `InitMode`, set both the legacy metadata and the canonical shared state:

```cpp
void InitMode(
    size_t modeIx,
    size_t numTracks,
    size_t numVoices)
{
    m_bankModes[modeIx].m_numTracks = numTracks;
    m_bankModes[modeIx].m_numVoices = numVoices;
    m_bankModes[modeIx].m_sharedEncoderState.m_numTracks = numTracks;
    m_bankModes[modeIx].m_sharedEncoderState.m_numVoices = numVoices;
    m_bankModes[modeIx].m_sharedEncoderState.m_modulatorValues = &m_bankModes[modeIx].m_modulatorValues;
}
```

- [ ] **Step 3: Wire every named encoder during creation**

In `EncoderBankBank::CreateEncoder`, immediately after `SmartGrid::BankedEncoderCell* cell = m_encoders[index].get();`, add:

```cpp
cell->m_sharedEncoderState = &m_bankModes[modeIx].m_sharedEncoderState;
```

Keep the existing `cell->m_numTracks`, default value, color, names, and switch metadata setup.

- [ ] **Step 4: Keep bank placement view-only**

In `private/src/EncoderBank.hpp`, update `EncoderBankInternal::PlaceEncoder` so it sets ownership needed for visible interaction but does not overwrite the mode-level shared state:

```cpp
void PlaceEncoder(int x, int y, BankedEncoderCell* encoder)
{
    m_baseCell[x][y] = encoder;
    if (encoder)
    {
        encoder->m_ownerBank = this;
    }

    if (!m_selected)
    {
        SetVisibleCell(x, y, encoder);
    }
}
```

Do not remove `EncoderBankInternal::m_sharedEncoderState` yet. It still drives UI projection fields in `PopulateUIState`, `GetCurrentTrack`, and bank-local current-track metadata.

- [ ] **Step 5: Keep mode and bank current-track state mirrored**

In `EncoderBankBank::SetTrack`, set the mode shared state before updating the bank views:

```cpp
void SetTrack(size_t modeIx, size_t track)
{
    m_bankModes[modeIx].m_sharedEncoderState.m_currentTrack = track;

    for (size_t i = 0; i < m_numBanks; ++i)
    {
        if (m_bankConfigs[i].m_modeIx == modeIx)
        {
            m_banks[i].SetTrack(track);
        }
    }
}
```

- [ ] **Step 6: Build and run focused visible behavior**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="sys_gestures:*"
```

Expected:

- Build succeeds.
- Existing gesture tests pass.
- The skipped hidden Detune repro is reported as skipped, not failed.

- [ ] **Step 7: Update OpenSpec task progress**

Check off tasks `1.1` through `1.5` in `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md` only after Step 6 passes.

Checkpoint commit message if commits are requested:

```bash
git add private/src/EncoderBank.hpp private/src/EncoderBankBank.hpp private/test/system/sys_gestures.cpp openspec/changes/fix-hidden-machine-parameter-processing/tasks.md
git commit -m "refactor: wire encoder state at mode level"
```

---

### Task 2: Hidden-Parameter Regression Tests

**Files:**
- Modify: `private/test/system/sys_gestures.cpp`
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

- [ ] **Step 1: Add a hidden Detune helper**

At the end of the existing helper section in `private/test/system/sys_gestures.cpp`, add:

```cpp
struct HiddenDetuneFixture
{
    using Param = SmartGridOneEncoders::Param;
    using SourceMachine = VoiceMachine::SourceMachine;
    using Trio = TheNonagonSmartGrid::Trio;

    synthrig::SynthRig m_rig;

    HiddenDetuneFixture()
        : m_rig()
    {
        m_rig.SetLeftScene(0);
        m_rig.SetRightScene(1);
        m_rig.SetBlend(0.0f);

        for (size_t voice = 3; voice < 6; ++voice)
        {
            m_rig.Internal().m_squiggleBoy.m_state[voice].m_voiceConfig.m_sourceMachine = SourceMachine::Sample;
        }

        SelectWaterSource();
    }

    void SelectWaterSource()
    {
        m_rig.Internal().SetActiveTrio(Trio::Water);
        m_rig.Internal().m_squiggleBoy.m_encoders.SelectBank(SmartGridOneEncoders::Bank::Source);
        m_rig.RunFrames(2);
    }

    void SelectEarthSource()
    {
        m_rig.Internal().SetActiveTrio(Trio::Earth);
        m_rig.Internal().m_squiggleBoy.m_encoders.SelectBank(SmartGridOneEncoders::Bank::Source);
        m_rig.RunFrames(2);
    }

    float WaterDetuneVoice(size_t voice)
    {
        return m_rig.Internal().m_squiggleBoy.m_encoders.GetValueNoSlew(Param::OscillatorDetune, voice);
    }

    void SelectDetuneModulators()
    {
        SelectWaterSource();
        m_rig.PressEncoder(1, 3);
        m_rig.RunFrames(1);
    }

    void DeselectDetuneModulators()
    {
        m_rig.PressEncoder(3, 3);
        m_rig.RunFrames(1);
    }

    void AddGestureToSpreadDepth(int gesture)
    {
        SelectDetuneModulators();
        m_rig.SetFader(gesture, 1.0f);
        m_rig.RunFrames(2);
        m_rig.PressGesturePad(gesture);
        m_rig.RunFrames(1);
        for (int i = 0; i < 8; ++i)
        {
            m_rig.IncEncoder(3, 2, +50);
            m_rig.RunFrames(1);
        }

        m_rig.ReleaseGesturePad(gesture);
        m_rig.RunFrames(kSettleFrames);
        DeselectDetuneModulators();
    }
};
```

- [ ] **Step 2: Replace the skipped repro body with helper usage**

Keep the test skipped for now, but simplify its setup to use `HiddenDetuneFixture`. The assertion must remain the same: gesture 2 changes while Earth hides Detune, then Water's direct Detune read must rise above `0.5f`.

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches gesture weight changes" * doctest::skip(true))
{
    HiddenDetuneFixture fixture;
    fixture.AddGestureToSpreadDepth(2);

    fixture.m_rig.SetFader(2, 0.0f);
    fixture.m_rig.RunFrames(kSettleFrames);

    float waterDetuneAtZero = fixture.WaterDetuneVoice(2);
    DOCTEST_REQUIRE_MESSAGE(waterDetuneAtZero < 0.1f,
        "precondition: gesture 2 at zero should remove Water spread detune, got "
        << waterDetuneAtZero);

    fixture.SelectEarthSource();
    fixture.m_rig.SetFader(2, 1.0f);
    fixture.m_rig.RunFrames(kSettleFrames);

    fixture.SelectWaterSource();
    fixture.m_rig.RunFrames(kSettleFrames);

    float waterDetuneAtOne = fixture.WaterDetuneVoice(2);
    DOCTEST_CHECK_MESSAGE(waterDetuneAtOne > 0.5f,
        "Water Detune missed gesture 2's weight change while hidden by Earth's Sample topology, got "
        << waterDetuneAtOne);
}
```

- [ ] **Step 3: Add the hidden modulation-source anomaly test**

Add:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches modulator source changes")
{
    HiddenDetuneFixture fixture;
    fixture.SelectDetuneModulators();

    fixture.m_rig.IncEncoder(3, 2, +50);
    fixture.m_rig.RunFrames(1);
    fixture.DeselectDetuneModulators();

    fixture.SelectEarthSource();
    auto& modulatorValues = fixture.m_rig.Internal().m_squiggleBoy.m_encoders.GetModulatorValues(SmartGridOneEncoders::BankMode::Voice);
    for (size_t voice = 0; voice < 16; ++voice)
    {
        modulatorValues.m_value[11][voice] = 1.0f;
    }

    fixture.m_rig.RunFrames(kSettleFrames);
    float hiddenValue = fixture.WaterDetuneVoice(2);

    DOCTEST_CHECK_MESSAGE(hiddenValue > 0.5f,
        "Water Detune missed Spread modulator source changes while hidden, got "
        << hiddenValue);
}
```

Expected before Task 3 implementation: FAIL because hidden Detune is not computed while hidden.

- [ ] **Step 4: Add the hidden gesture-clear anomaly test**

Add:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune clears gesture while hidden")
{
    HiddenDetuneFixture fixture;
    fixture.AddGestureToSpreadDepth(2);

    fixture.SelectEarthSource();
    fixture.m_rig.WithShift([&fixture]() {
        fixture.m_rig.PressGesturePad(2);
        fixture.m_rig.RunFrames(1);
        fixture.m_rig.ReleaseGesturePad(2);
    });
    fixture.m_rig.RunFrames(kSettleFrames);

    fixture.m_rig.SetFader(2, 0.0f);
    fixture.m_rig.RunFrames(kSettleFrames);
    float atZero = fixture.WaterDetuneVoice(2);

    fixture.m_rig.SetFader(2, 1.0f);
    fixture.m_rig.RunFrames(kSettleFrames);
    float atOne = fixture.WaterDetuneVoice(2);

    DOCTEST_CHECK_MESSAGE(std::fabs(atOne - atZero) < 0.05f,
        "hidden Detune still responds to gesture 2 after ClearGesture while hidden: zero="
        << atZero << " one=" << atOne);
}
```

Expected before Task 4 implementation: FAIL because `ClearGesture` only traverses visible bank cells.

- [ ] **Step 5: Add the visible bank-reset scope test**

Add:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune survives visible bank reset")
{
    HiddenDetuneFixture fixture;
    fixture.SelectWaterSource();
    fixture.m_rig.SetEncoder(1, 3, 0.8f);
    fixture.m_rig.RunFrames(kSettleFrames);
    DOCTEST_REQUIRE(fixture.WaterDetuneVoice(2) > 0.7f);

    fixture.SelectEarthSource();
    fixture.m_rig.Internal().m_squiggleBoy.m_encoders.ResetBank(SmartGridOneEncoders::Bank::Source);
    fixture.m_rig.RunFrames(kSettleFrames);

    float hiddenValue = fixture.WaterDetuneVoice(2);
    DOCTEST_CHECK_MESSAGE(hiddenValue > 0.7f,
        "hidden Detune should not reset from Earth's visible Source grid reset, got "
        << hiddenValue);
}
```

Expected before Task 4 implementation: PASS. This documents that shift-resetting a bank selector resets the visible grid projection only.

- [ ] **Step 6: Add the hidden scene-copy anomaly test**

Add:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune copies scene state while hidden")
{
    HiddenDetuneFixture fixture;

    fixture.m_rig.SetLeftScene(0);
    fixture.m_rig.SetRightScene(1);
    fixture.m_rig.SetBlend(0.0f);
    fixture.SelectWaterSource();
    fixture.m_rig.SetEncoder(1, 3, 0.8f);
    fixture.m_rig.RunFrames(kSettleFrames);
    DOCTEST_REQUIRE(fixture.WaterDetuneVoice(2) > 0.7f);

    fixture.m_rig.SetLeftScene(2);
    fixture.m_rig.SetRightScene(1);
    fixture.m_rig.SetBlend(0.0f);
    fixture.SelectWaterSource();
    fixture.m_rig.SetEncoder(1, 3, 0.0f);
    fixture.m_rig.RunFrames(kSettleFrames);

    fixture.SelectEarthSource();
    fixture.m_rig.Internal().m_squiggleBoy.m_encoders.CopyToScene(2);
    fixture.m_rig.RunFrames(kSettleFrames);

    fixture.m_rig.SetLeftScene(2);
    fixture.m_rig.SetRightScene(1);
    fixture.m_rig.SetBlend(0.0f);
    fixture.SelectWaterSource();
    fixture.m_rig.RunFrames(kSettleFrames);

    float copiedValue = fixture.WaterDetuneVoice(2);
    DOCTEST_CHECK_MESSAGE(copiedValue > 0.7f,
        "hidden Detune did not copy current scene state into scene 2, got "
        << copiedValue);
}
```

Expected before Task 4 implementation: FAIL because `CopyToScene` only traverses visible bank cells.

- [ ] **Step 7: Run the anomaly tests**

Run each new focused test. Expected:

- Hidden modulation-source test fails.
- Hidden gesture-clear test fails.
- Hidden default/reset test fails.
- Hidden scene-copy test fails.
- Existing skipped hidden gesture-weight repro remains skipped.

Use commands like:

```bash
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent detune catches modulator source changes"
```

- [ ] **Step 8: Update OpenSpec task progress**

Check off tasks `2.1` through `2.6` only after the tests are added and each exposes the expected anomaly.

Checkpoint commit message if commits are requested:

```bash
git add private/test/system/sys_gestures.cpp openspec/changes/fix-hidden-machine-parameter-processing/tasks.md
git commit -m "test: expose hidden machine parameter anomalies"
```

---

### Task 3: Mode-Level Processing

**Files:**
- Modify: `private/src/EncoderBankBank.hpp`
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

- [ ] **Step 1: Add owner-array traversal helpers**

In `EncoderBankBank`, add:

```cpp
bool EncoderBelongsToMode(size_t encoderIx, size_t modeIx)
{
    SmartGrid::BankedEncoderCell* cell = GetEncoder(encoderIx);
    return cell && cell->m_sharedEncoderState == &m_bankModes[modeIx].m_sharedEncoderState;
}

template <typename Func>
void ForEachNamedEncoder(Func func)
{
    for (size_t i = 0; i < m_numEncoders; ++i)
    {
        SmartGrid::BankedEncoderCell* cell = GetEncoder(i);
        if (cell && cell->m_name)
        {
            func(i, cell);
        }
    }
}

template <typename Func>
void ForEachNamedEncoderInMode(size_t modeIx, Func func)
{
    ForEachNamedEncoder([this, modeIx, &func](size_t encoderIx, SmartGrid::BankedEncoderCell* cell) {
        if (EncoderBelongsToMode(encoderIx, modeIx))
        {
            func(encoderIx, cell);
        }
    });
}
```

- [ ] **Step 2: Process the canonical owner array**

Change `EncoderBankBank::Process` so control frames compute the flat named encoder set after mode change masks are computed:

```cpp
void Process()
{
    if (SampleTimer::IsControlFrame())
    {
        for (size_t i = 0; i < m_numModes; ++i)
        {
            m_bankModes[i].m_modulatorValues.ComputeChanged();
        }
    }

    if (m_sceneManager->m_changed)
    {
        HandleChangedSceneManager();
    }

    if (m_sceneManager->m_changedScene)
    {
        HandleChangedSceneManagerScene();
    }

    if (SampleTimer::IsControlFrame())
    {
        ForEachNamedEncoder([](size_t, SmartGrid::BankedEncoderCell* cell) {
            cell->Compute();
        });

        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].ProcessTopology();
        }
    }
}
```

- [ ] **Step 3: Make `ProcessTopology` view-only**

In `EncoderBankInternal::ProcessTopology`, remove `cell->Compute()` and keep only active-prefix calculation:

```cpp
void ProcessTopology()
{
    m_activeEncoderPrefix = 0;

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            BankedEncoderCell* cell = GetBase(i, j);
            if (cell && cell->m_connected)
            {
                m_activeEncoderPrefix = i * 4 + j + 1;
            }
        }
    }
}
```

- [ ] **Step 4: Verify processing tests**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent detune catches modulator source changes"
```

Expected:

- Build succeeds.
- Hidden modulation-source test passes.
- Existing skipped gesture-weight repro remains skipped until Task 5.

- [ ] **Step 5: Update OpenSpec task progress**

Check off tasks `3.1` through `3.3` only after Step 4 passes.

Checkpoint commit message if commits are requested:

```bash
git add private/src/EncoderBank.hpp private/src/EncoderBankBank.hpp openspec/changes/fix-hidden-machine-parameter-processing/tasks.md
git commit -m "fix: process hidden machine parameters"
```

---

### Task 4: Mode-Level Bulk Operations

**Files:**
- Modify: `private/src/EncoderBankBank.hpp`
- Modify: `private/src/EncoderBank.hpp`
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

- [ ] **Step 1: Add a mode-aware affecting summary refresh**

In `EncoderBankBank`, add:

```cpp
void SetAllModulatorsAffectingForMode(size_t modeIx)
{
    ForEachNamedEncoderInMode(modeIx, [](size_t, SmartGrid::BankedEncoderCell* cell) {
        cell->SetModulatorsAffecting();
    });

    for (size_t i = 0; i < m_numBanks; ++i)
    {
        if (m_bankConfigs[i].m_modeIx == modeIx)
        {
            m_banks[i].ComputeGesturesAffectingPerTrack();
        }
    }
}
```

This recomputes hidden semantic state on the owner array and keeps visible UI summaries bank-local.

- [ ] **Step 2: Make scene manager state refresh owner-array aware**

Change `HandleChangedSceneManager`:

```cpp
void HandleChangedSceneManager()
{
    ForEachNamedEncoder([](size_t, SmartGrid::BankedEncoderCell* cell) {
        cell->SetStateRecursive();
    });
}
```

Change `HandleChangedSceneManagerScene`:

```cpp
void HandleChangedSceneManagerScene()
{
    for (size_t modeIx = 0; modeIx < m_numModes; ++modeIx)
    {
        SetAllModulatorsAffectingForMode(modeIx);
    }
}
```

- [ ] **Step 3: Make `ClearGesture` owner-array aware**

Change `EncoderBankBank::ClearGesture`:

```cpp
void ClearGesture(int gesture)
{
    ForEachNamedEncoder([gesture](size_t, SmartGrid::BankedEncoderCell* cell) {
        cell->ClearGesture(gesture);
        cell->SetForceUpdateRecursive();
    });

    for (size_t modeIx = 0; modeIx < m_numModes; ++modeIx)
    {
        SetAllModulatorsAffectingForMode(modeIx);
    }
}
```

- [ ] **Step 4: Keep bank reset visible-grid scoped**

Replace `ResetGrid` with a visible-grid reset. It should not take `allScenes` or `allTracks`, and should not loop over the global named encoder array.

```cpp
void ResetGrid(uint64_t ix)
{
    m_banks[ix].RevertToDefault(false, false);
    m_banks[ix].SetAllModulatorsAffecting();
}
```

- [ ] **Step 5: Make full default reset owner-array aware**

Change `EncoderBankBank::RevertToDefault`:

```cpp
void RevertToDefault(bool allScenes, bool allTracks)
{
    ForEachNamedEncoder([allScenes, allTracks](size_t, SmartGrid::BankedEncoderCell* cell) {
        cell->RevertToDefault(allScenes, allTracks);
    });

    for (size_t modeIx = 0; modeIx < m_numModes; ++modeIx)
    {
        SetAllModulatorsAffectingForMode(modeIx);
    }
}
```

- [ ] **Step 6: Make scene copy owner-array aware**

Change `EncoderBankBank::CopyToScene`:

```cpp
void CopyToScene(int scene)
{
    ForEachNamedEncoder([scene](size_t, SmartGrid::BankedEncoderCell* cell) {
        cell->CopyToScene(scene);
    });
}
```

- [ ] **Step 7: Verify bulk-operation tests**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent detune clears gesture while hidden"
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent detune survives visible bank reset"
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent detune copies scene state while hidden"
```

Expected:

- Build succeeds.
- All three hidden bulk-operation tests pass.

- [ ] **Step 8: Update OpenSpec task progress**

Check off tasks `4.1` through `4.5` only after Step 7 passes.

Checkpoint commit message if commits are requested:

```bash
git add private/src/EncoderBank.hpp private/src/EncoderBankBank.hpp openspec/changes/fix-hidden-machine-parameter-processing/tasks.md
git commit -m "fix: apply bulk operations to hidden parameters"
```

---

### Task 5: Re-enable Repro and Final Verification

**Files:**
- Modify: `private/test/system/sys_gestures.cpp`
- Modify: `openspec/changes/fix-hidden-machine-parameter-processing/tasks.md`

- [ ] **Step 1: Re-enable the original repro**

In `private/test/system/sys_gestures.cpp`, change:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches gesture weight changes" * doctest::skip(true))
```

back to:

```cpp
DOCTEST_TEST_CASE("sys_gestures: hidden machine-dependent detune catches gesture weight changes")
```

- [ ] **Step 2: Run focused hidden suite**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="sys_gestures: hidden machine-dependent*"
```

Expected:

- Gesture-weight repro passes.
- Modulation-source test passes.
- Gesture-clear test passes.
- Visible bank-reset scope test passes.
- Scene-copy test passes.

- [ ] **Step 3: Run full verification**

Run:

```bash
private/test/build/smartgrid_tests
git diff --check
git diff --stat
```

Expected:

- Full suite passes.
- `git diff --check` has no output.
- Diff is limited to the files listed in this plan and the OpenSpec task checkboxes.

- [ ] **Step 4: Update OpenSpec task progress**

Check off tasks `5.1` through `5.4` after Step 3 passes.

Checkpoint commit message if commits are requested:

```bash
git add private/test/system/sys_gestures.cpp openspec/changes/fix-hidden-machine-parameter-processing/tasks.md
git commit -m "test: verify hidden machine parameter behavior"
```

---

## Review Requirements

For each task:

- Run spec compliance review first against `openspec/changes/fix-hidden-machine-parameter-processing/specs/encoder-parameter-system/spec.md`.
- Run code quality review second against the changed files.
- Fix and re-review until both pass before checking off OpenSpec tasks.

Final review checklist:

- Hidden machine-dependent parameters are wired at creation.
- Bank placement no longer controls whether a parameter has shared processing state.
- Processing covers every named encoder cell.
- UI projection remains selected-machine filtered.
- `ClearGesture`, full `RevertToDefault`, and `CopyToScene` reach hidden parameters; `ResetBank` remains visible-grid scoped.
- No JSON, DSP parameter name, controller route, or UIState schema changes.
