#!/usr/bin/env python3

import argparse
import asyncio
import re
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path, PurePosixPath

from pymobiledevice3.services.house_arrest import HouseArrestService
from pymobiledevice3.services.os_trace import OsTraceService

from ipad_device import connect_ipad, device_host, device_udid
from sync_ipad import APP_BUNDLE_ID, IPAD_DOCUMENTS_DIR, download_afc_file, list_files_recursive


DEFAULT_OUTPUT_ROOT = Path.home() / "Documents" / "SmartGridOne" / "diagnostics"
SYSTEM_SIZE_LIMIT = 64 * 1024 * 1024
SYSTEM_TIMEOUT = 180


def parse_duration(value):
    match = re.fullmatch(r"([1-9][0-9]*)([smhd])", value)
    if match is None:
        raise ValueError("duration must look like 30s, 15m, 2h, or 1d")
    multipliers = {"s": 1, "m": 60, "h": 3600, "d": 86400}
    return int(match.group(1)) * multipliers[match.group(2)]


def parse_since(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("--since must include a timezone, such as -07:00 or Z")
    return parsed


def capture_paths(kind, requested_output):
    if requested_output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        complete = DEFAULT_OUTPUT_ROOT / f"{stamp}-{kind}"
    else:
        complete = requested_output.expanduser().resolve()
    partial = complete.with_name(f"{complete.name}.partial")
    if complete.exists() or partial.exists():
        raise FileExistsError(f"Capture path already exists: {complete} or {partial}")
    partial.mkdir(parents=True)
    return partial, complete


def selected_remote_path(name):
    if name is None:
        return None
    relative = PurePosixPath(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("--name must be relative to the SmartGridOne logs directory")
    return f"{IPAD_DOCUMENTS_DIR}/logs/{relative}"


async def latest_app_log(afc):
    root = f"{IPAD_DOCUMENTS_DIR}/logs"
    candidates = []
    for relative, is_directory, remote_path in await list_files_recursive(afc, root):
        if is_directory or not relative.lower().endswith(".log"):
            continue
        info = await afc.stat(remote_path)
        modified = info.get("st_mtime")
        order = modified.timestamp() if hasattr(modified, "timestamp") else float(modified or 0)
        candidates.append((order, relative, remote_path, int(info["st_size"])))
    if not candidates:
        raise FileNotFoundError(f"No app logs under {root}")
    _order, relative, remote_path, size = max(candidates)
    return relative, remote_path, size


async def capture_app(args, udid, host):
    partial, complete = capture_paths("app", args.output)
    print(f"Capture in progress: {partial}")
    async with connect_ipad("wifi", udid=udid, host=host) as lockdown:
        print(f"Device: {lockdown.display_name} ({lockdown.udid})")
        async with await HouseArrestService.create(
            lockdown,
            APP_BUNDLE_ID,
            documents_only=False,
        ) as afc:
            remote_path = selected_remote_path(args.name)
            if remote_path is None:
                relative, remote_path, size = await latest_app_log(afc)
            else:
                info = await afc.stat(remote_path)
                relative = args.name
                size = int(info["st_size"])
            destination = partial / Path(relative).name
            await download_afc_file(
                afc,
                remote_path,
                destination,
                progress_label="Captured",
                expected_size=size,
            )
    partial.rename(complete)
    print(f"App snapshot: {complete}")


async def capture_system(args, udid, host):
    now = datetime.now(timezone.utc)
    if args.since is not None:
        since = parse_since(args.since)
        window = f"since {since.isoformat()}"
    else:
        seconds = parse_duration(args.last)
        since = now - timedelta(seconds=seconds)
        window = f"last {args.last} (since {since.isoformat()})"

    partial, complete = capture_paths("system.logarchive", args.output)
    print(f"Capture in progress: {partial}")
    async with connect_ipad("wifi", udid=udid, host=host) as lockdown:
        print(f"Device: {lockdown.display_name} ({lockdown.udid})")
        print(f"Requested system window: {window}")
        async with OsTraceService(lockdown) as trace:
            await asyncio.wait_for(
                trace.collect(
                    str(partial),
                    size_limit=SYSTEM_SIZE_LIMIT,
                    start_time=int(since.timestamp()),
                ),
                timeout=SYSTEM_TIMEOUT,
            )
    partial.rename(complete)
    print(f"System archive: {complete}")


def build_parser():
    parser = argparse.ArgumentParser(description="Capture SmartGridOne or iPad system logs over Wi-Fi")
    parser.add_argument("--udid", default=None)
    parser.add_argument("--host", default=None)
    subparsers = parser.add_subparsers(dest="mode", required=True)

    app = subparsers.add_parser("app", help="copy one app log without changing device files")
    app.add_argument("--name", help="log path relative to Documents/SmartGridOne/logs")
    app.add_argument("--output", type=Path)

    system = subparsers.add_parser("system", help="collect a historical unified log archive")
    window = system.add_mutually_exclusive_group()
    window.add_argument("--last", default="15m")
    window.add_argument("--since")
    system.add_argument("--output", type=Path)
    return parser


def main():
    args = build_parser().parse_args()
    udid = device_udid(args.udid)
    host = device_host(args.host)
    try:
        if args.mode == "app":
            asyncio.run(capture_app(args, udid, host))
        else:
            asyncio.run(capture_system(args, udid, host))
    except Exception as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
