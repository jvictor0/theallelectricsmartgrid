#!/usr/bin/env python3

import asyncio
import os
from contextlib import asynccontextmanager, suppress
from pathlib import Path

from pymobiledevice3.lockdown import create_using_tcp, create_using_usbmux
from pymobiledevice3.pair_records import get_usbmux_pairing_record
from pymobiledevice3.remote import tunnel_service
from pymobiledevice3.remote.remote_service_discovery import RemoteServiceDiscoveryService
from pymobiledevice3.remote.tunnel_service import CoreDeviceTunnelProxy
from pymobiledevice3.remote.userspace_tunnel import UserspaceDialPlane
from pymobiledevice3.usbmux import list_devices


DEFAULT_UDID = "00008122-001439681EA1001C"
DEFAULT_HOST = "192.168.1.53"
CONNECT_TIMEOUT = 15
HEARTBEAT_TIMEOUT = 30
PAIRING_CACHE = Path("/private/tmp/smartgrid-device-pairing-cache")


def device_udid(value=None):
    return value or os.environ.get("IOS_DEVICE") or DEFAULT_UDID


def device_host(value=None):
    return value or os.environ.get("IPAD_HOST") or DEFAULT_HOST


def _same_udid(left, right):
    return left.replace("-", "") == right.replace("-", "")


def choose_transport(requested, devices, udid):
    if requested != "auto":
        return requested
    if any(device.is_usb and _same_udid(device.serial, udid) for device in devices):
        return "usb"
    return "wifi"


async def selected_transport(requested, udid):
    if requested != "auto":
        return requested
    return choose_transport(requested, await list_devices(), udid)


async def _heartbeat_loop(service):
    while True:
        message = await asyncio.wait_for(service.recv_plist(), HEARTBEAT_TIMEOUT)
        if message.get("Command") != "Marco":
            raise ConnectionError(f"Unexpected heartbeat response: {message!r}")
        await service.send_plist({"Command": "Polo"})


@asynccontextmanager
async def connect_ipad(transport="wifi", udid=None, host=None):
    udid = device_udid(udid)
    host = device_host(host)
    transport = await selected_transport(transport, udid)
    pair_record = await get_usbmux_pairing_record(udid)
    if pair_record is None:
        raise ConnectionError(
            f"No pairing record for {udid}. Connect by USB, unlock the iPad, and trust this Mac."
        )

    if transport == "usb":
        create = create_using_usbmux(
            serial=udid,
            identifier=udid,
            connection_type="USB",
            autopair=False,
            pair_record=pair_record,
            pairing_records_cache_folder=PAIRING_CACHE,
        )
    elif transport == "wifi":
        create = create_using_tcp(
            host,
            identifier=udid,
            autopair=False,
            pair_record=pair_record,
            pairing_records_cache_folder=PAIRING_CACHE,
            keep_alive=True,
        )
    else:
        raise ValueError(f"Unsupported transport: {transport}")

    client = await asyncio.wait_for(create, CONNECT_TIMEOUT)
    heartbeat = None
    heartbeat_task = None
    try:
        if not _same_udid(client.udid, udid):
            raise ConnectionError(f"Connected to {client.udid}, expected {udid}")
        if transport == "wifi":
            heartbeat = await asyncio.wait_for(
                client.start_lockdown_service("com.apple.mobile.heartbeat"),
                CONNECT_TIMEOUT,
            )
            first = await asyncio.wait_for(heartbeat.recv_plist(), CONNECT_TIMEOUT)
            if first.get("Command") != "Marco":
                raise ConnectionError(f"Unexpected heartbeat response: {first!r}")
            await heartbeat.send_plist({"Command": "Polo"})
            heartbeat_task = asyncio.create_task(_heartbeat_loop(heartbeat))
        yield client
    finally:
        if heartbeat_task is not None:
            heartbeat_task.cancel()
            with suppress(asyncio.CancelledError, Exception):
                await heartbeat_task
        if heartbeat is not None:
            with suppress(Exception):
                await heartbeat.close()
        await client.close()


@asynccontextmanager
async def connect_developer(udid=None, host=None):
    udid = device_udid(udid)
    async with connect_ipad("wifi", udid=udid, host=host) as lockdown:
        proxy = await CoreDeviceTunnelProxy.create(lockdown)
        previous = tunnel_service.USE_USERSPACE_TUNNEL
        tunnel_service.USE_USERSPACE_TUNNEL = True
        try:
            async with proxy.start_tcp_tunnel() as result:
                result.client.tun.set_peer(result.address)
                async with UserspaceDialPlane(result.client.tun, result.address) as dial_plane:
                    async with RemoteServiceDiscoveryService(
                        (result.address, result.port),
                        open_connection=dial_plane.dial,
                    ) as rsd:
                        if not _same_udid(rsd.udid, udid):
                            raise ConnectionError(f"Developer tunnel reached {rsd.udid}, expected {udid}")
                        yield rsd
        finally:
            tunnel_service.USE_USERSPACE_TUNNEL = previous
            await proxy.close()
