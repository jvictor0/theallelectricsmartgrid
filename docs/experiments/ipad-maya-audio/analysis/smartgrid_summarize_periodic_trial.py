import argparse
import json
from pathlib import Path
from collections import Counter
import numpy as np

p=argparse.ArgumentParser()
p.add_argument('--base',required=True)
p.add_argument('--flat-suffix',default='.flat-candidates.json')
p.add_argument('--output')
a=p.parse_args()
b=Path(a.base)
out=Path(a.output or str(b)+'.periodic.json')
if out.exists():raise FileExistsError(out)
flat=json.loads(Path(str(b)+a.flat_suffix).read_text())
runs=np.asarray(flat['short_runs_frames'],dtype=np.int64).reshape(-1,2)
bins=Counter((runs[:,0]//48000).tolist())
groups=[]
for second in sorted(s for s,count in bins.items() if count>=10):
    if groups and second==groups[-1][-1]+1:groups[-1].append(second)
    else:groups.append([second])
episodes=[]
for group in groups:
    selected=runs[(runs[:,0]>=group[0]*48000)&(runs[:,0]<(group[-1]+1)*48000)]
    episodes.append({'bin_start_seconds':group[0],'bin_end_seconds':group[-1]+1,'first_hole_seconds':selected[0,0]/48000,'last_hole_end_seconds':selected[-1,1]/48000,'short_holes':len(selected),'width_ms_min_median_p99_max':np.percentile((selected[:,1]-selected[:,0])/48,[0,50,99,100]).tolist(),'flat_fraction_within_episode_envelope':float(np.sum(selected[:,1]-selected[:,0])/(selected[-1,1]-selected[0,0]))})
windows=[]
for start in range(0,int(flat['seconds']),10):
    end=min(start+10,flat['seconds'])
    r=runs[(runs[:,0]>=start*48000)&(runs[:,0]<end*48000)]
    spacing=np.diff(r[:,0])/48
    windows.append({'start_seconds':start,'end_seconds':end,'holes':len(r),'holes_per_second':len(r)/(end-start),'flat_fraction':float(np.sum(r[:,1]-r[:,0])/(48000*(end-start))) if len(r) else 0,'median_width_ms':float(np.median(r[:,1]-r[:,0])/48) if len(r) else None,'onset_spacing_ms_p10_p50_p90':np.percentile(spacing,[10,50,90]).tolist() if len(spacing) else []})
info={'base':str(b),'seconds':flat['seconds'],'short_candidates':flat['short_candidates'],'episodes':episodes,'ten_second_windows':windows,'method':'Consecutive 1-second bins with at least 10 short flat candidates form dense episodes. Same criterion as the earlier UI comparison. Natural quiet/smooth music can produce candidates; inspect waveform and cadence to attribute periodic blanking. Analog settling affects widths. No claim against subtle clicks.'}
out.write_text(json.dumps(info,indent=2))
print(json.dumps({k:v for k,v in info.items() if k not in ['ten_second_windows','method']},indent=2))
