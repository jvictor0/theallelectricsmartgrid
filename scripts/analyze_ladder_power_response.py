#!/usr/bin/env python3
"""Check the averaged phase-only power curve against the full sample map.

This deliberately tests more than the strongest-drive oscillator. No response
data is fitted. The prediction omits driven waveform deformation, so these
checks determine where an amplitude response is necessary. Numerical evidence
only; this is not a runtime evaluator or a certified error bound.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np

from analyze_ladder_harmonic_reduction import Model, basis, forced_check


def coherence_power(r):
    r = abs(r)
    return 1.0 if r <= 1 else (1/(r+math.sqrt(r*r-1)))**2


def total_orbit_power(model, orbit):
    theta = 2*np.pi*np.arange(16384)/16384
    waveform = basis(theta, np.arange(1, orbit['highest']+1, 2))@np.array(orbit['coefficients'])
    output = (1+model.k)*model.sat(waveform[:, 3])
    return float(np.mean(output**2))


def compare(model, orbit, check, baseline_power):
    m = check['harmonic']
    width = check['amplitude']/0.25*orbit['phase']['halfWidthHzAtAmplitudeQuarter'][str(m)]
    offset = (check['frequencyHz']-m*orbit['frequencyHz'])/width
    power = orbit['outputPowers'][str(m)]*coherence_power(offset)
    return check | {
        'model': model.kind,
        'normalizedOffset': offset,
        'predictedCoherentPower': power,
        'coherentErrorDb': 10*math.log10(power/check['inputFrequencyPower']),
        'predictedTotalPower': baseline_power,
        'totalErrorDb': 10*math.log10(baseline_power/check['totalPower']),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--refine', action='store_true', help='Ten-second Hann/rectangular checks')
    mode.add_argument('--periodic', action='store_true', help='Whole repeating-period checks near the weak seventh peak')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    data = root/'build/results/fundamental-third'
    out = root/'build/results/ladder-power-response'
    out.mkdir(parents=True, exist_ok=True)
    models = json.loads((data/'reduction.json').read_text())
    if args.refine or args.periodic:
        results = []
        for row in models:
            if row['drive'] == 1 or (args.periodic and row['drive'] != 0.5):
                continue
            model = Model(row['model'], row['drive'], row['resonance'])
            orbit = row['orbits'][-1]
            if args.periodic:
                cycles = 2000
                period = round(model.fs*cycles/orbit['frequencyHz'])
                frequency = 7*cycles*model.fs/period
                for amplitude in [0.0625, 0.125, 0.25, 0.5]:
                    check = forced_check(model, amplitude, frequency, 7,
                                         warm=period*4, count=period*2, period=(period, 7*cycles))
                    check.update(model=model.kind, drive=row['drive'], resonance=row['resonance'],
                                 oscillatorCycles=cycles, predictedPower=orbit['outputPowers']['7'],
                                 errorDb=10*math.log10(orbit['outputPowers']['7']/check['inputFrequencyPower']))
                    results.append(check)
                    print(json.dumps(check), flush=True)
            else:
                width = orbit['phase']['halfWidthHzAtAmplitudeQuarter']['7']
                for offset in [-1.2, 0, 1.2]:
                    frequency = 7*orbit['frequencyHz']+offset*width
                    check = forced_check(model, 0.25, frequency, 7, warm=384000, count=1920000)
                    predicted = orbit['outputPowers']['7']*coherence_power(offset)
                    check.update(model=model.kind, drive=row['drive'], resonance=row['resonance'],
                                 offset=offset, predictedPower=predicted,
                                 rectangularErrorDb=10*math.log10(predicted/check['inputFrequencyPower']),
                                 hannErrorDb=10*math.log10(predicted/check['windowedInputFrequencyPower']))
                    results.append(check)
                    print(json.dumps(check), flush=True)
        (out/('periodic.json' if args.periodic else 'refine.json')).write_text(json.dumps(results, indent=2)+'\n')
        return
    existing = json.loads((data/'locking.json').read_text())
    results = []
    for row in models:
        model = Model(row['model'], row['drive'], row['resonance'])
        orbit = row['orbits'][-1]
        baseline = total_orbit_power(model, orbit)
        if row['drive'] == 1:
            checks = [x for x in existing if x['model'] == model.kind]
        else:
            checks = []
            for amplitude in [0.125, 0.25]:
                for harmonic in [3, 5, 7]:
                    width = amplitude/0.25*orbit['phase']['halfWidthHzAtAmplitudeQuarter'][str(harmonic)]
                    for offset in [-1.2, 0, 1.2]:
                        checks.append(forced_check(model, amplitude,
                            harmonic*orbit['frequencyHz']+offset*width, harmonic))
        for check in checks:
            result = compare(model, orbit, check, baseline)
            result.update(drive=row['drive'], resonance=row['resonance'])
            results.append(result)
            print(json.dumps(result), flush=True)
    (out/'power.json').write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()
