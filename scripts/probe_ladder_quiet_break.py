"""Probe sensitivity to roundoff on the high-input/low-cutoff corner of the map."""
import ctypes as ct
import json
from pathlib import Path
import sys
import numpy as np
from replay_ladder_breaks import ROOT, RATE, PTR, ptr, measurements, PeriodMap


def run(root):
    meta = json.loads((root.parent/'manifest.json').read_text())
    lib=ct.CDLL(str(ROOT/'build/tools/ladder-break-replay/libladder_break_replay.so'))
    lib.LadderBreakPrepare.argtypes=[ct.c_double,ct.c_double,PTR,PTR]
    lib.LadderBreakStep.argtypes=[PTR,PTR,PTR,ct.c_int,ct.c_int,ct.c_int,PTR]
    lib.LadderBreakTone.argtypes=[ct.c_double,ct.c_double,PTR,ct.c_int]
    lib.LadderBreakStep.restype=lib.LadderBreakPrepare.restype=None
    tone=meta['frequencies'][90]
    amp=meta['amplitude']
    p=np.zeros(5,np.float32)
    ks=np.empty(10000,np.float32)
    lib.LadderBreakPrepare(90/99,20.,ptr(p),ptr(ks))
    buf=np.empty(10000,np.float32)
    q=lib.LadderBreakTone(tone['target_hz'],amp,ptr(buf),len(buf))
    state=np.zeros(4,np.float32)
    phase=0
    def step(s,ph,count,y=None):
        lib.LadderBreakStep(ptr(p),ptr(s),ptr(buf),q,ph,count,ptr(y) if y is not None else None)
        return (ph+count)%q
    p[2]=ks[0]
    phase=step(state,phase,48000)
    for i in range(18*101+1):
        p[2]=ks[i]
        phase=step(state,phase,RATE)
    model=PeriodMap(dict(coefficients=p.tolist(),input=buf[:q].tolist(),quadrature=np.zeros(q)))
    root_state,err,_=model.fit(np.zeros(4))
    assert err < 1e-10
    _,j,_=model.cycle(root_state)
    results={'radius':float(max(abs(np.linalg.eigvals(j)))),'holds':{}}
    for kick in [0.,1e-10,1e-7,-1e-7]:
        copy=state.copy()
        copy[3]+=kick
        ph=step(copy,phase,11*RATE)
        y=np.empty(RATE,np.float32)
        step(copy,ph,RATE,y)
        results['holds'][str(kick)]=measurements(y,q,tone['actual_hz'],amp)
    (root/'quiet-probe.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':
    run(Path(sys.argv[1]))
