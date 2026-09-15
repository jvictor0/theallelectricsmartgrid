# iPad Wi-Fi tools

The repository can build, install, launch, sync, and collect logs from the paired
SmartGridOne iPad without a USB cable.

## One-time setup

1. Initialize the repository's pinned dependencies with
   `git submodule update --init --recursive`.
2. Connect the iPad by USB, unlock it, trust this Mac, and enable Wi-Fi device
   connections in Finder or Xcode.
3. Enable Developer Mode on the iPad.
4. Confirm Xcode signing for the `SmartGridOne - App` scheme. The project uses
   development team `84DCGNY7JA`.
5. Install SoX if recording sync will be used (`brew install sox`).
6. Create the pinned Python environment:

   ```bash
   make ios-setup
   ```

The known device defaults are UDID `00008122-001439681EA1001C` and host
`192.168.1.53`. If DHCP changes the address, set `IPAD_HOST`; set `IOS_DEVICE`
to target another paired device.

## Build and deploy

From the repository root:

```bash
make ios-deploy
```

This builds a signed Release app for generic iOS, upgrades the installed app
over Wi-Fi, and launches it. The iPad must be unlocked for launch. Each failure
stops the command; fix the reported cause and rerun it from the beginning.

Other targets are `ios-build`, `ios-install`, and the compatibility alias
`deploy-ios`. `ios-install` installs the existing build without launching it.

```bash
make ios-deploy IPAD_HOST=192.168.1.80 IOS_DEVICE=00008122-001439681EA1001C
```

## Sync

Sync chooses USB when the requested UDID is attached and Wi-Fi otherwise:

```bash
scripts/.venv-ios/bin/python scripts/sync_ipad.py
```

Use `--transport usb` or `--transport wifi` to force a transport. Patch files
copy in both directions. App logs copy from the iPad and refresh when the remote
file grows. Recordings copy to the Mac, pass through SoX stereo extraction, and
are removed from the iPad only after a complete download and stable-size check.

For a connectivity check that cannot transfer or delete a recording:

```bash
scripts/.venv-ios/bin/python scripts/sync_ipad.py --transport wifi --no-recordings
```

## Log snapshots

Copy the latest app log, or a named log relative to the app log directory:

```bash
scripts/.venv-ios/bin/python scripts/ipad_logs.py app
scripts/.venv-ios/bin/python scripts/ipad_logs.py app --name session.log
```

Collect recent historical iPad unified logs:

```bash
scripts/.venv-ios/bin/python scripts/ipad_logs.py system --last 15m
scripts/.venv-ios/bin/python scripts/ipad_logs.py system --since 2026-09-15T12:00:00-07:00
```

Completed captures go under `~/Documents/SmartGridOne/diagnostics`. A failed
capture retains a directory ending in `.partial`; a rerun creates a fresh one.
See the repository `ipad-logs` skill for archive queries and the full evidence
taxonomy.
