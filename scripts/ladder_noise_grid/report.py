"""Pilot repeatability, observation duration and leave-one-out interpolation checks."""
import argparse,json
from pathlib import Path
import numpy as np
from survey import atomic_json
from display_metric import display_delta

def db(x):
    return 10*np.log10(np.maximum(x,1e-30))

def compare(a,b):
    delta=display_delta(a,b)
    return dict(count=delta.size,p50=round(float(np.median(delta)),3),
                p95=round(float(np.quantile(delta,.95)),3),p99=round(float(np.quantile(delta,.99)),3),
                maximum=round(float(delta.max()),3))

def main(directory, warm):
    m=json.loads((directory/'manifest.json').read_text())
    grid=m['grid'];trials=m['trials']
    spectra=np.full((grid,grid,grid,120),np.nan)
    halves={}; times={}
    for file in sorted(directory.glob('tile-*.npz')):
        with np.load(file) as z:
            records=z['pilot_epochs'].astype(np.float64)
            index=tuple(z['ids'].T)
        def curve0(records):
            avg=records.mean(axis=(0,2))
            return db(avg[:-1]/avg[-1])
        def curve(epochs,seeds):
            return curve0(records[epochs,:,seeds,:])
        late=slice(warm,None)
        spectra[index]=curve(late,slice(None))
        for count in (2,4,8):
            for span in (1,2,4,6,8):
                if 2*count>trials or span>len(records)-warm: continue
                comp=halves.setdefault(f'{count}_seeds_{span}_epochs',[[],[]])
                for j in (0,1):
                    comp[j].append(curve(slice(-span,None),slice(j*count,(j+1)*count)))
        for span in (1,2,4):
            if 2*span<=len(records)-warm:
                comp=times.setdefault(f'{trials}_seeds_{span}_epochs',[[],[]])
                comp[0].append(curve(slice(-2*span,-span),slice(None)))
                comp[1].append(curve(slice(-span,None),slice(None)))
    assert np.isfinite(spectra).all()
    def summarize(blocks):
        return {k:compare(*[np.concatenate(v) for v in pair]) for k,pair in blocks.items()}
    interpolation={}
    for axis,name in enumerate(['drive','resonance','cutoff']):
        v=np.moveaxis(spectra,axis,0)
        target=v[1:-1]
        interpolation[name]=compare((v[:-2]+v[2:])/2,target)
    report=dict(averages=summarize(halves),time_windows=summarize(times),coarse_interpolation=interpolation,
                eligible='all displayed bands, both sides clamped at -60 dB after interpolation',
                note='Independent groups of seeds share the same warm-up and window schedule. Interpolation is linear in dB along one knob coordinate.')
    atomic_json(directory/'pilot-analysis.json',report)
    print(json.dumps(report,indent=2))
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);ap.add_argument('--warm',type=int,default=2)
    a=ap.parse_args();main(a.directory,a.warm)
