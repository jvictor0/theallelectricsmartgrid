"""Compare independent resonance traces with neighboring-frequency shooting on the exact tones."""
import argparse
import csv
import gzip
import json
from pathlib import Path

import numpy as np

from pilot import ACTIVE, AMPLITUDE, RATE, Tone, diagnostics, solve


def march(seed, end, stride):
    start = seed['q']
    direction = 1 if end>start else -1
    previous = None
    state = np.array(seed['state'])
    out = {start:seed}
    for q in range(start+direction*stride,end+direction,direction*stride):
        tone = Tone(q,seed['cutoff_hz'],seed['drive'])
        tone.set_resonance(seed['r'])
        guess = state.copy() if previous is None else 2*state-previous
        guess[4:9] = tone.anchor[4:9]
        root = solve(tone,guess,None)
        if root is None or max(abs((root[0]-guess)[ACTIVE])/np.maximum(.25,tone.scales(state)))>.45:
            break
        previous,state = state,root[0]
        out[q] = dict(q=q,**diagnostics(tone,state,None))
    return out


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('sweep',type=Path)
    ap.add_argument('out',type=Path)
    args=ap.parse_args()
    with gzip.open(args.sweep) as stream:
        data=list(map(json.loads,stream))
    targets={int(r['q']) for r in data}
    records={(r['q'],r['spacing']):r for r in data if r['kind']=='target' and r['r']>.8}
    gaps={r['q']:r['state_scaled_inf'] for r in data if r['kind']=='mesh_check' and r['r']>.8}
    anchors={q:records[q,.01] for q,gap in gaps.items() if gap<=1e-5 and all(
        records[q,h]['full_return_residual']<=1e-8 and records[q,h]['metal_jac_relative_inf']<=.01 for h in (.01,.02))}
    with args.out.open('x',newline='') as file:
        cols=['hz','period','on_gpu_grid','mesh_gap','kind','base_feedback','total_gain_db','fundamental_gain_db',
              'wet_multiplier','return_residual','frequency_routes','route_gap','initial_state']
        writer=csv.DictWriter(file,fieldnames=cols)
        writer.writeheader()
        for near in (True,False):
            band=sorted(q for q in targets if (q<300)==near)
            seeds=sorted(q for q in anchors if q in band)
            tracks=[]
            for seed in seeds:
                for end in (band[0],band[-1]):
                    for stride in (1,2):
                        tracks.append(march(anchors[seed],end,stride))
            for q in range(band[0],band[-1]+1):
                found=[t[q] for t in tracks if q in t]
                item=anchors.get(q,found[0] if found else None)
                tone=Tone(q,anchors[seeds[0]]['cutoff_hz'],anchors[seeds[0]]['drive'])
                row=dict(hz=RATE/q,period=q,on_gpu_grid=q in targets,mesh_gap=gaps.get(q),frequency_routes=len(found))
                if item:
                    z=np.array(item['state'])
                    gap=max([max(abs((z-np.array(r['state']))[ACTIVE])/tone.scales(z)) for r in found],default=0.)
                    row.update(kind='anchored' if q in anchors else 'frequency_candidate',route_gap=gap,
                               base_feedback=item['base_feedback'],total_gain_db=10*np.log10(item['total_power']/(AMPLITUDE**2/2)),
                               fundamental_gain_db=10*np.log10(item['fundamental_power']/(AMPLITUDE**2/2)),
                               wet_multiplier=item['rho_wet'],return_residual=item['full_return_residual'],initial_state=json.dumps(z.tolist()))
                else:
                    row['kind']='unresolved'
                writer.writerow(row)


if __name__=='__main__':
    main()
