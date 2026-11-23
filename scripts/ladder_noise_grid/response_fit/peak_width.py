"""Measure the log-frequency width of visible errors, including interpolated floor crossings."""
import argparse
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from display_metric import DISPLAY_FLOOR_DB, curve_mse
from survey import EDGES


def visible_segments(centers, a, b):
    x = np.log2(np.r_[EDGES[0], centers, EDGES[-1]])
    a, b = np.pad(a, 1, mode='edge'), np.pad(b, 1, mode='edge')
    for i, width in enumerate(np.diff(x)):
        cuts = {0., 1.}
        for v in (a, b):
            dy = v[i+1] - v[i]
            if dy:
                cuts.add(np.clip((DISPLAY_FLOOR_DB-v[i])/dy, 0., 1.))
        cuts = np.array(sorted(cuts))
        left = np.maximum(DISPLAY_FLOOR_DB, a[i]+cuts*(a[i+1]-a[i]))
        right = np.maximum(DISPLAY_FLOOR_DB, b[i]+cuts*(b[i+1]-b[i]))
        errors = left-right
        for lo, hi, e0, e1 in zip(cuts, cuts[1:], errors, errors[1:]):
            yield x[i]+lo*width, x[i]+hi*width, e0, e1


def summarize(centers, measured, fit):
    pieces = list(visible_segments(centers, measured, fit))
    span = np.log2(EDGES[-1]/EDGES[0])
    area = sum((b-a)*(p*p+p*q+q*q)/3 for a, b, p, q in pieces)
    rms = np.sqrt(area/span)
    np.testing.assert_allclose(rms, np.sqrt(curve_mse(measured[None], fit[None], EDGES)[0]), atol=1e-11)
    peak, location = max(((e, x) for a,b,p,q in pieces for x,e in ((a,p),(b,q))), key=lambda t:abs(t[0]))
    row = dict(rms_db=float(rms), measured_minus_fit_db=float(peak), peak_hz=float(2**location), widths={})
    for threshold in (1., 3., 6.):
        runs = []
        for a, b, p, q in pieces:
            cuts = {0., 1.}
            if p != q:
                cuts.update(np.clip((t-p)/(q-p), 0., 1.) for t in (-threshold,threshold))
            cuts = sorted(cuts)
            for lo, hi in zip(cuts, cuts[1:]):
                if abs(p+.5*(lo+hi)*(q-p)) <= threshold:
                    continue
                start, end = a+(b-a)*lo, a+(b-a)*hi
                if runs and abs(start-runs[-1][1]) < 1e-12:
                    runs[-1][1] = end
                else:
                    runs.append([start, end])
        width = sum(end-start for start, end in runs)
        if width/span > (rms/threshold)**2 + 1e-12:
            raise ArithmeticError('Threshold measure exceeds the mean-square bound')
        row['widths'][f'{threshold:g}'] = dict(percent=float(100*width/span),
            octave_width=float(width), hz_intervals=[[float(2**a),float(2**b)] for a,b in runs])
    return row


def main(root, output):
    with np.load(root/'physical-stress/curves.npz') as data:
        fit, longer, centers = data['fitted'][:3], data['measured'][:3], data['centers']
        settings = data['settings'][:3].tolist()
    with np.load(root/'lagged-fresh-extended/pairs.npz') as data:
        power = (data['base'] + data['delta'].sum(axis=2)).mean(axis=0)
        assert np.all(power > 0)
        fresh = 10*np.log10(power[:,0]/power[:,1])
    result = dict(settings=settings, fresh=[summarize(centers, a, b) for a,b in zip(fresh,fit)],
        long_records=[summarize(centers, a, b) for a,b in zip(longer,fit)])
    output.write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    for row in result['fresh']:
        print(row['peak_hz'], row['measured_minus_fit_db'], [(k, v['percent']) for k,v in row['widths'].items()])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('screen_results', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    main(args.screen_results, args.output)
