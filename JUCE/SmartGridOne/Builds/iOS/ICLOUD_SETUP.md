# iCloud Sync Setup (NO PAID ACCOUNT NEEDED!)

Good news: **You don't need a paid developer account!** The app uses your regular Documents folder, which automatically syncs via iCloud Drive if you have it enabled.

## How It Works

The app saves patches to your Documents folder (`~/Documents/SmartGridOne/` on Mac, or the app's Documents on iOS). If you have iCloud Drive enabled with "Desktop and Documents" (Mac) or iCloud Drive (iOS), this folder will automatically sync between your devices.

## Setup (Free - No Paid Account!)

### On macOS:
1. Open **System Settings** → **Apple ID** → **iCloud**
2. Make sure **iCloud Drive** is enabled
3. Enable **Desktop and Documents Folders** (this syncs your Documents folder to iCloud)
4. That's it! Your patches will sync automatically.

### On iOS:
1. Open **Settings** → **[Your Name]** → **iCloud**
2. Make sure **iCloud Drive** is enabled
3. The app's Documents folder will sync automatically

## Verify It's Working

1. Save a patch on one device
2. Wait a few seconds for iCloud to sync
3. Check the other device - the patch should appear

## Troubleshooting

- **Patches not syncing?** Make sure both devices are signed into the same iCloud account
- **Still not working?** Check that iCloud Drive has enough storage space
- **On Mac:** Make sure "Desktop and Documents Folders" is enabled in iCloud settings

## Note

The path might show as `/Users/you/Documents/SmartGridOne/` instead of an "iCloud" path - that's normal! It's still syncing via iCloud Drive if you have the settings enabled above.
