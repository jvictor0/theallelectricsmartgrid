# MIDI sender off experiment implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Prepare the user-selected next iPad experiment with the MIDI send worker disabled and its enqueue path discarding messages.

**Architecture:** Add an immutable launch option to MidiSender. Default behavior remains enabled; SMARTGRID_MIDI_SEND_THREAD=0 skips thread creation and drops messages before touching its queue. Low-rate diagnostics expose the chosen mode, successful entry to run, queue depth and discard count.

**Tech Stack:** Existing C++17/JUCE8.0.15 iOS diagnostic worktree, signed Xcode Release build, authenticated Wi-Fi device tools and K-Mix3/4 external capture.

## User-approved design and scope

The user explicitly selected disabling the MIDI send thread and discarding enqueue requests as the next experiment. This intervention removes both worker wakeups and the traffic sent by that worker; a changed outcome cannot distinguish those two effects by itself. Direct WB LED/SysEx sends from the audio path remain active. Normal DSP/UI, the I/O worker, tracing, inputs0/outputs4 and the permanent48k/512 baseline remain in place. Existing authorization permits building/deploying experiments; no reconfirmation is needed for this selected change.

The current SonoBus hour must finish and its archive be preserved before deploying or launching another app. Prepare source now; defer heavy build/device activity until capture completion. Preserve the old signed package for rollback. Development stays in the existing diagnostics worktree; no main changes, commits, landing or cleanup of unrelated artifacts.

## Global Constraints

- Keep the 48 kHz request/enforcement and 512-frame buffer request unchanged.
- Keep JUCE8.0.15, zero app inputs/four hardware outputs, normal DSP/UI and existing direct MIDI sends unchanged.
- SMARTGRID_MIDI_SEND_THREAD=0 means no MIDI sender thread creation and no queue writes; unset or1 preserves enabled mode.
- Disabled enqueue is bounded and non-blocking; count discarded messages with a relaxed atomic and return before route assignment/queue push.
- Do not add per-message logging. Emit aggregate status only in the existing once-per-second platform diagnostic branch.
- Follow repository naming/braces/comment conventions and preserve mixed line endings outside the exact edits.
- Do not query/change the iPad or launch heavy builds during the SonoBus capture.
- This is a reversible diagnostic prototype; do not add mirror/stub tests. Validate with source review, actual iOS build/signature and runtime worker/queue/audio evidence.

## Task 1: Implement, review and validate the launch option

**Files:**
- Modify JUCE/SmartGridOne/Source/MidiSender.hpp.
- Modify JUCE/SmartGridOne/Source/MainComponent.cpp only to call aggregate diagnostics in its existing one-second branch.
- Update this plan and docs/experiments/ipad-maya-audio with experiment identity and results.

**Interface and code contract:**

Add these public members to MidiSender near existing state:

```cpp
const bool m_sendThreadEnabled = juce::SystemStats::getEnvironmentVariable("SMARTGRID_MIDI_SEND_THREAD", "1") != "0";
std::atomic<bool> m_workerRunning{false};
std::atomic<size_t> m_discardedMessages{0};
```

After route initialization, before realtime options, return from the constructor when disabled and log the mode once. In enabled mode preserve the existing realtime options, capture startRealtimeThread's bool result and log it truthfully; add no fallback or scheduling adjustment. Set m_workerRunning true on entry to run and false on exit using relaxed atomics.

At the beginning of SendMessage:

```cpp
if (!m_sendThreadEnabled)
{
    m_discardedMessages.fetch_add(1, std::memory_order_relaxed);
    return;
}
```

Add a const LogDiagnostics method emitting one aggregate line with enabled, m_workerRunning, m_queue.Size and m_discardedMessages. Call it just after the cached platform-state stores in the existing one-second timer branch. Do not add a separate timer. Preserve route allocation, ProcessMessagesOut buffer clearing, all direct sends and shutdown behavior.

- [x] Inspect current source, queue producers and compiled dependency; record user-approved scope and validation gates.
- [x] Preserve exact pre-edit copies of both files and implement the two-file change.
- [x] Review the exact change independently for scope, thread safety and disabled-queue behavior.
- [x] After SonoBus completes and its logs are collected, build Release iOS using the existing8.0.15 derived-data cache and a new product directory; verify the signature and preserve package/hash metadata.
- [x] Deploy over the established authenticated Wi-Fi connection. Verify a brief enabled-mode start if needed to establish whether the worker actually starts; preserve that evidence. Then launch the selected disabled mode with SMARTGRID_AUDIO_TEST_MODE=normal, SMARTGRID_UI_RENDER=1, SMARTGRID_AUDIO_TEST_PLAY=1 and SMARTGRID_MIDI_SEND_THREAD=0.
- [x] Verify actual MAYA48k, settled512, app inputs0/outputs4, normal running transport/UI, enabled=0, running=0, queued=0 and increasing discarded count when messages are generated. An enabled baseline whose worker fails to start changes the interpretation and must be reported.
- [x] Record a separately identified exposure, collect its archive afterward and score both sporadic interruptions and periodic holes. Mark startup transitions separately; retain exact duration if stopped early.

## Review and interpretation gates

This prototype can be prepared without claiming it fixes audio. Source review and build are not a substitute for observing that the worker is absent and queue depth stays zero. A clean trial implicates the disabled path only if the control worker actually ran; continued periodic damage is still a failure of that symptom even if sporadic gaps disappear. Preserve the previous SonoBus outcome and unverified settings as distinct evidence. Do not reclassify any partial trial as a clean hour.
