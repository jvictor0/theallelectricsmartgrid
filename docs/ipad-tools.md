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
6. The deploy and install targets create the pinned Python environment
   automatically when it is missing. To prepare it separately for sync or logs:

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
All three use `build/xcode/ios/Release/Products/SmartGridOne.app` by default;
`CONFIG=Debug` selects the separate Debug directory. Build caches and products
stay in this checkout. See [building.md](building.md) for incremental builds,
configuration selection, and cleanup.

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

## Experimental journal

The [iPad / MAYA44 investigation journal](experiments/ipad-maya-audio/README.md)
is versioned in this repository. Add dated results there and update its index
and experiment queue. The old diagnostic worktree is historical context, not
the journal's storage location. Large raw device captures remain under
`~/Documents/SmartGridOne/diagnostics` and are linked from the entries.

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
audio callbacks, audio-engine hooks, or audio-thread polling for sync.
**Cancel sync** stops the transfer while leaving discovery and publishing
controls available.

The separately launched Mac receiver listens only while its command is running;
press Ctrl-C to stop it. Its default port is 47658, and Bonjour publishes the
port, so no address entry or LAN scan is needed. `--name`, `--port`, `--host`,
`--root`, and `--identity-dir` can override the defaults. Receiver keys and
identity are stored under
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

Physical-iPad discovery, pairing, and full sync were verified on 2026-09-23:
4.12 GB of recordings arrived, both stereo files were extracted, logs synced,
and verified iPad originals were removed. Bulk transfer averaged about 6.6 MB/s
on that run; TCP retransmissions caused some pauses. The normal app made no sync
connections during a 20-second launch observation. Physical screen-lock and
background transitions still need verification. Compare transfer MiB/s with
`sync_ipad.py --transport wifi` on the same recording and network before drawing
conclusions about relative speed.

## Publish a Mac recording to SoundCloud from the iPad

### Connect the account once on the Mac

Register a SoundCloud application for the account you intend to publish from.
Its registered redirect must exactly match the setup callback; the default is
`http://127.0.0.1:47659/callback`. From the repository root on the Mac, run:

```bash
python3 scripts/soundcloud_connect.py
```

The command privately prompts for the application's client ID and client secret,
then asks for the registered callback URL. Press Return to use the default.
Complete the browser consent using the intended SoundCloud account. Setup uses
PKCE and state, verifies the account, and reports that it is connected in Mac
Keychain. Credentials and rotating tokens are stored under service
`com.theallelectricsmartgrid.soundcloud`, account `publisher`; they are not stored
with recordings or in the repository. The setup command has no credential flags.

Start, or restart, the receiver after connecting:

```bash
python3 scripts/sync_receiver.py
```

The receiver loads authorization at startup. Keep the Mac awake and this command
running. The default archive is `~/Documents/SmartGridOne`; `--root` chooses a
different archive. Its `.soundcloud.sqlite3` stores non-secret catalog data, jobs,
and receipts. Preserve it to retain upload history. Receiver pairing still uses
its separate identity directory and access code described above.

No SoundCloud login is added on the iPad. Its existing pinned receiver pairing
allows publication to the account configured on that Mac. The panel message
**Connect SoundCloud on the Mac receiver to enable publishing. Sync is available.**
means setup is missing or authorization needs repair: run the connection command
again and restart the receiver. An older receiver shows an
update message while ordinary sync remains available.

### Choose a recording and upload

1. Open **File > Sync with Mac** and select the Mac. Pair with its fingerprint
   and access code if needed. Selecting it starts normal sync and loads the
   publishing panel independently; existing stereo masters can be selected
   before sync finishes. Check the displayed SoundCloud account.
2. Choose a stereo recording from the newest-first list. Only completed
   `*_stereo.wav` exports appear. The selected details show its filename and
   default title; recording date and duration appear in the list.
3. Leave the title automatic, or enter a custom title. **Use default** restores
   automatic naming. The default is `Smart Grid Nightly YYYY/MM/DD`, using the
   recording's stored local start date. The first known post for a date has no
   suffix; subsequent defaults begin at ` #2`. Existing posts and pending jobs
   affect the number, which the Mac assigns when it accepts the upload. Custom
   titles are trimmed and receive no automatic suffix. Older numbered files use
   SGREC start metadata or a stored modification-time fallback; edit the title
   if that displayed date is unsuitable.
4. Artwork is empty by default. Optionally tap **Choose image (Files)** or
   **Photos** and select an image in the system picker. Only the selected image
   is read; Photos does not require access to the whole library. The app
   normalizes the image and shows a thumbnail. **Remove image** clears it;
   cancelling the picker preserves the draft. No artwork is inherited from a
   previous recording.
5. Tap **Upload** once. Publication is immediate, with genre **Electronic** and
   **public** visibility. No description or tags are added. The row shows Queued,
   Uploading, then Uploaded. The posted title and link replace the draft, and
   **Open on SoundCloud** opens the receipt's link. Uploaded means SoundCloud
   accepted creation; its processing may continue afterward.

**Refresh catalog** rescans local stereo exports and refreshes the configured
account's track metadata, including tracks uploaded manually elsewhere. Ordinary
status polling reads the Mac's saved state. If the first account listing fails,
automatic naming waits for a successful refresh; a complete cached listing can
continue to supply defaults after a later refresh failure.

Cloud-only recordings are skipped without downloading them during catalog scans.
Download a recording on the Mac first, then use **Refresh catalog** to include it.

### Link a historical upload or recover a result

For a recording already published manually, select its track in **Select an
existing account track** and tap **Link existing**. The receiver can suggest a
unique nightly date and duration match; verify the selected track before
explicitly linking it. Custom titles and ambiguous matches can be selected from
the same account-track list. Linking records Uploaded for this file version and
account without sending audio again. Use **Refresh catalog** if the track is
missing from the list.

A definite rejection shows its error and **Retry**. Fix the cause before retrying.
If the iPad loses the receiver's acceptance response, it retains the request
identity and checks the Mac's status. **Retry same request** uses that same
identity to recover the accepted job. Resolve this pending upload before
uploading another recording; other drafts can still be edited.

An **unknown** result means SoundCloud may already have accepted the Mac's
request. Tap **Open SoundCloud account**, inspect the account, and refresh/link
an existing track if it was created. Only use **Upload again** after checking:
that explicit new attempt may create a duplicate. Unknown results never trigger
an automatic second creation request.

### Leaving the page versus stopping the receiver

**Cancel sync** stops only iPad file transfer. Leaving the Sync page or making
the app inactive stops iPad discovery, requests, and polling; an upload already
accepted by the Mac continues there. Reopen the page and choose the Mac to see
its saved job or receipt. Foreground return to an already-open page reconnects
after discovery without automatically restarting file transfer or submitting
another upload.

Stopping the receiver with Ctrl-C, sleeping the Mac, or interrupting its network
can interrupt the outgoing publication. On receiver restart, queued jobs resume,
completed receipts remain Uploaded, and jobs interrupted while Uploading become
Unknown for explicit recovery. The original and stereo recordings remain on the
Mac regardless of publication outcome.

### Publishing verification and remaining setup

On 2026-10-11, both macOS and iPad simulator builds succeeded. Focused tests used
real local HTTPS receivers, temporary stereo files, and a fake external account.
Automated tests covered independent bulk/control requests, close/inactivity,
recovery of a lost acceptance response without another creation, historical linking, metadata,
artwork, restart persistence, and the existing sync/extraction regressions.

The iPad Air 13-inch (M3), iOS 18.5 simulator walkthrough could not reach the
Sync page. An existing topology cell construction crash was fixed and both
Apple targets rebuilt successfully; launch then aborted in the simulator audio
service (`_ReportRPCTimeout` / `AURemoteIO::Start`) even after one simulator
restart. No interactive recording selection, title/image editing, native picker,
or leaving/reopening check was completed. These checks remain pending.

The paired iPad Air 13-inch (M3) was unavailable in `xcrun devicectl list devices`
on 2026-10-11. A physical-device walkthrough of selection, image picking,
leaving/reopening during a Mac job, and foreground transitions remains pending.
The user's SoundCloud application registration, browser consent, and real
Keychain access also remain user-run setup; no real account was connected and no
real track was published during verification.
