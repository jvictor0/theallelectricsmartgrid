"""Test the unrestricted finite-block power cap at 249.70 Hz on a legal noise word."""
import argparse
import json
from pathlib import Path
from fractions import Fraction as Q

import numpy as np

from record_bank import TAIL, bind, curve_mse
from fresh_dry_power import sin_pi, HOST, NATIVE
from lagged_records import shifted_bands
from survey import EDGES, EPOCH, atomic_json
from wet_block_audit import checked_analyzer


def main(root, library, checked_fft=None, output=None):
    old = root.parent/'survivor-replay-250'
    with np.load(old/'fresh-parents.npz') as z:
        knobs = z['settings'][0]
        anchor = (z['base']+z['delta'].sum(1)).mean(0)[0]
    dry = json.loads((old/'dry-power.json').read_text())
    dry_mid = np.sqrt(np.array(dry['dry_power_lower'])*dry['dry_power_upper'])
    with np.load(root.parent.parent/'physics-v1/analysis-coefficients.npz') as z:
        analyzer = [z[k].astype(float) for k in ('first','second','window')]
    measure = (checked_analyzer(analyzer,checked_fft) if checked_fft else
               lambda samples,past,offset: shifted_bands(samples,past,offset,analyzer))
    lib = bind(library)
    period = 4096
    assert NATIVE%period == 0 and 0 < NATIVE//period < HOST//2
    phase = 6*np.arange(period)+2+3*period//2
    lo, hi = sin_pi(phase, 3*period)
    word = (.25*lo).astype('f4')
    np.testing.assert_array_equal(word, (.25*hi).astype('f4'))
    # Each exact sample lies strictly inside its binary32 rounding cell.
    previous = np.nextafter(word, np.float32(-np.inf)).astype(float)
    following = np.nextafter(word, np.float32(np.inf)).astype(float)
    assert np.all(previous+word < .5*lo) and np.all(.5*hi < following+word)
    x = np.tile(word,NATIVE//period)
    y = np.empty_like(x)
    f = lib.CreateCycleFilter(*knobs)
    try:
        lib.RunCycleRecord(f,len(x),x,y)  # one allowable initial record phase
        for _ in range(4):
            lib.RunCycleRecord(f,len(x),x,y)
        values = []
        for _ in range(4):
            past = y[-TAIL:].copy()
            lib.RunCycleRecord(f,len(x),x,y)
            values.append(measure(y,past,0))
    finally:
        lib.DestroyCycleFilter(f)
    tone = np.mean(values, axis=0)
    ref = 10*np.log10(anchor/dry_mid)
    def display_error(mass):
        db = 10*np.log10(((1-mass)*anchor+mass*tone)/dry_mid)
        return float(np.sqrt(curve_mse(ref[None],db[None],EDGES)[0]))
    a,b = 0.,.01
    assert display_error(b) > .346784
    for _ in range(48):
        m=(a+b)/2
        if display_error(m)>.346784:
            b=m
        else:
            a=m
    # The phase/sign of this single Fourier mode is arbitrary for the following
    # bound. Its 2D energy fraction has Beta(1,(HOST-4)/2) law by spherical symmetry.
    # Binary32 rounding changes the normalized record by < 2^-23 in relative L2.
    # Any ideal direction yielding exactly this word has > 99% energy in that plane.
    assert 4*Q(1,2**23) < Q(1,100)
    j=int(np.argmax(tone/anchor))
    report=dict(frequency_hz=192000/period, max_ratio=float(max(tone/anchor)),
                worst_band_hz=float(np.sqrt(EDGES[j]*EDGES[j+1])),
                full_curve_error_db=display_error(1), hypothetical_contamination_breaks_at=b,
                probability_1024_miss_hypothetical_contamination=(1-b)**1024,
                one_record_99pct_mode_energy_probability_log10=-(HOST-4))
    report.update(checked_fft=checked_fft is not None,worst_band=j)
    atomic_json(output or root/'support-witness.json',report)
    print(report)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--library',type=Path,default=Path('build/tools/ladder-noise-grid/cycle.so'))
    parser.add_argument('--checked-fft',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    main(args.root,args.library,args.checked_fft,args.output)
