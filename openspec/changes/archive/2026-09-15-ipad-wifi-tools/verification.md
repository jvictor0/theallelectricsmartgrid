# Verification

Verified on 2026-09-15 against iPad
`00008122-001439681EA1001C` at `192.168.1.53`.

## Automated and static checks

- `make ios-setup` completed with `pymobiledevice3==11.12.1` and is rerunnable.
- `python -m unittest discover -s scripts/tests -v`: 9 tests passed. Coverage
  includes exact-device transport selection, atomic complete downloads, short
  reads, extraction failure preserving the remote recording, growing-log refresh,
  bundle validation, and time parsing.
- `python -m py_compile` passed for all four iPad scripts.
- Root and JUCE `make -n` checks showed `ios-deploy` orders `ios-build` before
  install/launch, including with parallel Make execution. The deploy recipe cannot
  run when its build prerequisite fails.
- Skill metadata and local links passed `quick_validate.py`.
- `git diff --check` passed. A source scan found no Rack Make variables/includes
  and no retry, resume, receipt, hash, manifest, rollback, saved-preference, or
  custom-discovery implementation.

## Live checks

- Direct Wi-Fi pairing, UDID validation, and Marco/Polo heartbeat succeeded on
  iPadOS 26.6.1.
- `ipad_logs.py app` copied a 4,838-byte persisted app log over Wi-Fi.
- `ipad_logs.py system --last 1m` completed with the 64 MiB collection limit;
  `/usr/bin/log show --archive` opened the result and returned `usbaudiod` and
  `audiomxd` events. The extracted Apple archive was about 204 MiB, demonstrating
  why analysis must inspect actual coverage rather than infer it from limits.
- A deliberate connection failure exited nonzero and retained only the visibly
  named `.partial` capture directory.
- `sync_ipad.py --transport wifi --no-recordings` completed patch and app-log
  sync without transferring or deleting recordings.
- `make ios-build` produced and signed the expected Release app.
- `make ios-deploy` completed the incremental build, Wi-Fi upgrade, and launch.
  A CoreDevice process query found the launched executable at PID 63967. A log
  captured before the upgrade remained readable afterward, confirming preserved
  app-container data.

## Unavailable live checks

- USB sync was not smoke-tested because usbmux reported no attached devices.
  Automatic selection and exact-UDID USB matching are covered by the focused test.
- The locked-device launch failure was not induced because the supplied iPad was
  unlocked. The deploy helper wraps a post-install launch exception as
  `Installed, but could not launch: ...` and exits nonzero.
