import hashlib
import io
import json
import plistlib
import subprocess
import sys
import tarfile
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ipad_log_reader as reader


class ReaderTests(unittest.TestCase):
    def test_corrupt_cached_reader_is_never_executed_or_silently_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / reader.PACKAGE / "unifiedlog_iterator"
            binary.parent.mkdir()
            binary.write_bytes(b"corrupt")
            with patch.object(reader, "TOOLS_DIR", root), patch.object(reader.platform, "system", return_value="Darwin"), patch.object(reader.platform, "machine", return_value="arm64"), patch.object(reader.subprocess, "run") as run:
                with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                    reader.ensure_reader()
            run.assert_not_called()
            self.assertEqual(binary.read_bytes(), b"corrupt")

    def test_download_is_verified_before_install_and_cache_is_reused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            release = root / "release.tar.gz"
            with tarfile.open(release, "w:gz") as archive:
                for name in ("unifiedlog_iterator", "LICENSE", "README.md"):
                    data = name.encode()
                    member = tarfile.TarInfo(f"{reader.PACKAGE}/{name}")
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
            payload = release.read_bytes()

            def download(command, **kwargs):
                Path(command[-1]).write_bytes(payload)

            with patch.object(reader, "TOOLS_DIR", root / "tools"), patch.object(reader.platform, "system", return_value="Darwin"), patch.object(reader.platform, "machine", return_value="arm64"), patch.object(reader.subprocess, "run", side_effect=download) as run, redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                    reader.ensure_reader()
                self.assertFalse((reader.TOOLS_DIR / reader.PACKAGE).exists())
                with patch.object(reader, "PACKAGE_SHA256", hashlib.sha256(payload).hexdigest()), patch.object(reader, "READER_SHA256", hashlib.sha256(b"unifiedlog_iterator").hexdigest()):
                    binary = reader.ensure_reader()
                    self.assertEqual(reader.ensure_reader(), binary)
                    self.assertTrue(binary.stat().st_mode & 0o111)
                self.assertEqual(run.call_count, 2)

    def test_decode_preserves_archive_and_measures_unsorted_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "capture.logarchive"
            archive.mkdir()
            info = plistlib.dumps({"OSArchiveVersion": 6})
            (archive / "Info.plist").write_bytes(info)
            output = root / "decoded"
            records = [
                {"timestamp": "2026-11-10T07:14:00.250Z", "event_type": "Log"},
                {"timestamp": "2026-11-10T07:03:00Z", "event_type": "Loss"},
                {"timestamp": "2026-11-10T07:14:00Z", "event_type": "Log"},
            ]

            def decode(command, **kwargs):
                self.assertEqual(command[command.index("--input") + 1], str(archive.resolve()))
                destination = Path(command[command.index("--output") + 1])
                destination.write_text("".join(json.dumps(r) + "\n" for r in records))

            with patch.object(reader, "ensure_reader", return_value=root / "reader"), patch.object(reader.subprocess, "run", side_effect=decode), redirect_stdout(io.StringIO()):
                summary = reader.decode_archive(archive, output)
                with self.assertRaises(FileExistsError):
                    reader.decode_archive(archive, output)
                with self.assertRaisesRegex(ValueError, "outside the original archive"):
                    reader.decode_archive(archive, archive / "decoded")
            self.assertEqual(summary["first_timestamp"], records[1]["timestamp"])
            self.assertEqual(summary["last_timestamp"], records[0]["timestamp"])
            self.assertEqual(summary["loss_records"], 1)
            self.assertEqual((archive / "Info.plist").read_bytes(), info)
            self.assertEqual(list(archive.iterdir()), [archive / "Info.plist"])
            self.assertFalse(output.with_suffix(".partial").exists())

    def test_failed_or_empty_decode_retains_partial_and_never_publishes_success(self):
        for failure in (True, False):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / "capture.logarchive"
                archive.mkdir()
                (archive / "Info.plist").write_bytes(plistlib.dumps({"OSArchiveVersion": 6}))
                output = root / "decoded"

                def decode(command, **kwargs):
                    Path(command[command.index("--output") + 1]).write_text("")
                    if failure:
                        raise subprocess.CalledProcessError(1, command)

                with patch.object(reader, "ensure_reader", return_value=root / "reader"), patch.object(reader.subprocess, "run", side_effect=decode), redirect_stdout(io.StringIO()):
                    with self.assertRaises((subprocess.CalledProcessError, ValueError)):
                        reader.decode_archive(archive, output)
                self.assertFalse(output.exists())
                self.assertTrue((root / "decoded.partial" / "reader.stderr").exists())


if __name__ == "__main__":
    unittest.main()
