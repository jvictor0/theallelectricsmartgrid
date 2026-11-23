"""Score a fixed prefix-sample of completed midpoint tiles against the frozen model."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np


def main(root, examples):
    sys.path.insert(0, str(root/'src'))
    from display_metric import curve_mse
    sys.path.insert(0, str(root/'fits/physics-v1'))
    from response import response_db
    grid = root/'results/cutoff-bands-73x73x2881'
    fit = root/'fits/screen-v3/display-model.npz'
    digest = hashlib.sha256(fit.read_bytes()).hexdigest()
    if digest != '1e3c6fb810e4dc8ef373235ec398b691fd930fa94d577a22b8997eacd284e2ec':
        raise ValueError('Wrong frozen model')
    with np.load(fit) as z:
        m = dict(z)
    def neighbors(axis, x):
        j = np.clip(np.searchsorted(axis, x, side='right')-1, 0, len(axis)-2)
        t = (x-axis[j])/(axis[j+1]-axis[j])
        return [(j, 1-t), (j+1, t)]
    def predict(ids):
        d,r,c = ids.T
        out = np.zeros((len(d),120))
        for ci,cw in neighbors(m['ca'],c/8):
            for cut in np.unique(ci):
                ix = ci == cut
                lo, hi = m['offset'][cut:cut+2]
                scores = np.zeros((int(hi-lo), int(ix.sum())))
                for di,dw in neighbors(m['da'], d[ix]):
                    for ri,rw in neighbors(m['ra'], r[ix]):
                        scores += m['score'][lo:hi, di, ri]*(dw*rw)
                out[ix] += (scores.T@m['basis'][lo:hi]/(127*128) + m['center'][cut]/128)*cw[ix,None]
        return out
    edges = np.r_[20*2**(np.arange(120)/12),20000.]
    # Fixed before looking at the midpoint errors. The acquisition shuffles the parent tiles.
    files = [grid/f'offset-4-tile-{i:06d}.npz' for i in range(3000)]
    if not all(path.is_file() for path in files):
        raise ValueError('The first 3000 midpoint tiles are incomplete')
    files = files[::41]
    errors, peaks, ids_seen, halves = [], [], [], []
    maxima = {}
    for path in files:
        with np.load(path) as z:
            ids = z['ids'].astype(np.int64)
            wet = z['band_halves'].astype(float)
            dry = z['dry_band_halves'].astype(float)
        if not np.all(ids[:,2]%8 == 4):
            raise ValueError('Expected only new midpoints')
        measured = 10*np.log10(wet.sum(1)/dry.sum(0))
        pred = predict(ids)
        d,r,c = ids[0]
        check = response_db(m, d/72, r/72, 20*1000**(c/2880), np.sqrt(edges[:-1]*edges[1:]))
        np.testing.assert_allclose(pred[0], check, rtol=0, atol=1e-9)
        rms = np.sqrt(curve_mse(measured,pred,edges))
        residual = np.max(abs(np.maximum(measured,-60)-np.maximum(pred,-60)), axis=1)
        errors.extend(rms)
        peaks.extend(residual)
        half = 10*np.log10(wet/dry)
        halves.extend(np.sqrt(curve_mse(half[:,0],half[:,1],edges)))
        ids_seen.extend(ids.tolist())
        if examples:
            for metric, scores in [('whole_curve',rms), ('single_band',residual)]:
                j = int(scores.argmax())
                if metric not in maxima or scores[j] > maxima[metric]['score']:
                    maxima[metric] = dict(score=float(scores[j]), ids=ids[j].tolist(),
                        measured=measured[j].tolist(), fitted=pred[j].tolist(), halves=half[j].tolist())
    e = np.array(errors)
    if len(set(map(tuple,ids_seen))) != len(e):
        raise ValueError('Duplicate controls')
    if examples:
        print(json.dumps(dict(model=digest, points=len(e), edges=edges.tolist(), examples=maxima), allow_nan=False))
        return
    def coord(i):
        d,r,c = ids_seen[i]
        return [d/72,r/72,20*1000**(c/2880)]
    print(json.dumps(dict(tiles=len(files), first=files[0].name, last=files[-1].name, points=len(e),
        model=digest, rms=float(np.sqrt(np.mean(e**2))), p95=float(np.quantile(e,.95)),
        worst=float(e.max()), worst_controls=coord(int(e.argmax())), max_center_db=float(max(peaks)),
        worst_center_controls=coord(int(np.argmax(peaks))), seed_half_rms=float(np.sqrt(np.mean(np.square(halves)))),
        above_one_db=int(np.sum(e>1))), indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('survey_root', type=Path)
    parser.add_argument('--examples', action='store_true', help='Emit the spectra at both error maxima')
    args = parser.parse_args()
    main(args.survey_root.resolve(), args.examples)
