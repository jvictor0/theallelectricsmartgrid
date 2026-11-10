"""Pinned, read-only decoding of iPad unified-log archives on Apple Silicon Macs."""

import hashlib
import json
import platform
import plistlib
import subprocess
import tarfile
import tempfile
from datetime import datetime
from pathlib import Path


VERSION = "0.7.0"
PACKAGE = f"unifiedlog_iterator-v{VERSION}-aarch64-apple-darwin"
RELEASE_URL = f"https://github.com/mandiant/macos-UnifiedLogs/releases/download/v{VERSION}/{PACKAGE}.tar.gz"
PACKAGE_SHA256 = "bf5c4a3418b133fbd403ca7893a2280acb8c2c3605b4ad6f87f9fc75b35a20b7"
READER_SHA256 = "6a2367d50f24e5013621bfd5493ce6a91757be5d3c307cc1e9fe179df8928c7f"
TOOLS_DIR = Path(__file__).resolve().parents[1] / "build" / "tools" / f"unifiedlogs-{VERSION}"


def verify_digest(path, expected):
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"SHA-256 mismatch for {path}: expected {expected}, got {actual}")


def ensure_reader():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise RuntimeError("The pinned iPad log reader requires an Apple Silicon Mac")
    reader = TOOLS_DIR / PACKAGE / "unifiedlog_iterator"
    if reader.exists():
        verify_digest(reader, READER_SHA256)
        return reader

    TOOLS_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=TOOLS_DIR) as directory:
        staging = Path(directory)
        package = staging / f"{PACKAGE}.tar.gz"
        print(f"Downloading checksum-pinned iPad log reader v{VERSION}", flush=True)
        subprocess.run([
            "curl", "--fail", "--location", "--silent", "--show-error",
            "--connect-timeout", "20", "--max-time", "120",
            RELEASE_URL, "--output", str(package),
        ], check=True)
        verify_digest(package, PACKAGE_SHA256)
        extracted = staging / PACKAGE
        extracted.mkdir()
        with tarfile.open(package, "r:gz") as archive:
            for name in ("unifiedlog_iterator", "LICENSE", "README.md"):
                member = archive.getmember(f"{PACKAGE}/{name}")
                if not member.isfile():
                    raise ValueError(f"Expected a regular release file: {member.name}")
                with archive.extractfile(member) as source:
                    (extracted / name).write_bytes(source.read())
        verify_digest(extracted / "unifiedlog_iterator", READER_SHA256)
        (extracted / "unifiedlog_iterator").chmod(0o755)
        extracted.rename(reader.parent)
    return reader


def summarize(path):
    count = 0
    losses = 0
    first = None
    last = None
    with path.open() as source:
        for line in source:
            record = json.loads(line)
            timestamp = record["timestamp"]
            if not timestamp.endswith("Z"):
                raise ValueError(f"Expected a UTC reader timestamp: {timestamp}")
            key = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
            if first is None or key < first[0]:
                first = (key, timestamp)
            if last is None or key > last[0]:
                last = (key, timestamp)
            count += 1
            losses += record.get("event_type") == "Loss"
    if count == 0:
        raise ValueError("Reader produced no records; archive coverage is unknown")
    return {"records": count, "first_timestamp": first[1], "last_timestamp": last[1], "loss_records": losses}


def decode_archive(archive, output):
    archive = archive.expanduser().resolve()
    output = output.expanduser().resolve()
    partial = output.with_name(output.name + ".partial")
    if archive.name.endswith(".partial"):
        raise ValueError("Refusing to decode a partial capture as a completed archive")
    with (archive / "Info.plist").open("rb") as source:
        info = plistlib.load(source)
    if output.is_relative_to(archive) or partial.is_relative_to(archive):
        raise ValueError("Decoded output must be outside the original archive")
    if output.exists() or partial.exists():
        raise FileExistsError(f"Output already exists: {output} or {partial}")
    reader = ensure_reader()
    partial.mkdir(parents=True)
    print(f"Decoding original archive into {partial}", flush=True)
    try:
        with (partial / "reader.stdout").open("w") as stdout, (partial / "reader.stderr").open("w") as stderr:
            subprocess.run([
                str(reader), "--mode", "log-archive", "--input", str(archive),
                "--format", "jsonl", "--output", str(partial / "records.jsonl"),
                "--bookmark-path", str(partial / "bookmark.json"),
            ], stdout=stdout, stderr=stderr, check=True)
        summary = summarize(partial / "records.jsonl")
        summary.update({
            "archive": str(archive), "archive_version": info.get("OSArchiveVersion"),
            "reader_version": VERSION, "reader_sha256": READER_SHA256,
            "release_url": RELEASE_URL, "package_sha256": PACKAGE_SHA256,
            "reader_stderr_bytes": (partial / "reader.stderr").stat().st_size,
        })
        (partial / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        partial.rename(output)
    except BaseException:
        print(f"Decode failed; retained incomplete output: {partial}", flush=True)
        raise
    print(f"Decoded {summary['records']} records: {summary['first_timestamp']} through {summary['last_timestamp']}")
    print(f"Loss records: {summary['loss_records']}; reader stderr bytes: {summary['reader_stderr_bytes']}")
    print(f"Decoded archive: {output}")
    return summary
