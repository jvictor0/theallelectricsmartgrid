"""Test whether per-sample state boxes can bound the wet spectrum after warm-up."""
import argparse
import ctypes as ct
import json
from pathlib import Path

import numpy as np

from record_bank import EDGES, EPOCH, F32, TAIL, bands, curve_mse, make_noise
from stress import SETTINGS


def main(results, library, records):
    if records < 2:
        raise ValueError('Use at least two common records')
    dll = ct.CDLL(str(library.resolve()))
    run = dll.WetHistoryBox
    run.argtypes = [ct.c_double]*3 + [ct.c_int]*2 + [F32]*7
    run.restype = ct.c_int
    with np.load(results/'physics-v1/analysis-coefficients.npz') as coefficients:
        analyzer = [coefficients[k].astype(float) for k in ('first', 'second', 'window')]
    seed = 658412
    def draw(i):
        return make_noise(seed+i, 1, 14).ravel()
    first, second = draw(0), draw(1)
    common = np.concatenate([draw(i+2) for i in range(records)])
    fits = np.load(results/'screen-v3/physical-stress/curves.npz')['fitted'][:3]
    rows = []
    for index, knobs in enumerate([*SETTINGS[:3], [SETTINGS[1,0], 0., SETTINGS[1,2]]]):
        wet, other, low, high = [np.empty_like(common) for _ in range(4)]
        if run(*knobs, len(first), len(common), first, second, common, wet, other, low, high):
            raise ArithmeticError('Nonfinite stock evaluation')
        if not all(np.isfinite(x).all() for x in (wet, other, low, high)):
            raise ArithmeticError('Nonfinite output')
        assert np.all(low<=np.minimum(wet, other)) and np.all(np.maximum(wet, other)<=high)
        # Closest-to-zero waveform in the product of the marginal signal intervals.
        quiet = np.clip(0., low, high)
        last_different = np.flatnonzero(wet.view('u4') != other.view('u4'))
        row = dict(knobs=list(knobs), last_unequal_audio_seconds=(
            (int(last_different[-1])+1)/192000 if len(last_different) else 0.), windows=[])
        for start in (6*EPOCH, (records-1)*len(first)+6*EPOCH):
            end = start+8*EPOCH
            def power(x):
                return bands(x[start:end], x[start-TAIL:start], analyzer)
            dry = power(common)
            def curve(x):
                with np.errstate(divide='ignore'):
                    return 10*np.log10(power(x)/dry)
            ref = curve(wet)
            boxed = curve(quiet)
            silent = np.all(quiet[start-TAIL:end]==0.)
            info = dict(start_seconds=start/192000, window_seconds=(end-start)/192000,
                zero_allowed_on_whole_window_and_fir_tail=bool(silent),
                zero_allowed_fraction=float(np.mean(quiet[start-TAIL:end]==0.)),
                true_pair_max_abs=float(np.max(abs(wet[start:end]-other[start:end]))),
                true_pair_curve_rms_db=float(np.sqrt(curve_mse(ref[None], curve(other)[None], EDGES)[0])))
            if index < len(fits):
                def rms(a, b):
                    return float(np.sqrt(curve_mse(a[None], b[None], EDGES)[0]))
                info['measured_fit_rms_db'] = rms(ref, fits[index])
                if silent:
                    info['interval_witness_fit_rms_db'] = rms(np.full_like(ref, -60.), fits[index])
                elif np.isfinite(boxed).all():
                    info['interval_witness_fit_rms_db'] = rms(boxed, fits[index])
            row['windows'].append(info)
        rows.append(row)
    print(json.dumps(dict(seed=seed, common_records=records, rows=rows), indent=2, allow_nan=False))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('results', type=Path)
    ap.add_argument('library', type=Path)
    ap.add_argument('--records', type=int, default=6)
    args = ap.parse_args()
    main(args.results, args.library, args.records)
