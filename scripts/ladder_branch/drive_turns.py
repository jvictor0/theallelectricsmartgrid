"""Trace the detector-on first resonance turn as drive and the input tone vary."""
import argparse
import csv
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
import numpy as np

from pilot import RATE, Tone
from switch_frequency import CUTOFF, DRIVE
from turning_points import follow

ORIGINAL = .25*20**DRIVE


def drive_knob(g):
    return np.log(4*g)/np.log(20)


def resonance_knob(k):
    return np.log1p(48*k/3.9)/np.log(49)


def trace(args):
    q,g,step = args
    t = Tone(q,CUTOFF,drive_knob(g))
    s = g/t.coeffs[3]
    result = follow(t,step,1.,after_first_turn=0.)
    path = result['path']
    i = int(np.argmax([p['r'] for p in path]))
    peak = path[i]
    t.set_resonance(peak['r'])
    return dict(period=q,input_hz=RATE/q,g=g,step=step,termination=result['reason'],samples=len(path),
                knob_at_max=peak['r'],loop_gain_at_max=t.coeffs[1]*s**3,closure=peak['residual'])


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    ap.add_argument('--step',type=float,default=.024)
    ap.add_argument('--periods',nargs='+',type=int,default=[182,183,190,550,558,561,562,569,582,600])
    ap.add_argument('--gains',nargs='+',type=float,default=[ORIGINAL,3.,2.5])
    a = ap.parse_args()
    with a.out.open('w',newline='') as f, ProcessPoolExecutor(5) as pool:
        writer = None
        for row in pool.map(trace,[(q,g,a.step) for q in a.periods for g in a.gains]):
            if writer is None:
                writer = csv.DictWriter(f,fieldnames=row)
                writer.writeheader()
            writer.writerow(row)
            f.flush()
