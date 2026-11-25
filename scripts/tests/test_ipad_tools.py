import asyncio
import io
import os
import plistlib
import sys
import tempfile
import unittest
from contextlib import asynccontextmanager, redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

import ipad_device
import ipad_logs
import ipad_perf
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
    async def test_retry_reuses_complete_local_recording(self):
        afc = FakeAfc([4], [])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "recording.sgrec"
            destination.write_bytes(b"quad")

            with redirect_stdout(io.StringIO()):
                transfer = await sync_ipad.download_recording(
                    afc,
                    "/recording.sgrec",
                    destination,
                )

        self.assertEqual(transfer.expected_size, 4)
        self.assertEqual(afc.removed, [])
        self.assertFalse(afc.closed)

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
        afc = FakeAfc([4], [b"quad"])
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "take.wav"
            transfer = await sync_ipad.download_recording(
                afc,
                "/take.wav",
                destination,
            )

            def fail_extraction(path):
                raise RuntimeError("sox failed")

            with redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, "sox failed"):
                    sync_ipad.extract_recording(transfer, fail_extraction)

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


class RecordingGuardTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.lockdown = object()

    @asynccontextmanager
    async def connect(self, transport, udid, host):
        yield self.lockdown

    async def test_busy_preflight_uses_one_file_lookup_and_tells_agent_to_wait(self):
        container = MagicMock()
        exists = container.__aenter__.return_value.exists = AsyncMock(return_value=True)
        with patch.object(ipad_device.HouseArrestService, "create", return_value=container) as create:
            with self.assertRaisesRegex(ipad_device.IpadRecordingGuardError,
                                        r"Continue other work\. Do not attempt the iPad again until a human"):
                await ipad_device.require_ipad_idle(self.lockdown)
        create.assert_awaited_once_with(self.lockdown, ipad_device.APP_BUNDLE_ID, documents_only=False)
        exists.assert_awaited_once_with("/Documents/SmartGridOne/recording-active")

    async def test_app_absent_allows_first_install_but_a_failed_check_blocks(self):
        with patch.object(ipad_device.HouseArrestService, "create", side_effect=ipad_device.AppNotInstalledError):
            await ipad_device.require_ipad_idle(self.lockdown)

        with patch.object(ipad_device.HouseArrestService, "create", side_effect=OSError("unavailable")):
            with self.assertRaisesRegex(ipad_device.IpadRecordingGuardError, "until a human"):
                await ipad_device.require_ipad_idle(self.lockdown)

    async def test_main_app_checks_before_install_and_rechecks_before_launch(self):
        busy = ipad_device.IpadRecordingGuardError("recording")
        for results, expected_installs in [([busy], 0), ([None, busy], 1)]:
            with self.subTest(expected_installs=expected_installs):
                with patch.object(ios_deploy, "connect_ipad", self.connect), \
                     patch.object(ios_deploy, "validate_app"), \
                     patch.object(ios_deploy, "require_ipad_idle", side_effect=results) as check, \
                     patch.object(ios_deploy, "InstallationProxyService") as installer, \
                     patch.object(ios_deploy, "connect_developer") as launch, \
                     redirect_stdout(io.StringIO()):
                    install = installer.return_value.__aenter__.return_value
                    install.install_from_local = AsyncMock()
                    with self.assertRaisesRegex(RuntimeError, "recording"):
                        await ios_deploy.deploy(Path("SmartGridOne.app"), "ipad", "host", False)
                    self.assertEqual(install.install_from_local.await_count, expected_installs)
                    self.assertEqual(check.await_count, len(results))
                    launch.assert_not_called()

    async def test_perf_blocks_install_and_skip_install_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory)
            with (app / "Info.plist").open("wb") as output:
                plistlib.dump({"CFBundleIdentifier": ipad_perf.BUNDLE_ID}, output)

            for skip_install in (False, True):
                with self.subTest(skip_install=skip_install):
                    args = SimpleNamespace(udid="ipad", host="host", app=app, skip_install=skip_install)
                    with patch.object(ipad_device, "connect_ipad", self.connect), \
                         patch.object(ipad_device, "require_ipad_idle", side_effect=ipad_device.IpadRecordingGuardError) as check, \
                         patch.object(ipad_device, "connect_developer") as launch, \
                         patch("pymobiledevice3.services.installation_proxy.InstallationProxyService") as installer, \
                         patch.object(ipad_perf.subprocess, "run"):
                        with self.assertRaises(ipad_device.IpadRecordingGuardError):
                            await ipad_perf.run_device(args, {})
                        check.assert_awaited_once_with(self.lockdown)
                        installer.assert_not_called()
                        launch.assert_not_called()


class LogWindowTests(unittest.TestCase):
    def test_relative_window_supports_minutes_and_hours(self):
        self.assertEqual(ipad_logs.parse_duration("15m"), 900)
        self.assertEqual(ipad_logs.parse_duration("2h"), 7200)

    def test_since_requires_timezone(self):
        with self.assertRaisesRegex(ValueError, "timezone"):
            ipad_logs.parse_since("2026-09-15T12:00:00")


class CommandTests(unittest.TestCase):
    def test_sync_downloads_then_extracts_then_reconnects_to_delete(self):
        events = []
        transfer = SimpleNamespace()

        async def download_phase(*args, **kwargs):
            events.append("download-and-close")
            return [transfer]

        def extract_phase(value):
            self.assertIs(value, transfer)
            events.append("extract-locally")

        async def delete_phase(*args, **kwargs):
            self.assertEqual(args[-1], [transfer])
            events.append("reconnect-and-delete")

        with patch.object(sync_ipad, "sync", side_effect=download_phase):
            with patch.object(
                sync_ipad,
                "extract_recording",
                side_effect=extract_phase,
                create=True,
            ):
                with patch.object(
                    sync_ipad,
                    "delete_recordings_from_ipad",
                    side_effect=delete_phase,
                    create=True,
                ):
                    sync_ipad.sync_all("wifi", "device", "host")

        self.assertEqual(
            events,
            ["download-and-close", "extract-locally", "reconnect-and-delete"],
        )

    def test_keyboard_interrupt_exits_cleanly(self):
        def interrupt(*args, **kwargs):
            raise KeyboardInterrupt

        errors = io.StringIO()
        output = io.StringIO()
        with patch.object(sys, "argv", ["sync_ipad.py"]):
            with patch.object(sync_ipad, "sync_all", side_effect=interrupt):
                with redirect_stdout(output):
                    with redirect_stderr(errors):
                        result = sync_ipad.main()

        self.assertEqual(result, 130)
        self.assertIn("Interrupted", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
