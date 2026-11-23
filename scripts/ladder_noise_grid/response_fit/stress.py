"""Remeasure predefined fit outliers with independent uninterrupted noise records."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys
import numpy as np

SETTINGS = np.array([
    [.603285598555831, .9773123499490617, 249.69915451988314],
    [.7146004424093085, .9313113492006563, 515.4256803667313],
    [.49585706604611235, .9271559566816086, 358.5223651365964],
    [.8724309547513712, .42003005226619483, 21.51786432209247],
])
WINDOW_EPOCHS = 14
WINDOWS = 8
TRIALS = 64

def main(experiment, out):
    sys.path.insert(0, str(experiment / 'fits/screen-v3'))
    from replay import EPOCH, EDGES, stock, make_noise, all_phases
    from fit_display import response_db, curve_mse
    model_path = experiment / 'fits/screen-v3/display-model.npz'
    model = dict(np.load(model_path))
    centers = np.sqrt(EDGES[:-1]*EDGES[1:])
    fit = np.stack([response_db(model, *p, centers) for p in SETTINGS])
    out.mkdir(parents=True, exist_ok=True)
    width = EPOCH * WINDOW_EPOCHS
    def windows(x):
        return [all_phases(x[w*width:(w+1)*width],x[w*width-1024:w*width])
                for w in (1,3,6,7)]
    with ThreadPoolExecutor(len(SETTINGS)+1) as pool:
        for j in range(TRIALS):
            dest = out / f'trial-{j:03d}.npz'
            if dest.exists(): continue
            x = make_noise(315780002+j,1,WINDOW_EPOCHS*WINDOWS).reshape(-1)
            def wet(p):
                y = np.empty_like(x)
                stock.Reference(*p, len(x), x, y)
                return windows(y)
            dry = pool.submit(windows,x)
            values = list(pool.map(wet, SETTINGS))
            np.savez(dest, wet=values, dry=dry.result())
    records = [np.load(out/f'trial-{j:03d}.npz') for j in range(TRIALS)]
    wet = np.stack([r['wet'] for r in records])
    dry = np.stack([r['dry'] for r in records])
    for r in records: r.close()
    def curve(w,d):return 10*np.log10(w/d)
    def score(w,d):return np.sqrt(curve_mse(curve(w,d),fit,EDGES))
    window_scores = [score(wet[:,:,j].mean(0),dry[:,j].mean(0)) for j in range(4)]
    w,d = wet[:,:,2:].mean(2),dry[:,2:].mean(1)
    truth = curve(w.mean(0), d.mean(0))
    rms = score(w.mean(0), d.mean(0))
    bootstrap = []
    rng = np.random.default_rng(90311881)
    for _ in range(2000):
        ix = rng.integers(TRIALS,size=TRIALS)
        bootstrap.append(score(w[ix].mean(0),d[ix].mean(0)))
    ci = np.quantile(bootstrap,[.025,.975],axis=0).T
    halves = [score(w[a:b].mean(0), d[a:b].mean(0)) for a,b in ((0,32),(32,64))]
    split = np.sqrt(curve_mse(curve(w[:32].mean(0),d[:32].mean(0)),curve(w[32:].mean(0),d[32:].mean(0)),EDGES))
    diff = np.maximum(truth,-60)-np.maximum(fit,-60)
    result = []
    for i,p in enumerate(SETTINGS):
        j = int(np.argmax(np.abs(diff[i])))
        result.append(dict(controls=p.tolist(), rms=float(rms[i]), bootstrap95=ci[i].tolist(),
            window_rms=[float(a[i]) for a in window_scores],half_rms=[float(a[i]) for a in halves],
            half_disagreement=float(split[i]),largest_center_hz=float(centers[j]),measured_minus_fit_db=float(diff[i,j])))
    report=dict(trials=TRIALS, duration=width*WINDOWS/192000,window_starts=[j*width/192000 for j in (1,3,6,7)],
        window_seconds=width/192000, rows=result)
    (out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    np.savez_compressed(out/'curves.npz',settings=SETTINGS,measured=truth,fitted=fit,centers=centers)
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('experiment',type=Path)
    ap.add_argument('out',type=Path)
    opt=ap.parse_args()
    main(opt.experiment.resolve(),opt.out.resolve())
