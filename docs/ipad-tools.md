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
file grows. Recordings copy to the Mac and extract the stereo master using the
SMRTGRID extractor or SoX for legacy WAV/RF64 files. They are removed from the iPad
only after a complete download, successful extraction, and stable-size check.

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

## Sync from the iPad over the LAN

Start the receiver on the Mac (Python 3.10 or newer; SoX is needed for legacy
WAV/RF64 recordings):

```bash
python3 scripts/sync_receiver.py
```

On the iPad, open **File > Sync with Mac** and tap the receiver. On first use,
compare the certificate fingerprint with the Mac terminal and enter the access
code printed there. The iPad remembers that pairing in Keychain. Use **Pair /
change access code** to correct a code. The Mac and iPad must share a local
network; allow Local Network access when prompted and incoming connections to
the receiver if macOS asks.

Sync copies missing patches both ways without overwriting them, refreshes larger
logs on the Mac, and sends completed recordings. The Mac retains the original
and automatically extracts its stereo master while later files transfer. Only a
verified successful extraction acknowledgement permits deleting that recording
from the iPad. Unfinished recordings are skipped and retained. An interrupted
file starts again on the next sync; complete matching Mac originals are reused.

**All iPad sync activity belongs to the foreground Sync page.** Leaving the page
stops discovery and resolving, cancels requests, closes the session, and joins
the transfer worker before returning to File. App inactivity does the same.
Returning to the app while the page remains open restarts discovery, but does
not restart a transfer. There are no background sessions, startup discovery,
audio callbacks, audio-engine hooks, or audio-thread polling for sync. The Cancel
button stops the transfer while leaving page discovery available.

The separately launched Mac receiver listens only while its command is running;
press Ctrl-C to stop it. Its default port is 47658, and Bonjour publishes the
port, so no address entry or LAN scan is needed. `--name`, `--port`, `--host`, and `--root`
can override the defaults. Receiver keys and identity are stored under
`~/.config/smartgridone/sync`, outside the repository. Keep that directory to
preserve pairing. HTTPS uses the pinned certificate and a private access code.

### LAN sync verification

```bash
scripts/.venv-ios/bin/python -m unittest discover -s scripts/tests -v
```

The native integration tests compile a Foundation-only client, exercise TLS
against temporary loopback receivers, and verify Bonjour discovery on the Mac. They cover full/repeated sync, certificate
mismatch, unfinished/corrupt recordings, cancellation during upload/download and
extraction, foreground loss, and rejection of transfers after Close. The receiver
suite also exercises real legacy WAV extraction and shutdown during an upload.

Physical-iPad Bonjour discovery, permission prompts, screen lock/background
transitions, and throughput still need verification with the iPad online. Compare
transfer MiB/s with `sync_ipad.py --transport wifi` on the same recording and
network before drawing conclusions about speed.
