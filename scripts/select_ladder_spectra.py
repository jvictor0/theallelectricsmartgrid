#!/usr/bin/env python3
"""Choose low/high power representatives and reset controls for waveform capture."""
import argparse
import json
import math
from pathlib import Path
import numpy as np


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    args=parser.parse_args(); root=args.root
    source=root/'ten-minutes'
    data=np.load(source/'checkpoint-trials.npz')
    report=json.loads((source/'report.json').read_text())
    metadata=json.loads((source/'metadata.json').read_text())
    clusters=json.loads((source/'findings.json').read_text())['persistent_clusters']
    result={'sample_rate':192000,'amplitude':.25,'start_sample':600*192000,
            'model':'detector-disabled float/FMA rational saturation',
            'source':'ten-minutes/final-states.bin','representatives':[]}
    lines=[]
    for cluster in clusters:
        point=cluster['point_id']; i=int(np.where(data['point_ids']==point)[0][0])
        c=report['cases'][i]; tone=metadata['cases'][i]
        assert c['point_id']==tone['point_id']==point
        q=tone['period_samples']; multiple=840*q
        samples=math.ceil(16*192000/multiple)*multiple
        powers=data['values'][i,-1,0,:,1]
        order=np.argsort(powers[:100]); split=cluster['low_count']
        levels={'low':order[:split],'high':order[split:]}
        chosen=[]
        for label,indices in levels.items():
            for rank in sorted(set([len(indices)//4,3*len(indices)//4])):
                chosen.append((int(indices[rank]),label))
        reset_label='low' if powers[100] < math.sqrt(powers[order[split-1]]*powers[order[split]]) else 'high'
        chosen.append((100,'reset_'+reset_label))
        for start,label in chosen:
            r={k:v for k,v in c.items() if k!='checkpoints'}
            r.update(start_index=start,level=label,period_samples=q,samples=samples,
                     cycles_per_period=round(c['frequency_hz']*q/192000),
                     global_state_index=i*101+start,cluster_counts=[split,100-split],
                     long_total_power=float(data['values'][i,-1,0,start,0]),
                     long_fundamental_power=float(powers[start]),
                     file=f'wave-{point}-{start}.f32')
            result['representatives'].append(r)
            lines.append(f'{point} {i*101+start} {samples}\n')
    (root/'spectrum-selection.json').write_text(json.dumps(result,indent=2)+'\n')
    (root/'spectrum-requests.txt').write_text(''.join(lines))
    print(json.dumps({'captures':len(lines),'bytes':sum(r['samples']*4 for r in result['representatives']),
                      'durations':sorted({r['samples']/192000 for r in result['representatives']})}))


if __name__=='__main__':
    main()
