"""Distribution-free tolerance limit for the frozen fit on the held-out finite stock probe."""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from display_metric import curve_mse
from survey import EDGES


def main(results):
    physics = results/'physics-v1'
    sys.path.insert(0, str(physics))
    from response import load_model, response_db
    screen = results/'screen-v3'
    path = screen/'display-model.npz'
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != json.loads((screen/'certificate.json').read_text())['model_hash']:
        raise ValueError('Unexpected frozen fit')
    model = load_model(path)
    centers = np.sqrt(EDGES[:-1]*EDGES[1:])
    with np.load(physics/'stock-curves.npz') as z:
        coords = z['coords'].reshape(96, 2, 3)
        measurements = z['stock'].reshape(96, 2, 120)
    maxima = []
    # Only the first sixteen acquisition groups were used in fit selection.
    for group in range(16,96):
        rng = np.random.default_rng(np.random.SeedSequence([872310771,group]))
        expected = rng.random((48,3))[:2]
        expected[:,2] = 20*1000**expected[:,2]
        np.testing.assert_allclose(coords[group], expected, rtol=2e-14, atol=0)
        fit = np.stack([response_db(model,*p,centers) for p in coords[group]])
        maxima.append(float(np.sqrt(curve_mse(fit,measurements[group],EDGES)).max()))
    # For iid group maxima, P(F(maximum observed) < p) <= p^n, including ties.
    coverage = Fraction(9632,10000)
    failure = coverage**len(maxima)
    assert failure <= Fraction(1,20)
    print(json.dumps(dict(model=digest, independent_groups=len(maxima), settings_per_group=2,
        records_per_setting=16, largest_group_rms_db=max(maxima), outward_db=float(np.ceil(1000*max(maxima))/1000),
        lower_coverage=float(coverage), failure_probability_upper=float(failure)), indent=2, allow_nan=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    main(parser.parse_args().results)
