#!/usr/bin/env python3
"""Run one named, single-threaded iPad benchmark and retain its results in SQLite."""

import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import plistlib
import sqlite3
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
BUNDLE_ID = "com.theallelectricsmartgrid.perf"
DEFAULT_APP = ROOT / "build/tools/perf-ipad/Release/Release-iphoneos/SmartGridPerf.app"
DEFAULT_DB = Path.home() / "Documents/SmartGridOne/perf.sqlite3"


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def open_database(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(path)
    db.execute("""CREATE TABLE IF NOT EXISTS runs (
        run_id TEXT PRIMARY KEY, started_at TEXT NOT NULL, finished_at TEXT,
        case_name TEXT NOT NULL, git_commit TEXT, git_dirty INTEGER,
        notes TEXT NOT NULL, case_notes TEXT, arguments_json TEXT NOT NULL,
        status TEXT NOT NULL, error TEXT, results_json TEXT, telemetry_json TEXT,
        app_json TEXT
    )""")
    db.commit()
    return db


def start_run(db, request):
    db.execute("""INSERT INTO runs
        (run_id, started_at, case_name, notes, arguments_json, status)
        VALUES (?, ?, ?, ?, ?, 'running')""",
        (request["run_id"], utc_now(), request["case"], request["notes"], json.dumps(request["arguments"])))
    db.commit()


def save_report(db, request, report):
    for key in ("run_id", "case", "arguments", "notes"):
        if report.get(key) != request[key]:
            raise ValueError(f"Device report does not match request: {key}")
    if report.get("status") not in ("completed", "failed", "timeout"):
        raise ValueError("Device report has no final status")
    rows = report.get("results")
    if not isinstance(rows, list) or not all(isinstance(row, dict) for row in rows):
        raise ValueError("Case results must be JSON objects")
    telemetry = report.get("telemetry")
    if not isinstance(telemetry, list):
        raise ValueError("Missing device telemetry")
    if not isinstance(report.get("git_commit"), str) or len(report["git_commit"]) != 40:
        raise ValueError("Missing build commit")
    app = {key: value for key, value in report.items()
           if key not in ("results", "telemetry", "notes", "case_notes", "arguments")}
    values = (utc_now(), report["git_commit"], bool(report["git_dirty"]),
              report.get("case_notes", ""), report["status"], report.get("error", ""),
              json.dumps(rows, allow_nan=False), json.dumps(telemetry, allow_nan=False),
              json.dumps(app, allow_nan=False), request["run_id"])
    db.execute("""UPDATE runs SET finished_at=?, git_commit=?, git_dirty=?,
        case_notes=?, status=?, error=?, results_json=?, telemetry_json=?, app_json=?
        WHERE run_id=?""", values)
    db.commit()


def fail_run(db, run_id, error):
    db.execute("UPDATE runs SET status='failed', error=?, finished_at=? WHERE run_id=?",
               (str(error), utc_now(), run_id))
    db.commit()


async def run_device(args, request):
    from pymobiledevice3.remote.core_device.app_service import AppServiceService
    from pymobiledevice3.services.house_arrest import HouseArrestService
    from pymobiledevice3.services.installation_proxy import InstallationProxyService
    from ipad_device import connect_developer, connect_ipad, device_host, device_udid

    udid, host = device_udid(args.udid), device_host(args.host)
    async with connect_ipad("wifi", udid=udid, host=host) as lockdown:
        if not args.skip_install:
            with (args.app / "Info.plist").open("rb") as file:
                info = plistlib.load(file)
            if info.get("CFBundleIdentifier") != BUNDLE_ID:
                raise ValueError("Expected the benchmark app, not SmartGridOne")
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(args.app)], check=True)
            print("Installing SmartGrid Perf", flush=True)
            async with InstallationProxyService(lockdown) as installer:
                await installer.install_from_local(args.app, cmd="Upgrade", developer=True)

        async with await HouseArrestService.create(lockdown, BUNDLE_ID, documents_only=False) as afc:
            async with connect_developer(udid=udid, host=host) as rsd:
                async with AppServiceService(rsd) as service:
                    await service.launch_application(
                        BUNDLE_ID, arguments=["--request", json.dumps(request)])
            print(f"Running {request['case']} ({request['run_id']})", flush=True)
            path = f"/Documents/perf/{request['run_id']}.json"
            deadline = time.monotonic() + request["timeout"] + 15
            while time.monotonic() < deadline:
                if await afc.exists(path):
                    report = json.loads(await afc.get_file_contents(path))
                    report["device_udid"] = udid
                    return report
                await asyncio.sleep(1)
            raise TimeoutError("No final report from the iPad")


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", default="inverter")
    parser.add_argument("--notes", default="", help="What this run tests and why; stored alongside case notes")
    parser.add_argument("--db", type=Path, default=DEFAULT_DB)
    parser.add_argument("--app", type=Path, default=DEFAULT_APP)
    parser.add_argument("--skip-install", action="store_true", help="Run the already installed benchmark build")
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--udid")
    parser.add_argument("--host")
    parser.add_argument("arguments", nargs=argparse.REMAINDER, help="Case-specific arguments after --")
    return parser


def main():
    args = build_parser().parse_args()
    if not 0 < args.timeout <= 86400:
        raise SystemExit("--timeout must be in 1..86400 seconds")
    words = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    request = {"run_id": str(uuid.uuid4()), "case": args.case, "arguments": words,
               "notes": args.notes, "timeout": args.timeout}
    db = open_database(args.db.expanduser())
    try:
        start_run(db, request)
        report = asyncio.run(run_device(args, request))
        save_report(db, request, report)
        for row in report["results"]:
            print(json.dumps(row))
        print(f"{report['status']}: {request['run_id']}\nDatabase: {args.db}")
        if report["status"] != "completed":
            print(report.get("error", "Benchmark failed"), file=sys.stderr)
            return 1
        return 0
    except (Exception, KeyboardInterrupt) as error:
        fail_run(db, request["run_id"], error or "Interrupted")
        print(f"Benchmark failed: {error}\nRun: {request['run_id']}\nDatabase: {args.db}", file=sys.stderr)
        return 1
    finally:
        db.close()


if __name__ == "__main__":
    sys.exit(main())
