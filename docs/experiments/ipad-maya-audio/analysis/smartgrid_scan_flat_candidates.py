import argparse,json,struct
from pathlib import Path
import numpy as np
from scipy.ndimage import uniform_filter1d
p=argparse.ArgumentParser();p.add_argument('--wav',required=True);p.add_argument('--output',required=True);a=p.parse_args()
path=Path(a.wav);out=Path(a.output)
if out.exists():raise FileExistsError(out)
runs=[]
with path.open('rb') as f:
    f.read(12)
    fmt=None
    while True:
        kind,n=struct.unpack('<4sI',f.read(8))
        if kind==b'fmt ':
            fmt=f.read(n);f.seek(n%2,1)
        elif kind==b'data':
            offset=f.tell();break
        else:f.seek(n+n%2,1)
    _,channels,rate,_,align,bits=struct.unpack('<HHIIHH',fmt[:16])
    if (channels,rate,align,bits)!=(2,48000,6,24):raise ValueError('Unexpected PCM format')
    frames=(path.stat().st_size-offset)//6
    for first in range(0,frames,480000):
        lo=max(0,first-8);hi=min(frames,first+480000+8)
        f.seek(offset+lo*6)
        raw=np.frombuffer(f.read((hi-lo)*6),dtype=np.uint8).reshape(-1,3).astype(np.int32)
        v=raw[:,0]|(raw[:,1]<<8)|(raw[:,2]<<16)
        x=((v^8388608)-8388608).reshape(-1,2)/8388608
        mean=uniform_filter1d(x,8,axis=0)
        var=np.maximum(0,uniform_filter1d(x*x,8,axis=0)-mean*mean)
        mask=(var.max(axis=1)<.00015**2)&(abs(mean).max(axis=1)<.01)
        mask=mask[first-lo:min(len(mask),first-lo+480000)]
        edges=np.diff(np.r_[False,mask,False].astype(np.int8))
        for s,t in zip(np.flatnonzero(edges==1)+first,np.flatnonzero(edges==-1)+first):
            if runs and s==runs[-1][1]:runs[-1][1]=int(t)
            else:runs.append([int(s),int(t)])
runs=[r for r in runs if r[1]-r[0]>=20]
short=[r for r in runs if r[1]-r[0]<960]
long=[r for r in runs if r[1]-r[0]>=960]
bins={}
for s,t in short:
    sec=s//48000
    b=bins.setdefault(sec,{'second':sec,'count':0,'flat_fraction':0})
    b['count']+=1;b['flat_fraction']+=(t-s)/48000
ranked=sorted(bins.values(),key=lambda b:(b['flat_fraction'],b['count']),reverse=True)
info={'wav':str(path),'seconds':frames/48000,'method':'Candidate flat regions in both channels: local 8-sample sigma <0.00015 and |mean|<0.01. Short candidates are 0.417–20 ms; natural smooth/quiet music can match, so clusters require waveform inspection and cadence checks. This detects the previously observed substantial blanking but does not rule out subtle clicks.','short_candidates':len(short),'long_candidates':len(long),'densest_short_candidate_seconds':ranked[:20],'short_runs_frames':short,'long_runs_frames':long}
out.write_text(json.dumps(info,indent=2))
print(json.dumps({k:v for k,v in info.items() if k not in ['short_runs_frames','long_runs_frames','method']},indent=2))
