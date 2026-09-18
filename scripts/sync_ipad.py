#!/usr/bin/env python3

import argparse
import asyncio
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from pymobiledevice3.services.house_arrest import HouseArrestService

from extract_recording import extract
from ipad_device import connect_ipad, device_host, device_udid
from sgrec import x_magic


APP_BUNDLE_ID = "com.theallelectricsmartgrid.smartgridone"
MAC_SMARTGRID_DIR = Path.home() / "Documents" / "SmartGridOne"
MAC_PATCHES_DIR = MAC_SMARTGRID_DIR / "patches"
MAC_RECORDINGS_DIR = MAC_SMARTGRID_DIR / "recordings"
MAC_LOGS_DIR = MAC_SMARTGRID_DIR / "logs"
IPAD_DOCUMENTS_DIR = "/Documents/SmartGridOne"
DOWNLOAD_CHUNK_SIZE = 4 * 1024 * 1024
AFC_READ_TIMEOUT = 30
PROGRESS_REPORT_BYTES = 64 * 1024 * 1024


@dataclass(frozen=True)
class RecordingTransfer:
    remote_path: str
    local_path: Path
    expected_size: int


def extract_stereo_recording(local_path: Path) -> int:
    with local_path.open("rb") as source:
        magic = source.read(12)
    if magic[:8] == x_magic:
        result = extract(local_path, master="stereo", overwrite=True)
        print(f"  Extracted {result.m_frames} frames: {result.m_output.name}")
        if not result.m_complete:
            print(f"  Incomplete recording: {result.m_error}")
            return 1
        return 0
    if magic[:4] not in (b"RIFF", b"RF64") or magic[8:12] != b"WAVE":
        raise ValueError(f"{local_path} is not a SmartGrid or RIFF/RF64 recording")

    channel_count = int(
        subprocess.check_output(["soxi", "-c", str(local_path)], text=True).strip()
    )
    if channel_count < 2:
        raise ValueError(f"{local_path} has only {channel_count} channel(s)")
    stereo_path = local_path.with_stem(local_path.stem + "_stereo")
    subprocess.run(
        [
            "sox",
            str(local_path),
            str(stereo_path),
            "remix",
            str(channel_count - 1),
            str(channel_count),
        ],
        check=True,
    )
    return 0


async def download_afc_file(
    afc,
    remote_path,
    local_path,
    progress_label="Copying",
    expected_size=None,
):
    if expected_size is None:
        info = await afc.stat(remote_path)
        if info.get("st_ifmt") != "S_IFREG":
            raise ValueError(f"{remote_path} is not a regular file")
        expected_size = int(info["st_size"])

    local_path.parent.mkdir(parents=True, exist_ok=True)
    partial_path = local_path.with_name(f".{local_path.name}.partial")
    partial_path.unlink(missing_ok=True)
    handle = await afc.fopen(remote_path, "r")
    total = 0
    show_progress = expected_size >= PROGRESS_REPORT_BYTES
    next_progress = PROGRESS_REPORT_BYTES
    if show_progress:
        print(
            f"  {progress_label}: 0.0 / {expected_size / (1024 * 1024):.1f} MiB (0.0%)",
            flush=True,
        )
    try:
        with partial_path.open("wb") as output:
            while total < expected_size:
                requested = min(DOWNLOAD_CHUNK_SIZE, expected_size - total)
                try:
                    chunk = await asyncio.wait_for(
                        afc.fread(handle, requested),
                        timeout=AFC_READ_TIMEOUT,
                    )
                except TimeoutError as error:
                    raise TimeoutError(
                        f"timed out reading {remote_path} after {AFC_READ_TIMEOUT} seconds "
                        f"at {total} of {expected_size} bytes"
                    ) from error
                if not chunk:
                    raise IOError(
                        f"short read for {remote_path}: received {total} of {expected_size} bytes"
                    )
                output.write(chunk)
                total += len(chunk)
                if total > expected_size:
                    raise IOError(
                        f"oversized read for {remote_path}: received more than {expected_size} bytes"
                    )
                if show_progress and (total >= next_progress or total == expected_size):
                    print(
                        f"  {progress_label}: {total / (1024 * 1024):.1f} / "
                        f"{expected_size / (1024 * 1024):.1f} MiB "
                        f"({100 * total / expected_size:.1f}%)",
                        flush=True,
                    )
                    while next_progress <= total:
                        next_progress += PROGRESS_REPORT_BYTES
        partial_path.replace(local_path)
    except BaseException:
        partial_path.unlink(missing_ok=True)
        raise
    finally:
        await afc.fclose(handle)

    if expected_size >= 1024 * 1024 and not show_progress:
        print(f"  {progress_label}: {expected_size / (1024 * 1024):.1f} MiB done")


async def ensure_remote_dir(afc, path):
    if not await afc.exists(path):
        await afc.makedirs(path)


async def list_files_recursive(afc, root):
    entries = []
    async for directory, directories, files in afc.walk(root):
        directory_path = PurePosixPath(directory)
        for name in directories:
            full_path = directory_path / name
            entries.append((str(full_path.relative_to(root)), True, str(full_path)))
        for name in files:
            full_path = directory_path / name
            entries.append((str(full_path.relative_to(root)), False, str(full_path)))
    return entries


async def sync_patches_to_ipad(afc, remote_root):
    if not MAC_PATCHES_DIR.exists():
        return
    await ensure_remote_dir(afc, remote_root)
    remote_entries = {
        relative for relative, _is_directory, _full in await list_files_recursive(afc, remote_root)
    }
    for local_path in sorted(MAC_PATCHES_DIR.rglob("*")):
        relative = local_path.relative_to(MAC_PATCHES_DIR).as_posix()
        if relative in remote_entries:
            continue
        remote_path = f"{remote_root}/{relative}"
        if local_path.is_dir():
            print(f"Creating directory on iPad: {relative}")
            await ensure_remote_dir(afc, remote_path)
        else:
            print(f"Copying to iPad: {relative}")
            await ensure_remote_dir(afc, str(PurePosixPath(remote_path).parent))
            await afc.set_file_contents(remote_path, local_path.read_bytes())


async def sync_patches_from_ipad(afc, remote_root):
    MAC_PATCHES_DIR.mkdir(parents=True, exist_ok=True)
    for relative, is_directory, remote_path in await list_files_recursive(afc, remote_root):
        local_path = MAC_PATCHES_DIR / relative
        if local_path.exists():
            continue
        if is_directory:
            print(f"Creating directory on Mac: {relative}")
            local_path.mkdir(parents=True, exist_ok=True)
        else:
            print(f"Copying from iPad: {relative}")
            await download_afc_file(afc, remote_path, local_path)


async def download_recording(afc, remote_path, local_path):
    first = await afc.stat(remote_path)
    expected_size = int(first["st_size"])
    if local_path.exists() and local_path.stat().st_size == expected_size:
        print(f"  Reusing complete local recording: {local_path.name}")
    else:
        await download_afc_file(
            afc,
            remote_path,
            local_path,
            progress_label="Copied",
            expected_size=expected_size,
        )
    return RecordingTransfer(remote_path, local_path, expected_size)


def extract_recording(transfer, extractor=extract_stereo_recording):
    print(f"  Extracting stereo locally: {transfer.local_path.name}", flush=True)
    if extractor(transfer.local_path) != 0:
        raise RuntimeError("Extraction failed; keeping the recording on iPad")


async def delete_recording(afc, transfer):
    current_size = int((await afc.stat(transfer.remote_path))["st_size"])
    if current_size != transfer.expected_size:
        raise IOError(
            f"{transfer.remote_path} changed size during transfer "
            f"({transfer.expected_size} to {current_size}); retained on iPad"
        )
    print(f"  Deleting from iPad: {transfer.local_path.name}")
    await afc.rm(transfer.remote_path)


async def download_recordings_from_ipad(afc, remote_root):
    MAC_RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
    transfers = []
    entries = await list_files_recursive(afc, remote_root)
    for relative, is_directory, remote_path in entries:
        if is_directory:
            continue
        print(f"Copying recording from iPad: {relative}")
        transfers.append(
            await download_recording(afc, remote_path, MAC_RECORDINGS_DIR / relative)
        )
    return transfers


async def sync_log_file(afc, remote_path, local_path):
    info = await afc.stat(remote_path)
    remote_size = int(info["st_size"])
    if local_path.exists() and local_path.stat().st_size >= remote_size:
        return False
    await download_afc_file(
        afc,
        remote_path,
        local_path,
        progress_label="Copied",
        expected_size=remote_size,
    )
    return True


async def sync_logs_from_ipad(afc, remote_root):
    MAC_LOGS_DIR.mkdir(parents=True, exist_ok=True)
    entries = await list_files_recursive(afc, remote_root)
    for relative, is_directory, remote_path in entries:
        if is_directory:
            continue
        local_path = MAC_LOGS_DIR / relative
        if await sync_log_file(afc, remote_path, local_path):
            print(f"Copied log from iPad: {relative}")


async def sync(transport, udid, host, include_recordings=True):
    for directory in (MAC_PATCHES_DIR, MAC_RECORDINGS_DIR, MAC_LOGS_DIR):
        directory.mkdir(parents=True, exist_ok=True)

    recordings = []
    async with connect_ipad(transport, udid=udid, host=host) as lockdown:
        print(f"Connected to {lockdown.display_name} ({lockdown.udid})")
        async with await HouseArrestService.create(
            lockdown,
            APP_BUNDLE_ID,
            documents_only=False,
        ) as afc:
            remote_paths = {
                "patches": f"{IPAD_DOCUMENTS_DIR}/patches",
                "recordings": f"{IPAD_DOCUMENTS_DIR}/recordings",
                "logs": f"{IPAD_DOCUMENTS_DIR}/logs",
            }
            for path in (IPAD_DOCUMENTS_DIR, *remote_paths.values()):
                await ensure_remote_dir(afc, path)

            print("\n--- Syncing Patches (Mac -> iPad) ---")
            await sync_patches_to_ipad(afc, remote_paths["patches"])
            print("\n--- Syncing Patches (iPad -> Mac) ---")
            await sync_patches_from_ipad(afc, remote_paths["patches"])
            if include_recordings:
                print("\n--- Syncing Recordings (iPad -> Mac, delete after extraction) ---")
                recordings = await download_recordings_from_ipad(
                    afc,
                    remote_paths["recordings"],
                )
            print("\n--- Syncing Logs (iPad -> Mac, retain on iPad) ---")
            await sync_logs_from_ipad(afc, remote_paths["logs"])
    return recordings


async def delete_recordings_from_ipad(transport, udid, host, recordings):
    async with connect_ipad(transport, udid=udid, host=host) as lockdown:
        print(f"Reconnected to {lockdown.display_name} ({lockdown.udid})")
        async with await HouseArrestService.create(
            lockdown,
            APP_BUNDLE_ID,
            documents_only=False,
        ) as afc:
            for transfer in recordings:
                await delete_recording(afc, transfer)


def sync_all(transport, udid, host, include_recordings=True):
    recordings = asyncio.run(
        sync(
            transport,
            udid,
            host,
            include_recordings=include_recordings,
        )
    )
    for transfer in recordings:
        extract_recording(transfer)
    if recordings:
        asyncio.run(delete_recordings_from_ipad(transport, udid, host, recordings))


def build_parser():
    parser = argparse.ArgumentParser(description="Sync SmartGridOne files with the paired iPad")
    parser.add_argument("--transport", choices=("auto", "usb", "wifi"), default="auto")
    parser.add_argument("--udid", default=None)
    parser.add_argument("--host", default=None)
    parser.add_argument(
        "--no-recordings",
        action="store_true",
        help="skip all recording download and deletion",
    )
    return parser


def main():
    args = build_parser().parse_args()
    udid = device_udid(args.udid)
    host = device_host(args.host)
    print(f"SmartGridOne iPad sync: {udid} via {args.transport}")
    try:
        sync_all(
            args.transport,
            udid,
            host,
            include_recordings=not args.no_recordings,
        )
    except KeyboardInterrupt:
        print("Interrupted; iPad files retained.", file=sys.stderr)
        return 130
    except Exception as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print("Sync complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
