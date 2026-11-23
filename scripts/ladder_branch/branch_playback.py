"""Perturb the three digital cycles, compare amplifier feedback and release the forcing."""
import argparse
import json
from pathlib import Path

import numpy as np

from pilot import ACTIVE,AMPLITUDE,LIB,RATE,Tone
from switch_frequency import CUTOFF,DRIVE,SCALE,TARGET,PhaseGrid,resample
from switch_probe import refine_cycle


def run(seeds):
    q=562
    t=Tone(q,CUTOFF,DRIVE)
    t.set_resonance(TARGET)
    grid=PhaseGrid(q,3)
    grid.frequency(RATE/q)
    result=[]
    with np.load(seeds) as waves:
        for branch in ('outgoing','returning','quiet'):
            arr=waves['341.637010676-'+branch]
            z,_=refine_cycle(grid,resample(arr,grid.phase)[:,ACTIVE]/SCALE,TARGET,np.arange(q),None)
            grid.system(z[:,ACTIVE]/SCALE,TARGET)
            _,k,_,_,_,_,_,_,ref,damping,floor=t.coeffs
            effective=k/(1+damping*np.maximum(0,z[:,13]-ref*z[:,8])/(z[:,8]+floor))
            stage_input=t.input-effective*z[:,3]
            fundamental=lambda v:2*np.mean(v*t.phase)
            matrix=np.eye(9)
            for j in grid.jac:matrix=j@matrix
            initial=z[0].copy()
            initial[0]+=1e-9
            t.period(initial,round(2*RATE/q))
            forced=t.output.copy()
            silent=np.zeros(2*RATE)
            after=np.empty_like(silent)
            memory=z[0].copy()
            LIB.ReturnMap(*t.params,silent,len(silent),1,memory,after)
            window=after[-RATE:]
            spectrum=abs(np.fft.rfft(np.hanning(len(window))*window))
            freq=np.argmax(spectrum[1:])+1
            result.append(dict(branch=branch,wet_multiplier=max(abs(np.linalg.eigvals(matrix))),
                               residual_drive_fund_amplitude=abs(fundamental(stage_input)),
                               residual_drive_relative_to_input=abs(fundamental(stage_input))/AMPLITUDE,
                               feedback_fund_amplitude=abs(fundamental(effective*z[:,3])),
                               feedback_fund_phase_deg=np.angle(fundamental(effective*z[:,3]),deg=True),
                               stage4_fund_amplitude=abs(fundamental(z[:,3])),
                               stage4_third_amplitude=abs(2*np.mean(z[:,3]*t.phase**3)),
                               forced_fund_db=20*np.log10(abs(2*np.mean(forced*t.phase))/AMPLITUDE),
                               forced_total_db=10*np.log10(2*np.mean(forced**2)/AMPLITUDE**2),
                               release_peak_hz=int(freq),release_rms_dbfs=10*np.log10(np.mean(window**2)),
                               original_fund_db=20*np.log10(abs(2*np.mean(grid.y*t.phase))/AMPLITUDE)))
    return result


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('seeds',type=Path)
    ap.add_argument('out',type=Path)
    a=ap.parse_args()
    rows=run(a.seeds)
    a.out.write_text(json.dumps(rows,indent=2)+'\n')
    for r in rows:print(r)
