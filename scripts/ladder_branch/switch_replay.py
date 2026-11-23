"""Check that the saved charts select the measured state at each of the four original tones."""
import argparse
import json
from pathlib import Path

import numpy as np

from arc_certificate import Arc
from orbit_power import measure_tube
from phase_tube import AMP,Ball
from resonance_chain import Chain
from segmented_inverse import inverse_bounds
from switch_frequency import CUTOFF,DRIVE,TARGET,Tone


def replay(out,q,recalculate):
    t = Tone(q,CUTOFF,DRIVE)
    t.set_resonance(TARGET)
    k = t.coeffs[1]
    if q in (182,562):
        names = [f'anchored{q}']
    elif q==183:
        names = [f'full183-{i}' for i in range(0,400,100)]
    else:
        names = [f'part561-{i}' for i in range(0,252,42)]+[f'bridgepart561-{i}' for i in range(0,80,20)]
        names += [f'quiet561-{i}' for i in range(8)]
    previous = None
    previous_normal = None
    previous_scale = None
    count = 0
    cut = []
    targets = []
    largest = []
    for name in names:
        with (out/f'{name}.jsonl').open() as f:
            setup = json.loads(next(f))
            cells = list(map(json.loads,f))
        with np.load(out/f'{name}.npz') as data:
            z = data['states']
            planes = data['normals'] if 'normals' in data else None
        scale = np.array(setup['scale'])
        rec = (Arc if planes is not None else Chain)(t,scale)
        if setup.get('segmented'): rec.inverse = inverse_bounds
        if len(z)!=len(cells)+1 or any(c['kind']!='cell' for c in cells):
            raise ArithmeticError('incomplete witness')
        fixed = np.eye(15)[-1]
        def orientation(n):
            return fixed if n is None else n
        if previous is not None:
            if not np.array_equal(previous,z[0,:,:14]) or last!=cells[0]['lo']:
                raise ArithmeticError(f'non-identical junction at {name}')
            if not np.array_equal(previous_scale,scale[:14]):
                raise ArithmeticError('changed junction scales')
            before,after = orientation(previous_normal),orientation(None if planes is None else planes[0])
            if not (np.array_equal(before,after) or np.array_equal(before,-after)):
                raise ArithmeticError('different endpoint hyperplanes')
        else:
            if cells[0]['lo']!=0 or not np.array_equal(orientation(None if planes is None else planes[0]),fixed):
                raise ArithmeticError('missing feedforward anchor')
            last = 0.
        for i,c in enumerate(cells):
            a,b = c['lo'],c['hi']
            if a!=last: raise ArithmeticError('gap in parameter')
            if planes is not None and not (np.all(z[i,:,-1]==a) and np.all(z[i+1,:,-1]==b)):
                raise ArithmeticError('wrong chart')
            rad,lip,eta = (c[key] for key in ('radius','contraction','defect'))
            if recalculate:
                if planes is not None: ok,lip,eta = rec.check_arc(z[i],z[i+1],*planes[i:i+2],rad)
                else: ok,lip,eta = rec.check(a,b,z[i],z[i+1],rad)
                if not ok: raise ArithmeticError('interval replay failed')
            if not (Ball(lip)*rad+eta).hi()<rad:
                raise ArithmeticError('unenclosed chart')
            err = float((Ball(eta)/(1-Ball(lip))).hi())
            if planes is not None:
                kerr = float((Ball(err)*scale[-1]).hi())
                largest.append(((Ball(max(a,b))-kerr).lo(),(Ball(max(a,b))+kerr).hi()))
                if q==183 and not cut and a>b and b<1.5 and max(v[0] for v in largest)>1.7:
                    cut=largest.copy()
            else:
                kerr = 0.
            if (Ball(min(a,b))-kerr).lo() <= k <= (Ball(max(a,b))+kerr).hi():
                if planes is None and b==k:
                    lo,hi = 1.,1.
                else:
                    weight = ((Ball(k,kerr)-Ball(a))/(Ball(b)-a)) if a!=b else Ball(.5,.5)
                    lo,hi = max(0.,float(weight.lo())),min(1.,float(weight.hi()))
                if lo<=hi:
                    delta = Ball(z[i+1,:,:14])-Ball(z[i,:,:14])
                    points = [Ball(z[i,:,:14])+delta*u for u in (lo,hi)]
                    targets.append((count,[p+Ball(0.,(Ball(scale[:14])*err).magnitude()) for p in points]))
            last = b
            count += 1
        previous = z[-1,:,:14].copy()
        previous_scale = scale[:14]
        previous_normal = planes[-1] if planes is not None else None
        if q==561 and name.startswith('part561'): cut = largest.copy()
    if last!=k or not np.array_equal(orientation(previous_normal),fixed):
        raise ArithmeticError('wrong target at end of path')
    if not targets or targets[-1][0]!=count-1:
        raise ArithmeticError('missing final crossing')
    # All possible earlier target crossings must fall in the same local
    # uniqueness box as the terminal solution. This also catches extra crossings
    # in the interiors of charts whose proposed endpoints both miss the target.
    offset = max(((p-Ball(previous))/Ball(scale[:14])).magnitude().max() for _,v in targets for p in v)
    rad = max(float((Ball(offset)*1.05).hi()),2e-8)
    chain = Chain(t,scale[:14])
    chain.inverse = inverse_bounds
    ok,lip,eta = chain.check(k,k,previous,previous,rad)
    if not ok: raise ArithmeticError(f'unmatched target ball: radius {rad} L {lip} eta {eta}')
    width = (Ball(scale[:14])*(Ball(eta)/(1-Ball(lip)))).magnitude()
    power = measure_tube(chain,Ball(previous,np.broadcast_to(width,previous.shape)),k,k,harmonics=(3,))
    db = {v: (10*np.log10(np.array(power[v+'_power'])/(float(AMP.m)**2/2))).tolist() for v in ('total','fundamental')}
    report = dict(q=q,cells=count,possible_target_charts=[c for c,_ in targets],matching_radius=rad,
                  contraction=lip,defect=eta,gain_db=db,powers=power,sections=names)
    if cut:
        report['first_excursion_max_feedback'] = [max(v[i] for v in cut) for i in (0,1)]
    return report


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    ap.add_argument('--q',type=int,required=True,choices=(182,183,561,562))
    ap.add_argument('--recalculate',action='store_true')
    a = ap.parse_args()
    report = replay(a.out,a.q,a.recalculate)
    (a.out/f'anchored-powers-{a.q}.json').write_text(json.dumps(report,indent=2)+'\n')
    print(a.q,report['cells'],report['gain_db'],report.get('first_excursion_max_feedback'))
