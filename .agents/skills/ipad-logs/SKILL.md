---
name: ipad-logs
description: Collect and analyze SmartGridOne app logs and iPad system diagnostics over Wi-Fi. Use for iPad audio, USB, crash, timing, thermal, storage, or deployment investigations in this repository.
---

# iPad logs

Use the repository's pinned environment and shared connector. Read
[references/log-taxonomy.md](references/log-taxonomy.md) before choosing evidence
or interpreting an incident.

## Working snapshot commands

Collect the latest persisted app log without changing device files:

```bash
scripts/.venv-ios/bin/python scripts/ipad_logs.py app
```

Collect a historical iPad unified-log archive:

```bash
scripts/.venv-ios/bin/python scripts/ipad_logs.py system --last 15m
```

Use a timezone-qualified start when an incident time is known:

```bash
scripts/.venv-ios/bin/python scripts/ipad_logs.py system --since 2026-09-15T12:00:00-07:00
```

Query a completed archive on the Mac:

```bash
/usr/bin/log show --archive CAPTURE.logarchive --style compact --info --debug
/usr/bin/log show --archive CAPTURE.logarchive --style compact --info --debug \
  --predicate 'process == "usbaudiod" OR process == "audiomxd" OR eventMessage CONTAINS[c] "MAYA"'
```

If collection fails, report the error and the `.partial` directory. Fix the
cause and start a fresh invocation. Do not repair, rename, or present a partial
capture as complete.

## Analysis rules

- Establish the first and last retained device timestamps before treating a
  requested window as covered.
- Preserve timezone offsets and correlate device event time; host receipt time
  measures transfer latency.
- Include app launch, audio route changes, and quit context around the symptom.
- Treat missing logs as missing evidence. Do not infer that an event did not occur.
- Keep routine app/system capture read-only. Do not run recording sync merely to
  collect logs.

The reference labels other evidence recipes by prerequisite and verification
status. Use them only when the incident needs that evidence; live streams,
crash pulls, stackshots, native traces, and external audio capture are not modes
of `ipad_logs.py`.
