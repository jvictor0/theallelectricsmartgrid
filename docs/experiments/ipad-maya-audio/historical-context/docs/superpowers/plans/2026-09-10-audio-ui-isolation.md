# Audio UI Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task by task. Steps use checkbox syntax for tracking.

**Goal:** Prepare a temporary signed iPad build that restores normal audio processing while independently disabling recurring UI rendering, so the two observed audio failure modes can be tested over Wi-Fi.

**Architecture:** Add two immutable launch-time flags to MainComponent. Preserve the installed tone behavior as the default, allow normal Nonagon processing with an environment value, and independently suppress visual refresh by hiding child components and skipping display/label/repaint work while retaining the message timer, state interchange, logging, and platform diagnostics. The parent handles wireless device control, recording, and deployment after code review.

**Tech Stack:** Existing JUCE 8.0.2 C++ source, Xcode signed iOS Release build, environment variables passed on app launch.

## Global Constraints

- Work only in /Users/joyo/.codex/worktrees/e18e/theallelectricsmartgrid on codex/audio-diagnostics-48khz; preserve all pre-existing uncommitted diagnostic changes. Do not edit main, commit, merge, push, or deploy from the implementer.
- This is a disposable diagnostic experiment authorized by the user, not a claimed fix. Keep actual rate/frame tracing and reject non-48000 Hz output as before. Keep 48000 Hz requested, 512 frames requested, zero application inputs, existing output routes and remaining workers unchanged.
- Modify only JUCE/SmartGridOne/Source/MainComponent.h and MainComponent.cpp plus this plan and this plan's scratch artifacts. Do not upgrade JUCE or change the backend.
- Read environment variables once before opening audio. SMARTGRID_AUDIO_TEST_MODE=normal restores normal processing; missing or any other value retains tone. SMARTGRID_UI_RENDER=0 disables UI refresh; missing or any other value leaves UI on. Log the resolved modes explicitly at startup.
- Both modes preserve prepareToPlay, sample-rate guard, callback diagnostics, thermal polling, message timer cadence, AsyncLogQueue draining, and state interchange. UI-off preserves normal DSP/MIDI/state production when normal mode is selected.
- Follow AGENTS.md for new C++: Allman braces, m_ members, x_ constants, public members, no C casts, new comments followed by a separate // line. Preserve unrelated formatting.
- Validate with signed iOS build, focused source review, and parent-run device experiments. Do not add permanent unit tests that merely duplicate the prototype branches. Keep evidence and source backups; another agent is cleaning disk space.

### Task 1: Implement and build independently selectable audio and UI test modes

**Files:** Modify JUCE/SmartGridOne/Source/MainComponent.h and MainComponent.cpp. Requirements include every Global Constraint above.

**Interfaces:** Consumes the existing m_nonagon.Process(bufferToFill, MakeIOInfo()), tone loop, MainComponent timer, and platform diagnostics. Produces the two launch flags and a signed app at /private/tmp/smartgrid-ui-isolation-ios-build/Build/Products/Release-iphoneos (report actual product path). Parent will launch with environment variables after review.

- [x] Inspect the existing header, constructor, paint, resized, and timer callback. Compare original normal branch with /private/tmp/smartgrid-before-tone/MainComponent.h. Baseline files for this task are saved in /private/tmp/smartgrid-ui-isolation-before/.
- [x] Add public immutable members before audio startup:

```cpp
const bool m_useTestTone = juce::SystemStats::getEnvironmentVariable("SMARTGRID_AUDIO_TEST_MODE", "tone") != "normal";
const bool m_renderTestUi = juce::SystemStats::getEnvironmentVariable("SMARTGRID_UI_RENDER", "1") != "0";
```

Log `Audio experiment audio_mode=%s ui_render=%d` after configuring the log directory and before OpenAudioDevice. In prepareToPlay, retain the tone marker only for tone and log `Audio test mode=normal; normal callback processing enabled` for normal. Keep phase reset and existing preparation.

- [x] Keep the common configuration forceStereo/stereo updates, then branch inside the existing valid-rate branch:

```cpp
if (m_useTestTone)
{
    // Existing complete phase-continuous tone loop remains here.
    //
}
else
{
    m_nonagon.Process(bufferToFill, MakeIOInfo());
}
```

The comment above describes moving the EXISTING loop verbatim; do not replace its implementation with a placeholder. Preserve the invalid-rate clearing and all callback timing logs after the branch.

- [x] At the end of construction, invoke SetDisplayMode only if UI is enabled. If UI is disabled, hide every direct child component after their creation:

```cpp
for (int i = 0; i < getNumChildComponents(); ++i)
{
    getChildComponent(i)->setVisible(false);
}
```

Leave the top-level app window alive. In paint, after filling the background, when UI is disabled draw the static text `SmartGridOne: UI rendering paused for audio test` and return. One initial repaint is allowed. Preserve normal paint behavior when enabled. Hidden controls must not trigger configuration actions.

- [x] In timerCallback keep platform diagnostics, HandleStateInterchange(), and AsyncLogQueue::s_instance.DoLog() unconditional. Wrap only the existing SetDisplayMode, CPU label buffer/write/text, and recurring repaint() in `if (m_renderTestUi)`. Keep startTimer(1000 / 60) unchanged. No new thread instrumentation in this experiment.
- [x] Run git diff --check. Inspect all four launch combinations for complete audio output writes, exactly one selected processing path, and continuous diagnostic logging. Build using:

```bash
xcodebuild -project JUCE/SmartGridOne/Builds/iOS/SmartGridOne.xcodeproj -scheme 'SmartGridOne - App' -configuration Release -destination 'generic/platform=iOS' -derivedDataPath /private/tmp/smartgrid-ui-isolation-ios-build -allowProvisioningUpdates build
```

Capture the full build output in /private/tmp/smartgrid-ui-isolation-ios-build.log, verify BUILD SUCCEEDED and report actual product path. Use sandbox escalation for signing/build/worktree writes when required. Do not remove old artifacts or rebuild other configurations.
- [x] Self-review the actual delta against /private/tmp/smartgrid-ui-isolation-before/, write the implementation report with exact build command and outcome, and return for independent review. Leave edits uncommitted and do not deploy.

### Task 2: Optional automatic transport start for unattended UI comparison

The user identified the initially stopped transport and explicitly allowed starting in the running state. This small diagnostic extension lets the parent start identical normal UI-on and UI-off runs remotely. The original Task 1 behavior remains the default.

**Files:** Modify only JUCE/SmartGridOne/Source/MainComponent.h and MainComponent.cpp, this task's scratch report, and checkboxes for Task 2. All other Global Constraints continue to apply. Baseline files are /private/tmp/smartgrid-autoplay-before-20260910/MainComponent.h and MainComponent.cpp. No commits or deployment. Existing Task 1 is already implemented and reviewed; do not redo it.

- [x] Add an immutable member beside the existing two launch flags:

```cpp
const bool m_startTestPlayback = juce::SystemStats::getEnvironmentVariable("SMARTGRID_AUDIO_TEST_PLAY", "0") == "1";
```

- [x] Extend the existing startup `Audio experiment` log to include `transport_autostart=%d`, with effective value `!m_useTestTone && m_startTestPlayback`. Do not remove the existing audio_mode/ui_render fields.
- [x] Immediately after LoadConfig() and before OpenAudioDevice(), set the existing engine transport running only for requested normal-mode autostart:

```cpp
if (!m_useTestTone && m_startTestPlayback)
{
    m_nonagon.m_internal.m_running = true;
}
```

This write occurs before the audio device starts. No new audio-thread branch or recurring transport writes. Leave faders, patch loading, MIDI, clocks, input/output configuration and DSP unchanged. Do not persist the launch flag or transport state. The existing internal-clock saved config remains parent-owned.
- [x] Verify source ordering and that FromJSON does not reset m_running. Inspect missing flag/0/1 and tone/normal behavior. Run git diff --check. Use the existing signed Release build command and derived-data path from Task 1, with log /private/tmp/smartgrid-ui-isolation-autoplay-ios-build.log. Do not rebuild other configurations or add trivial unit tests that duplicate the branch. Report BUILD SUCCEEDED and actual product path; parent will verify signature/package and runtime.
- [x] Self-review the exact two-file delta against the saved baseline, write the task report, and return for independent review. Leave all modifications uncommitted.

## Parent integration checklist

- [x] Independent task review of prototype delta and build evidence; address findings.
- [x] Final review of integrated diagnostic change with prior code context; preserve uncommitted prototype state.
- [x] Verify wireless app log retrieval, launch/stop, install, and historical system-log collection where available.
- [x] Run unchanged installed tone plus Wi-Fi baseline before replacing it; record K-Mix channels 3/4 and score both failure symptoms.
- [x] Deploy the reviewed prototype and verify actual MAYA route, 48000 Hz, 512 frames, zero app inputs, resolved launch flags, callback timings, and normal processing.
- [x] Run normal audio/UI-on and normal audio/UI-off comparisons with consistent topology, patch and controller state, record both symptoms, retrieve logs between measured runs, and report results without treating a clean short run as proof of a fix.
