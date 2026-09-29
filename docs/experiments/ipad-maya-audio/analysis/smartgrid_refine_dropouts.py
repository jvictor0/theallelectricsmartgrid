import argparse,json,struct
from pathlib import Path
import numpy as np
from scipy.ndimage import uniform_filter1d
p=argparse.ArgumentParser();p.add_argument('--base',required=True);a=p.parse_args()
base=Path(a.base)
activity=json.loads(Path(str(base)+'.activity-final.json').read_text())
rows=[]
with Path(str(base)+'.wav').open('rb') as f:
    f.read(12)
    while True:
        tag,n=struct.unpack('<4sI',f.read(8))
        if tag==b'data':
            offset=f.tell(); break
        f.seek(n+n%2,1)
    for core in activity['near_silence_intervals']:
        first=max(0,round((core['start_seconds']-.18)*48000))
        count=round((core['end_seconds']+.12)*48000)-first
        f.seek(offset+first*6)
        raw=np.frombuffer(f.read(count*6),dtype=np.uint8).reshape(-1,3).astype(np.int32)
        v=raw[:,0]|(raw[:,1]<<8)|(raw[:,2]<<16)
        x=((v^8388608)-8388608).reshape(-1,2)/8388608
        mean=uniform_filter1d(x,8,axis=0)
        var=np.maximum(0,uniform_filter1d(x*x,8,axis=0)-mean*mean)
        flat=(var.max(axis=1)<.00015**2)&(abs(mean).max(axis=1)<.01)
        e=np.diff(np.r_[False,flat,False].astype(np.int8))
        starts,ends=np.flatnonzero(e==1),np.flatnonzero(e==-1)
        candidates=[(s,t) for s,t in zip(starts,ends) if t-s>=960 and (s+first)/48000<=core['start_seconds']+.002 and (t+first)/48000>=core['end_seconds']-.002]
        if len(candidates)!=1:
            rows.append({'core':core,'refinement':'ambiguous','candidates':len(candidates)});continue
        s,t=candidates[0]
        rows.append({'start_seconds':(s+first)/48000,'end_seconds':(t+first)/48000,'duration_ms':(t-s)/48,'near_noise_core_ms':core['duration_ms']})
durations=[r['duration_ms'] for r in rows if 'duration_ms' in r]
info={'method':'Refines already-identified long near-noise candidates using 8-sample local standard deviation <0.00015 and |mean|<0.01 in both channels. Includes low-level analog settling outside the -75 dBFS core; not a general proof against subtle clicks or periodic corruption.','intervals':rows,'duration_ms_min_median_max':np.percentile(durations,[0,50,100]).tolist() if durations else []}
out=Path(str(base)+'.refined-dropouts.json')
if out.exists():raise FileExistsError(out)
out.write_text(json.dumps(info,indent=2))
print(json.dumps({'count':len(rows),'duration_ms_min_median_max':info['duration_ms_min_median_max'],'first':rows[:1]},indent=2))
