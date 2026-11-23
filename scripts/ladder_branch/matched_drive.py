"""Compare first crossings numerically on two arclength meshes at matched origin gain."""
import argparse
import csv
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

from drive_turns import ORIGINAL,drive_knob,resonance_knob
from multiple_shooting import continue_blocks
from pilot import AMPLITUDE,RATE,Tone
from switch_frequency import CUTOFF,TARGET
from turning_points import follow


def test(spec):
    q,g,L,h = spec
    t = Tone(q,CUTOFF,drive_knob(g))
    s = g/t.coeffs[3]
    knob = resonance_knob(L/s**3)
    result = follow(t,h,knob)
    row = dict(period=q,hz=RATE/q,g=g,L=L,knob=knob,step=h,termination=result['reason'],turn_count=len(result['turns']),
               first_turn=result['turns'][0][1] if result['turns'] else '',fund_db='',total_db='',third_fraction='',phase_deg='',closure='',method='single')
    if result['reason']=='target':
        t.set_resonance(knob)
        t.period(np.array(result['target']['state']))
        y = t.output.copy()
        error=result['target']['full_return_residual']
    else:
        extra=continue_blocks(t,dict(result,step=h),knob,56 if h>.02 else 44)
        row.update(method='segmented',termination=extra['reason'])
        if extra['reason']!='target':
            return row
        y=np.array(extra['output'])
        error=extra['block_residual']
    if row['termination']=='target':
        ph = 2*np.mean(y*t.phase)
        row.update(fund_db=20*np.log10(abs(ph)/AMPLITUDE),total_db=10*np.log10(2*np.mean(y*y)/AMPLITUDE**2),
                   third_fraction=2*abs(np.mean(y*t.phase**3))**2/np.mean(y*y),
                   phase_deg=np.angle(ph,deg=True),closure=error)
    return row


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    ap.add_argument('--fixed-knob',action='store_true')
    a=ap.parse_args()
    t=Tone(562,CUTOFF,drive_knob(ORIGINAL))
    t.set_resonance(TARGET)
    hot=t.coeffs[1]*(ORIGINAL/t.coeffs[3])**3
    cases=[(g,L,qs) for g,L,qs in [(3.,hot,[562,561,180,179,178]),(2.5,60.,[561,562,183,182,181]),
                                      (3.,60.,[562,183,184]),(ORIGINAL,60.,[562,183,184,190,192,194])]]
    if a.fixed_knob:
        cases=[]
        for g,periods in [(2.5,[561,562,565,566,567,568,569,570,571,188,189,190]),
                          (3.,[561,562,565,566,567,568]),(ORIGINAL,[561,562])]:
            t=Tone(562,CUTOFF,drive_knob(g))
            t.set_resonance(TARGET)
            cases.append((g,t.coeffs[1]*(g/t.coeffs[3])**3,periods))
    with a.out.open('w',newline='') as file,ProcessPoolExecutor(5) as pool:
        writer=None
        for row in pool.map(test,[(q,g,L,h) for g,L,qs in cases for q in qs for h in (.032,.016)]):
            if writer is None:
                writer=csv.DictWriter(file,fieldnames=row);writer.writeheader()
            writer.writerow(row);file.flush()
