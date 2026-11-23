"""Trace a detector-on shooting branch through turns of the resonance knob."""
import argparse
import json
import gzip
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
import numpy as np
from pilot import ACTIVE, RATE, Tone, diagnostics, solve


def follow(t, step_max, target, after_first_turn=None):
    scale=np.array([1.]*8+[.5])
    z=np.r_[t.anchor[ACTIVE]/scale,0.]
    direction=np.r_[np.zeros(9),1.]
    step=step_max
    turns=[]
    arc=0.
    prev_dr=1.
    rows=[]
    def system(p):
        state=t.anchor.copy()
        state[ACTIVE]=p[:9]*scale
        h=1e-6
        t.set_resonance(p[-1]+h)
        plus=t.period(state)
        t.set_resonance(p[-1]-h)
        minus=t.period(state)
        t.set_resonance(p[-1])
        dr=(plus-minus)[ACTIVE]/(2*h*scale)
        jac=t.double_jacobian(state,scale)
        end=t.period(state)
        return (end-state)[ACTIVE]/scale,np.column_stack((jac-np.eye(9),dr)),state
    for _ in range(3000):
        residual,jac,state=system(z)
        tangent=np.linalg.svd(jac,full_matrices=True)[2][-1]
        if tangent@direction<0: tangent=-tangent
        if prev_dr*tangent[-1]<0:
            turns.append([arc,float(z[-1])])
        rows.append(dict(arc=arc,r=float(z[-1]),tangent_r=float(tangent[-1]),residual=float(max(abs(residual))),state=state.tolist()))
        if after_first_turn is not None and turns and arc-turns[0][0]>=after_first_turn:
            return dict(reason="first_turn",turns=turns,path=rows)
        if z[-1]>=target-5e-10 and tangent[-1]>0:
            t.set_resonance(target)
            result=solve(t,state,None)
            if result is None:
                return dict(reason="target_refinement",turns=turns,path=rows)
            return dict(reason="target",turns=turns,path=rows,target=diagnostics(t,result[0],None))
        prev_dr=tangent[-1]
        if tangent[-1]>0 and target>z[-1]:
            step=min(step,max(3e-6,1.005*(target-z[-1])/tangent[-1]))
        while step>2e-6:
            predicted=z+step*tangent
            corrected=predicted.copy()
            for _ in range(9):
                if not -.01<corrected[-1]<1.05 or not np.all(np.isfinite(corrected)): break
                residual,jac,_=system(corrected)
                aug=np.r_[residual,(corrected-predicted)@tangent]
                if np.max(abs(aug))<1e-10: break
                try: corrected-=np.linalg.solve(np.vstack((jac,tangent)),aug)
                except np.linalg.LinAlgError: break
            if np.linalg.norm(corrected-predicted)<.2*step and np.max(abs(system(corrected)[0]))<1e-9:
                arc+=np.linalg.norm(corrected-z)
                z=corrected; direction=tangent
                step=min(step_max,step*1.3)
                break
            step*=.5
        else:
            return dict(reason="minimum_step",turns=turns,path=rows)
    return dict(reason="budget",turns=turns,path=rows)


def trace_pair(args):
    q,step=args
    t=Tone(q,20*1000**(56/99),90/99)
    return dict(q=q,hz=RATE/q,step=step,**follow(t,step,81/99))


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    which=ap.add_mutually_exclusive_group(required=True)
    which.add_argument('--periods',type=int,nargs='+')
    which.add_argument('--unresolved-sweep',type=Path)
    ap.add_argument('--workers',type=int,default=1)
    args=ap.parse_args()
    qs=args.periods
    if args.unresolved_sweep:
        with gzip.open(args.unresolved_sweep) as f:
            data=list(map(json.loads,f))
        checked={r['q'] for r in data if r['kind']=='mesh_check' and r['r']>.8 and r['state_scaled_inf']<1e-5}
        qs=sorted({r['q'] for r in data}-checked)
    with args.out.open('x',buffering=1) as f, ProcessPoolExecutor(args.workers) as pool:
        for result in pool.map(trace_pair,[(q,step) for q in qs for step in (.04,.02)]):
            print(json.dumps(result,allow_nan=False),file=f)
            print(result['q'],result['step'],result['reason'],len(result['turns']),len(result['path']),flush=True)


if __name__=='__main__':
    main()
