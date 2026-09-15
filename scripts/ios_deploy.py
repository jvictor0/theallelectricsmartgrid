#!/usr/bin/env python3

import argparse
import asyncio
import plistlib
import subprocess
import sys
from pathlib import Path

from pymobiledevice3.remote.core_device.app_service import AppServiceService
from pymobiledevice3.services.installation_proxy import InstallationProxyService

from ipad_device import connect_developer, connect_ipad, device_host, device_udid


APP_BUNDLE_ID = "com.theallelectricsmartgrid.smartgridone"
DEFAULT_APP = (
    Path(__file__).resolve().parents[1]
    / "JUCE"
    / "SmartGridOne"
    / "Builds"
    / "iOS"
    / "build"
    / "Release"
    / "SmartGridOne.app"
)


def validate_app(app_path):
    if not app_path.is_dir():
        raise FileNotFoundError(f"Missing iOS app: {app_path}")
    info_path = app_path / "Info.plist"
    if not info_path.is_file():
        raise FileNotFoundError(f"Missing app Info.plist: {info_path}")
    with info_path.open("rb") as source:
        bundle_id = plistlib.load(source).get("CFBundleIdentifier")
    if bundle_id != APP_BUNDLE_ID:
        raise ValueError(f"App has bundle id {bundle_id!r}; expected {APP_BUNDLE_ID!r}")
    subprocess.run(
        ["codesign", "--verify", "--deep", "--strict", str(app_path)],
        check=True,
    )


async def install_app(app_path, udid, host):
    print(f"Installing upgrade on {udid} over Wi-Fi")
    async with connect_ipad("wifi", udid=udid, host=host) as lockdown:
        async with InstallationProxyService(lockdown) as installer:
            await installer.install_from_local(
                app_path,
                cmd="Upgrade",
                developer=True,
            )


async def launch_app(udid, host):
    print(f"Launching {APP_BUNDLE_ID}")
    async with connect_developer(udid=udid, host=host) as rsd:
        async with AppServiceService(rsd) as app_service:
            await app_service.launch_application(APP_BUNDLE_ID)


async def deploy(app_path, udid, host, install_only):
    print(f"App: {app_path}")
    print("Packaging signed app for installation")
    validate_app(app_path)
    await install_app(app_path, udid, host)
    if install_only:
        print("Install complete")
        return
    try:
        await launch_app(udid, host)
    except Exception as error:
        raise RuntimeError(f"Installed, but could not launch: {error}") from error
    print("Deploy complete")


def build_parser():
    parser = argparse.ArgumentParser(description="Install and launch SmartGridOne over iPad Wi-Fi")
    parser.add_argument("--app", type=Path, default=DEFAULT_APP)
    parser.add_argument("--udid", default=None)
    parser.add_argument("--host", default=None)
    parser.add_argument("--install-only", action="store_true")
    return parser


def main():
    args = build_parser().parse_args()
    try:
        asyncio.run(
            deploy(
                args.app.resolve(),
                device_udid(args.udid),
                device_host(args.host),
                args.install_only,
            )
        )
    except Exception as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
