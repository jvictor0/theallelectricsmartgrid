import argparse
import datetime
import json
from pathlib import Path
import re
import numpy as np

p=argparse.ArgumentParser()
p.add_argument('--base',required=True)
a=p.parse_args()
base=Path(a.base)
read=lambda suffix: json.loads(Path(str(base)+suffix).read_text())
m=read('.window.json')
c=read('.callbacks.json')
w=read('.activity-final.json')
start=datetime.datetime.fromisoformat(m['started_local'])
end=datetime.datetime.fromisoformat(m['ended_local'])
events=[]
for e in read('.system.json'):
    t=datetime.datetime.fromisoformat(e['timestamp'])
    if start<=t<end:
        events.append({'timestamp':t.isoformat(),'seconds':(t-start).total_seconds(),'process':Path(e.get('processImagePath','')).name,'message':e.get('eventMessage','')})
tx=[e for e in events if e['process']=='kernel' and 'MAYA44 USB+' in e['message'] and 'transaction error' in e['message']]
restart=[e for e in events if 'restarting IO' in e['message']]
zlp=[e for e in events if 'zero length transfers' in e['message']]
drift=[e for e in events if 'ioDriftNS' in e['message']]
gaps=[g for g in c['callback_gaps_gt20ms'] if g['gap_us']>100000]
quiet=w['near_silence_intervals']
rows=[]
for event in tx:
    g=min(gaps,key=lambda g:abs(g['recording_seconds']-event['seconds'])) if gaps else None
    q=min(quiet,key=lambda q:abs(q['end_seconds']-event['seconds'])) if quiet else None
    gm=g is not None and abs(g['recording_seconds']-event['seconds'])<0.75
    qm=q is not None and abs(q['end_seconds']-event['seconds'])<0.75
    rows.append({'usb_error_time':event['timestamp'],'usb_error_seconds':event['seconds'],'callback_match':gm,'wave_match':qm,'callback':g['callback'] if gm else None,'callback_gap_ms':g['gap_us']/1000 if gm else None,'usb_error_to_callback_resume_ms':(g['recording_seconds']-event['seconds'])*1000 if gm else None,'wave_quiet_interval':q if qm else None,'callback_resume_minus_wav_quiet_end_ms':(g['recording_seconds']-q['end_seconds'])*1000 if gm and qm else None})
matched=[r for r in rows if r['callback_match'] and r['wave_match']]
offsets=[r['callback_resume_minus_wav_quiet_end_ms'] for r in matched]
info={'base':str(base),'recording_start':start.isoformat(),'recording_end':end.isoformat(),'recording_seconds':m['measured_wav_seconds'],'kernel_maya_transaction_failures':len(tx),'audio_restart_records':len(restart),'near_silence_wave_intervals':len(quiet),'native_callback_gaps_gt100ms':len(gaps),'matched_usb_error_messages':len(matched),'matched_interruption_episodes':len({r['callback'] for r in matched}),'unique_matched_callbacks':len({r['callback'] for r in matched}),'unique_matched_wave_intervals':len({r['wave_quiet_interval']['start_seconds'] for r in matched}),'zero_length_reports':len(zlp),'zero_length_transfers_reported':sum(int(re.search(r'recieved (\d+) zero length',e['message']).group(1)) for e in zlp),'io_drift_records':len(drift),'callback_resume_minus_wav_end_ms_range':[min(offsets),max(offsets)] if offsets else [],'correlation_limit':'Wave event matching uses a conservative 750 ms neighborhood and reports unique callback/wave episodes. Multiple USB failure messages can belong to one interruption. Recorder/CoreAudio startup latency and independent audio clocks shift WAV timing relative to app logs; offsets are not causal transport latency measurements.','rows':rows,'zlp_events':zlp,'drift_events':drift}
out=Path(str(base)+'.correlation.json')
if out.exists():
    raise FileExistsError(out)
out.write_text(json.dumps(info,indent=2))
print(json.dumps({k:v for k,v in info.items() if k not in ['rows','zlp_events','drift_events']},indent=2))
