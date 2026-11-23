"""Complete the one-third-frequency quiet path using fixed resonance after its second turn."""
import argparse
import json
from pathlib import Path

import numpy as np

from arc_certificate import Arc
from phase_tube import Ball
from resonance_chain import Chain
from segmented_inverse import inverse_bounds
from switch_frequency import ACTIVE,CUTOFF,DRIVE,RATE,SCALE,TARGET,PhaseGrid,Tone
from switch_probe import refine_cycle
from turning_points import follow


def row(file,q,step=None):
    return next(r for r in map(json.loads,file.open()) if r['q']==q and (step is None or r['step']==step))


def feedback(r):
    return 3.9*np.expm1(np.log(49)*r)/48


def setup():
    t = Tone(561,CUTOFF,DRIVE)
    grid = PhaseGrid(t.q,3)
    grid.frequency(RATE/t.q)
    dry = 2.**np.floor(np.log2(t.anchor[8]))
    scale = np.array([1.]*4+[np.sqrt(dry)/4]*4+[dry/8]+[1.]*4+[.5])
    return t,grid,scale


def prepare(previous,out):
    path = out/'first-turn-seeds.jsonl'
    if not path.exists():
        with path.open('x') as file:
            for q in (182,183,561,562):
                print(json.dumps(dict(q=q,**follow(Tone(q,CUTOFF,DRIVE),.016,1.04,after_first_turn=.25))),file=file)
    t,grid,scale = setup()
    first = row(path,t.q)['path']
    later = row(previous/'sweep-arcs.jsonl',t.q,.02)['path']
    # The first arc stops on its descending leg. Pick up the finer saved arc
    # there and finish comfortably past the second turn at positive dK/ds.
    bridge = [first[-1],*[x for x in later if first[-1]['arc']<x['arc']<=5.602]]
    if len(bridge)<2 or not bridge[0]['tangent_r']<0<bridge[-1]['tangent_r']:
        raise ArithmeticError('wrong orientation for bridge')
    (out/'bridge561.jsonl').write_text(json.dumps(dict(q=561,reason='bridge',path=bridge))+'\n')
    last = bridge[-1]
    k = feedback(last['r'])
    arc = Arc(t,np.r_[scale,1.])
    z = arc.states(np.r_[last['state'],k])[:,:14].copy()
    knobs = np.linspace(last['r'],TARGET,9)
    states = [z.copy()]
    for a,b in zip(knobs,knobs[1:]):
        for r in np.linspace(a,b,25)[1:]:
            z,_ = refine_cycle(grid,z[:,ACTIVE]/SCALE,r,np.arange(t.q),None)
        states.append(z.copy())
    np.savez_compressed(out/'quiet561-seeds.npz',states=np.array(states),feedback=feedback(knobs))
    print('bridge nodes',len(bridge),'quiet section',knobs,flush=True)


def section(out,i):
    t,g,scale = setup()
    chain = Chain(t,scale)
    chain.inverse = inverse_bounds
    saved = np.load(out/'quiet561-seeds.npz')
    start,end = saved['feedback'][i:i+2]
    left,right = saved['states'][i:i+2]
    order = np.arange(t.q)
    snapshots = [left]
    name = out/f'quiet561-{i}.jsonl'
    with name.open('x',buffering=1) as f:
        print(json.dumps(dict(kind='setup',q=t.q,scale=scale.tolist(),cutoff=t.cutoff,drive=t.drive,segmented=True)),file=f)
        def visit(k,h,z,w,depth):
            rad = max(1e-8,min(.001,(h-k)**2))
            for _ in range(4):
                ok,lip,eta = chain.check(k,h,z,w,rad)
                if ok:
                    print(json.dumps(dict(kind='cell',lo=k,hi=h,contraction=lip,defect=eta,radius=rad)),file=f)
                    snapshots.append(w)
                    return
                if lip>=.95: break
                rad = max(rad*1.4,1.2*eta/(1-lip))
            if depth>=20: raise ArithmeticError('unresolved fixed resonance segment')
            mid = (k+h)/2
            pred = (z[:,ACTIVE]+w[:,ACTIVE])/(2*SCALE)
            knob = np.log1p(48*mid/3.9)/np.log(49)
            v,_ = refine_cycle(g,pred,knob,order,None)
            visit(k,mid,z,v,depth+1)
            visit(mid,h,v,w,depth+1)
        visit(start,end,left,right,0)
    np.savez_compressed(name.with_suffix('.npz'),states=np.array(snapshots))
    print('fixed resonance section',i,len(snapshots)-1,flush=True)


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('previous',type=Path)
    ap.add_argument('out',type=Path)
    ap.add_argument('--section',type=int)
    a = ap.parse_args()
    if a.section is None:
        prepare(a.previous,a.out)
    else:
        section(a.out,a.section)
