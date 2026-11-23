"""Compare identical noise suffixes starting from zero versus a 4.096-second running ladder."""
import argparse
import json
from pathlib import Path
import numpy as np
from display_metric import displayed_db, display_delta
from survey import GPU,band_power,make_noise,atomic_json

def main(pilot,tools,production):
    m=json.loads((pilot/'manifest.json').read_text())
    trials=m['trials']
    epochs=m['measure_epochs']
    prefix=epochs//2 if not production else 10
    width=4 if not production else 8
    gpu=GPU(tools,(m['tile']+1)*trials,trials)
    result=[];ids=[]; combined=[]
    for tile,file in enumerate(sorted(pilot.glob('tile-*.npz'))):
        with np.load(file) as z:
            indices=z['ids']
            warm=z['pilot_epochs'][prefix:].mean(2,dtype=np.float64)
        fraction=indices/(m['grid']-1)
        coords=np.column_stack((fraction[:,:2],20*1000**fraction[:,2]))
        output=gpu.reset(coords,trials)
        noise=make_noise(m['base_seed']+tile,trials,epochs)
        cold=[]
        for block in noise[prefix:]:
            gpu.epoch(block,output)
            cold.append(band_power(output).mean(1,dtype=np.float64))
        cold=np.stack(cold)
        gain_w=displayed_db(10*np.log10(warm[:,:-1]/warm[:,-1:]))
        gain_c=displayed_db(10*np.log10(cold[:,:-1]/cold[:,-1:]))
        result.append(np.stack([gain_c,gain_w],axis=-1))
        averages=[]
        for start in (0,1,2,3,4,6,8):
            if start+width<=len(cold):
                a=cold[start:start+width].mean(0); b=warm[start:start+width].mean(0)
                averages.append(np.stack([10*np.log10(a[:-1]/a[-1:]),10*np.log10(b[:-1]/b[-1:])],axis=-1))
        combined.append(averages)
        ids.append(indices)
    values=np.concatenate(result,axis=1)
    indices=np.concatenate(ids)
    if not production:
        np.savez_compressed(pilot/'matched-startup.npz',db=values.astype(np.float32),ids=indices)
    rows=[]
    for j,pair in enumerate(values):
        err=display_delta(pair[:,:,0],pair[:,:,1])
        worst=np.unravel_index(np.argmax(err),err.shape)
        rows.append(dict(new_start_seconds=round(j*m['epoch_seconds'],4),eligible=err.size,
            abs_db_p50_p95_p99_max=np.quantile(err,[.5,.95,.99,1]).tolist(),
            fraction_over_1db=float(np.mean(err>1)),worst_indices=indices[worst[0]].tolist(),worst_band=int(worst[1])))
    blocks=np.concatenate(combined,axis=1)
    spans=[]
    for start,pair in zip([n for n in (0,1,2,3,4,6,8) if n+width<=epochs-prefix],blocks):
        err=display_delta(pair[:,:,0],pair[:,:,1])
        spans.append(dict(warm_epochs=start,warm_seconds=start*m['epoch_seconds'],measure_epochs=width,
            eligible=err.size,p95_p99_max=np.quantile(err,[.95,.99,1]).tolist(),
            over_1db=float(np.mean(err>1))))
    name='measured-protocol.json' if production else 'matched-startup.json'
    atomic_json(pilot/name,dict(reference_head_start_s=prefix*m['epoch_seconds'],individual=rows,averages=spans))
    print(json.dumps(rows,indent=2))

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('pilot',type=Path)
    ap.add_argument('--tools',type=Path,default=Path('tools'))
    ap.add_argument('--production',action='store_true')
    args=ap.parse_args(); main(args.pilot,args.tools,args.production)
