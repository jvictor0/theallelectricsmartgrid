"""Audit the deployed survey protocol against a frozen lag-corrected reference at one fixed control."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction
import hashlib
import json
from pathlib import Path

import numpy as np

from record_bank import EDGES, EPOCH, TAIL, bands, bind, curve_mse, make_noise


def main(results, library, output, survivor_audit=None):
    reference_path = results/'screen-v3/lagged-fresh-extended/pairs.npz'
    seed = 923856107
    provenance = {}
    if survivor_audit is None:
        with np.load(reference_path) as z:
            # The first preselected stress case had the longest observed shared-input memory.
            knobs = z['settings'][0]
            wet, dry = (z['base'][:,0] + z['delta'][:,0].sum(axis=1)).mean(axis=0)
    else:
        seed = 1570932406
        reference_path = survivor_audit/'fresh-parents.npz'
        with np.load(reference_path) as z:
            knobs = z['settings'][0]
            wet = (z['base']+z['delta'].sum(axis=1)).mean(axis=0)[0]
        dry_path = survivor_audit/'dry-power.json'
        dry_bounds = json.loads(dry_path.read_text())
        dry = np.sqrt(np.array(dry_bounds['dry_power_lower'])*dry_bounds['dry_power_upper'])
        provenance['dry_hash'] = hashlib.sha256(dry_path.read_bytes()).hexdigest()
    assert np.all(wet > 0) and np.all(dry > 0)
    reference = 10*np.log10(wet/dry)
    ref_hash = hashlib.sha256(reference_path.read_bytes()).hexdigest()
    with np.load(results/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first','second','window')]
    lib = bind(library)
    groups, repetitions = 72, 16
    start = 6*EPOCH
    def probe(index):
        # Flatten independent SeedSequence children onto separate records to avoid
        # allocating a full group of native sample buffers in every worker.
        noise = make_noise(seed+index, 1, 14).ravel()
        y = np.empty_like(noise)
        lib.Reference(*knobs, len(noise), noise, y)
        return np.stack([bands(s[start:], s[start-TAIL:start], analyzer) for s in (y,noise)])

    with ThreadPoolExecutor(6) as pool:
        powers = np.stack(list(pool.map(probe, range(groups*repetitions))))
    means = powers.reshape(groups,repetitions,2,-1).mean(axis=1)
    if not (np.isfinite(means).all() and np.all(means > 0)):
        raise ArithmeticError('Invalid band powers')
    curves = 10*np.log10(means[:,0]/means[:,1])
    errors = np.sqrt(curve_mse(curves, np.broadcast_to(reference,curves.shape), EDGES))
    upper = float(np.ceil(1000*errors.max())/1000)
    tail = Fraction(95,100)**groups
    assert tail < Fraction(1,40)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output/'powers.npz', means=means, reference=reference, errors=errors)
    report = dict(knobs=knobs.tolist(), seed=seed, groups=groups, records_per_group=repetitions,
        reference_hash=ref_hash, warm_seconds=start/192000, measure_seconds=8*EPOCH/192000,
        median_visible_rms_db=float(np.median(errors)), observed_max_db=float(errors.max()),
        outward_tolerance_db=upper, coverage=0.95, calibration_failure_upper=float(tail), **provenance)
    (output/'summary.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    parser.add_argument('library', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--survivor-audit', type=Path)
    args = parser.parse_args()
    main(args.results, args.library, args.output, args.survivor_audit)
