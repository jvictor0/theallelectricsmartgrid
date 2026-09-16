import asyncio
import io
import os
import plistlib
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

import ipad_device
import ipad_logs
import ios_deploy
import sync_ipad


class FakeAfc:
    def __init__(self, sizes, chunks):
        self.sizes = list(sizes)
        self.chunks = list(chunks)
        self.removed = []
        self.closed = False

    async def stat(self, path):
        return {"st_ifmt": "S_IFREG", "st_size": self.sizes.pop(0)}

    async def fopen(self, path, mode):
        return 7

    async def fread(self, handle, size):
        return self.chunks.pop(0)

    async def fclose(self, handle):
        self.closed = True

    async def rm(self, path):
        self.removed.append(path)


class StalledAfc(FakeAfc):
    async def fread(self, handle, size):
        await asyncio.Event().wait()


class DeviceSelectionTests(unittest.IsolatedAsyncioTestCase):
    def test_environment_overrides_known_device_defaults(self):
        with patch.dict(
            os.environ,
            {"IOS_DEVICE": "override-udid", "IPAD_HOST": "10.0.0.9"},
            clear=True,
        ):
            self.assertEqual(ipad_device.device_udid(), "override-udid")
            self.assertEqual(ipad_device.device_host(), "10.0.0.9")

    def test_auto_transport_uses_usb_only_for_requested_udid(self):
        devices = [
            SimpleNamespace(serial="other", is_usb=True),
            SimpleNamespace(serial="wanted", is_usb=False),
        ]
        self.assertEqual(ipad_device.choose_transport("auto", devices, "wanted"), "wifi")

        devices.append(SimpleNamespace(serial="wanted", is_usb=True))
        self.assertEqual(ipad_device.choose_transport("auto", devices, "wanted"), "usb")


class TransferTests(unittest.IsolatedAsyncioTestCase):
    async def test_large_download_reports_incremental_progress(self):
        afc = FakeAfc([5], [b"ab", b"cd", b"e"])
        output = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "recording.sgrec"

            with patch.object(sync_ipad, "PROGRESS_REPORT_BYTES", 2):
                with redirect_stdout(output):
                    await sync_ipad.download_afc_file(
                        afc,
                        "/recording.sgrec",
                        destination,
                    )

        progress = output.getvalue()
        self.assertIn("40.0%", progress)
        self.assertIn("80.0%", progress)
        self.assertIn("100.0%", progress)

    async def test_stalled_read_times_out_and_removes_partial(self):
        afc = StalledAfc([5], [])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "recording.sgrec"

            with patch.object(sync_ipad, "AFC_READ_TIMEOUT", 0.001):
                with self.assertRaisesRegex(TimeoutError, "timed out reading"):
                    await asyncio.wait_for(
                        sync_ipad.download_afc_file(
                            afc,
                            "/recording.sgrec",
                            destination,
                        ),
                        timeout=0.1,
                    )

            self.assertFalse((Path(directory) / ".recording.sgrec.partial").exists())
            self.assertTrue(afc.closed)

    async def test_short_read_keeps_existing_file_and_removes_partial(self):
        afc = FakeAfc([5], [b"ab", b""])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "session.log"
            destination.write_bytes(b"old")

            with self.assertRaisesRegex(IOError, "short read"):
                await sync_ipad.download_afc_file(afc, "/session.log", destination)

            self.assertEqual(destination.read_bytes(), b"old")
            self.assertFalse((Path(directory) / ".session.log.partial").exists())
            self.assertTrue(afc.closed)

    async def test_complete_download_atomically_replaces_destination(self):
        afc = FakeAfc([5], [b"ab", b"cde"])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "session.log"
            destination.write_bytes(b"old")

            await sync_ipad.download_afc_file(afc, "/session.log", destination)

            self.assertEqual(destination.read_bytes(), b"abcde")
            self.assertFalse((Path(directory) / ".session.log.partial").exists())

    async def test_failed_extraction_retains_remote_recording(self):
        afc = FakeAfc([4, 4], [b"quad"])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "take.wav"

            def fail_extraction(path):
                raise RuntimeError("sox failed")

            with self.assertRaisesRegex(RuntimeError, "sox failed"):
                await sync_ipad.sync_recording(
                    afc,
                    "/take.wav",
                    destination,
                    fail_extraction,
                )

            self.assertEqual(afc.removed, [])
            self.assertEqual(destination.read_bytes(), b"quad")

    async def test_growing_log_replaces_stale_snapshot(self):
        afc = FakeAfc([4], [b"new!"])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "session.log"
            destination.write_bytes(b"old")

            changed = await sync_ipad.sync_log_file(
                afc,
                "/session.log",
                destination,
            )

            self.assertTrue(changed)
            self.assertEqual(destination.read_bytes(), b"new!")


class DeployTests(unittest.TestCase):
    def test_app_bundle_identifier_must_match_expected_app(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / "SmartGridOne.app"
            app.mkdir()
            with (app / "Info.plist").open("wb") as output:
                plistlib.dump({"CFBundleIdentifier": "wrong.bundle"}, output)

            with self.assertRaisesRegex(ValueError, "wrong.bundle"):
                ios_deploy.validate_app(app)


class LogWindowTests(unittest.TestCase):
    def test_relative_window_supports_minutes_and_hours(self):
        self.assertEqual(ipad_logs.parse_duration("15m"), 900)
        self.assertEqual(ipad_logs.parse_duration("2h"), 7200)

    def test_since_requires_timezone(self):
        with self.assertRaisesRegex(ValueError, "timezone"):
            ipad_logs.parse_since("2026-09-15T12:00:00")


class CommandTests(unittest.TestCase):
    def test_keyboard_interrupt_exits_cleanly(self):
        def interrupt(coroutine):
            coroutine.close()
            raise KeyboardInterrupt

        errors = io.StringIO()
        output = io.StringIO()
        with patch.object(sys, "argv", ["sync_ipad.py"]):
            with patch.object(sync_ipad.asyncio, "run", side_effect=interrupt):
                with redirect_stdout(output):
                    with redirect_stderr(errors):
                        result = sync_ipad.main()

        self.assertEqual(result, 130)
        self.assertIn("Interrupted", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
