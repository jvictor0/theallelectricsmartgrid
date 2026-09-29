import asyncio, datetime, json, plistlib
from pathlib import Path
from smartgrid_ipad_wifi import connect, run
from pymobiledevice3.services.diagnostics import DiagnosticsService

BASE = Path('/private/tmp/smartgrid-ipad-power-hub-20260910')
KEEP = {'CurrentCapacity','MaxCapacity','IsCharging','FullyCharged','ExternalConnected','ExternalChargeCapable','BatteryCurrentCapacity','BatteryIsCharging','BatteryIsFullyCharged','Voltage','Amperage','InstantAmperage','Temperature','AdapterDetails','ChargerData','PowerTelemetryData','USB Product Name','USB Vendor Name','idVendor','idProduct','bcdDevice','bDeviceClass','bDeviceSubClass','bDeviceProtocol','bMaxPower','Device Speed','IOClass','IORegistryEntryName','name','product-name','manufacturer','portType'}

def selected(obj, path=''):
    rows = []
    if isinstance(obj, dict):
        for key, value in obj.items():
            current = f'{path}/{key}'
            if key in KEEP or ('temperat' in key.lower() and not isinstance(value, (dict,list))):
                rows.append({'path': current, 'value': value})
            if isinstance(value, (dict,list)):
                rows.extend(selected(value, current))
    elif isinstance(obj, list):
        for index, value in enumerate(obj):
            rows.extend(selected(value, f'{path}/{index}'))
    return rows

async def main():
    result = {'collected_local': datetime.datetime.now().astimezone().isoformat(), 'queries': {}}
    async with connect() as client:
        result['wifi_connections_preference'] = await client.get_enable_wifi_connections()
        try:
            value = await asyncio.wait_for(client.get_value(domain='com.apple.mobile.battery'), timeout=10)
            result['queries']['battery_domain'] = {'value': value}
        except Exception as error:
            result['queries']['battery_domain'] = {'error': type(error).__name__, 'detail': str(error)}
        async with DiagnosticsService(client) as diagnostics:
            for label, kwargs in [('battery', {'ioclass':'IOPMPowerSource'}), ('usb_devices', {'ioclass':'IOUSBHostDevice'}), ('usb_hub', {'name':'USB2.0 Hub'})]:
                try:
                    value = await asyncio.wait_for(diagnostics.ioregistry(**kwargs), timeout=12)
                    if value is not None:
                        Path(str(BASE)+'.'+label+'.plist').write_bytes(plistlib.dumps(value, fmt=plistlib.FMT_XML))
                    result['queries'][label] = {'present': value is not None, 'selected': selected(value)}
                except Exception as error:
                    result['queries'][label] = {'error': type(error).__name__, 'detail': str(error)}
    Path(str(BASE)+'.json').write_text(json.dumps(result, indent=2, default=str)+'\n')
    print(json.dumps(result, indent=2, default=str))

run(main(), timeout=55)
