"""Trace the repeating ladder waveform through folds and test cycle-wise feedback."""
import argparse
import csv
import ctypes as ct
import json
from pathlib import Path

import numpy as np

from replay_ladder_breaks import PeriodMap, PTR, ROOT, ptr

AMP = 10**(-18/20)


def period_model(meta, fi, di, ci):
    lib = ct.CDLL(str(ROOT/'build/tools/ladder-break-replay/libladder_break_replay.so'))
    lib.LadderBreakPrepare.argtypes = [ct.c_double, ct.c_double, PTR, PTR]
    lib.LadderBreakPrepare.restype = None
    lib.LadderBreakTone.argtypes = [ct.c_double, ct.c_double, PTR, ct.c_int]
    tone = meta['frequencies'][fi]
    assert tone['input_cycles'] == 1
    base = np.zeros(5,np.float32)
    ks = np.empty(10000,np.float32)
    lib.LadderBreakPrepare(di/99,20*1000**(ci/99),ptr(base),ptr(ks))
    samples = np.empty(20000,np.float32)
    q = lib.LadderBreakTone(tone['target_hz'],AMP,ptr(samples),len(samples))
    assert q == tone['period_samples']
    return PeriodMap(dict(coefficients=base, input=samples[:q], quadrature=np.zeros(q)))


def coefficients(model, r):
    p = model.p.copy()
    p[2] = min(p[2],3.9*np.expm1(np.log(49)*r)/48)
    return p


def trace(model, max_step):
    def system(z):
        x,r = z[:4],z[4]
        end, jac, y = model.cycle(x,coefficients(model,r))
        h = 2e-6
        plus = model.cycle(x,coefficients(model,r+h))[0]
        minus = model.cycle(x,coefficients(model,r-h))[0]
        return end-x,np.column_stack([jac-np.eye(4),(plus-minus)/(2*h)]),jac,y

    x,err,_ = model.fit(np.zeros(4),coefficients(model,0))
    assert err < 1e-10
    z = np.r_[x,0.]
    direction = np.array([0,0,0,0,1.])
    h = max_step
    records = []
    for _ in range(25000):
        residual,derivative,jac,y = system(z)
        assert max(abs(residual)) < 1e-9
        gain = 20*np.log10(abs(2*np.fft.rfft(y)[1]/len(y))/AMP)
        records.append([*map(float,z),float(max(abs(np.linalg.eigvals(jac)))),float(gain),float(max(abs(residual)))])
        if z[4]>=1:
            return np.array(records)
        tangent = np.linalg.svd(derivative)[2][-1]
        if tangent@direction<0:
            tangent = -tangent
        while h>1e-6:
            guess = z+h*tangent
            corrected = guess.copy()
            for iteration in range(10):
                residual,derivative,_,_ = system(corrected)
                residual = np.r_[residual,(corrected-guess)@tangent]
                if max(abs(residual))<2e-12:
                    break
                corrected -= np.linalg.solve(np.vstack([derivative,tangent]),residual)
            if max(abs(residual))<2e-12 and np.linalg.norm(corrected-guess)<.2*h:
                z = corrected
                direction = tangent
                h = min(max_step,h*1.3)
                break
            h *= .5
        else:
            raise RuntimeError('Correction failed')
    raise RuntimeError('Curve did not reach maximum resonance')


def control(model):
    p = coefficients(model,17/99)
    root,err,_ = model.fit(np.zeros(4),p)
    end,jac,y = model.cycle(root,p)
    assert err<1e-10 and max(abs(end-root))<1e-10
    eig = np.linalg.eigvals(jac)
    def advance(x,weight,cycles):
        for _ in range(cycles):
            end = model.cycle(x,p)[0]
            x = (1-weight)*end+weight*x
        return x
    state = advance(root+np.array([1e-7,0,0,0]),0,11000)
    results = []
    for weight in (0,.02,.04,.06,.10):
        z = advance(state,weight,11000)
        out = model.cycle(z,p)[2]
        results.append(dict(weight=weight, controlled_radius=float(max(abs(weight+(1-weight)*eig))),
            initial_distance=float(max(abs(state-root))), final_distance=float(max(abs(z-root))),
            relative_waveform_error=float(np.linalg.norm(out-y)/np.linalg.norm(y))))
    return results


def release_tests():
    source = ROOT/'build/results/ladder-long-runs-v1/basins-v1'
    tests = []
    for point in (82905776,82985375,75993866,89931773):
        model = PeriodMap(json.loads((source/f'{point}-model.json').read_text()))
        with (source/f'{point}-direct.csv').open() as stream:
            row = next(r for r in csv.DictReader(stream) if r['family']=='saved_low')
        start = np.array([float(row['initial_'+k]) for k in 'abcd'])
        roots = json.loads((source/f'{point}-periodic.json').read_text())['roots']
        assert len(roots)==1
        target = np.array(roots[0]['state'])
        for weight in (0,.04,.12,.25,.5):
            x = start.copy()
            for _ in range(250):
                end = model.cycle(x)[0]
                x = (1-weight)*end+weight*x
            after_control = float(max(abs(x-target)))
            for _ in range(1000):
                x = model.cycle(x)[0]
            tests.append(dict(point=point,weight=weight,period=len(model.input),
                after_control=after_control,after_release=float(max(abs(x-target)))))
        for reduction in (.02,.05,.10,.20,.35,.50,.75,1.):
            x = start.copy()
            p = model.p.copy()
            # 64 input buffers down and up, with a 100-buffer hold at the bottom.
            envelope = np.r_[np.linspace(0,1,65)[1:],np.ones(100),np.linspace(1,0,65)[1:]]
            for amount in envelope:
                p[2] = model.p[2]*(1-reduction*amount)
                x = model.cycle(x,p)[0]
            for _ in range(1000):
                x = model.cycle(x)[0]
            tests.append(dict(point=point,reduction=reduction,period=len(model.input),
                after_release=float(max(abs(x-target)))))
    return tests


def main(directory):
    meta=json.loads((directory.parent.parent/'manifest.json').read_text())
    model=period_model(meta,57,90,56)
    results={}
    for step in (.01,.005):
        points=trace(model,step)
        np.save(directory/f'arc-{step}.npy',points)
        r=points[:,4]
        turns=np.nonzero(np.sign(np.diff(r))[1:]!=np.sign(np.diff(r))[:-1])[0]+1
        roots=[]
        for probe in (.70,.80,.82,.95,1.):
            for i in np.nonzero((r[:-1]-probe)*(r[1:]-probe)<=0)[0]:
                coeff=coefficients(model,probe)
                fraction = (probe-r[i])/(r[i+1]-r[i])
                seed = (1-fraction)*points[i,:4]+fraction*points[i+1,:4]
                x,err,_=model.fit(seed,coeff)
                end,jac,y=model.cycle(x,coeff)
                assert err<1e-9, (probe,i,err)
                roots.append(dict(res=probe,phase_deg=float(np.angle(np.fft.rfft(y)[1],deg=True)),radius=float(max(abs(np.linalg.eigvals(jac)))),
                    gain_db=float(20*np.log10(abs(2*np.fft.rfft(y)[1]/len(y))/AMP)),
                    closure=float(max(abs(end-x)))))
        results[str(step)]=dict(turns=r[turns].tolist(),steps=len(r),max_closure=float(points[:,7].max()),roots=roots)
    results['cycle_feedback']=control(period_model(meta,90,90,56))
    results['stable_basin_release']=release_tests()
    (directory/'solves.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    main(parser.parse_args().directory)
