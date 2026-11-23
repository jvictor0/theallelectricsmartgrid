"""Replay selected whole resonance sweeps from reset and perturb their periodic orbits."""
import argparse
import ctypes as ct
import json
from pathlib import Path

import numpy as np

from analyze_ladder_periodic_branches import PeriodMap, ROOT

RATE = 192000
CASES = {
    'high': (90, 90, [16, 17, 18]),
    'near': (57, 90, [80, 81, 82]),
    'double': (66, 99, [16, 17, 18]),
    'regain': (33, 90, [28, 29, 31, 32, 33]),
    'control': (57, 75, [98, 99]),
}
PTR = ct.POINTER(ct.c_float)


def ptr(a):
    return a.ctypes.data_as(PTR)


def measurements(samples, q, hz, amp):
    # Choose whole buffers so the complex projection is exactly bin-centered.
    y = samples[-(len(samples)//q*q):].astype(float)
    n = len(y)
    fundamental = 2*abs(np.sum(y*np.exp(-2j*np.pi*hz/RATE*np.arange(n))))/n
    mismatch = np.sqrt(np.sum((y[q:]-y[:-q])**2)/np.sum(y[:-q]**2))
    hann = np.hanning(n)
    spectrum = abs(np.fft.rfft(y*hann))**2
    maxima = np.flatnonzero((spectrum[1:-1]>spectrum[:-2]) & (spectrum[1:-1]>spectrum[2:]))+1
    peaks = []
    for k in maxima[np.argsort(spectrum[maxima])[-9:][::-1]]:
        left, mid, right = np.log(np.maximum(spectrum[k-1:k+2],1e-50))
        delta = .5*(left-right)/(left+right-2*mid)
        peaks.append([float((k+delta)*RATE/n), float(spectrum[max(0,k-2):k+3].sum()/spectrum.sum())])
    return dict(error=float(mismatch), gain=float(20*np.log10(fundamental/amp)), peaks=peaks,
                total_power=float(y@y/n))


def fit_fold(model, seed_state, seed_level, gain_cap):
    # At a simple fold, one multiplier equals +1. Solve for the orbit,
    # knob position, and its unit eigenvector together in the smooth model.
    def evaluate(state, level):
        coeff = model.p.copy()
        coeff[2] = min(gain_cap, 3.9*np.expm1(np.log(49)*level/99)/48)
        end, jac, _ = model.cycle(state, coeff)
        return end-state, jac-np.eye(4)

    def equations(z):
        residual, jac = evaluate(z[:4],z[4])
        return np.r_[residual, jac@z[5:], z[5:]@z[5:]-1]

    _, jac = evaluate(seed_state,seed_level)
    vector = np.linalg.svd(jac)[2][-1]
    z = np.r_[seed_state, seed_level, vector]
    for _ in range(40):
        residual = equations(z)
        size = np.linalg.norm(residual)
        if size < 1e-10:
            break
        increments = np.eye(9)*1e-6
        j = np.stack([(equations(z+h)-equations(z-h))/2e-6 for h in increments],axis=1)
        delta = np.linalg.solve(j,-residual)
        for factor in 2.**-np.arange(14):
            trial = z + factor*delta
            if np.linalg.norm(equations(trial)) < size:
                z=trial
                break
        else:
            return None
    if np.linalg.norm(equations(z)) >= 1e-10:
        return None
    x,r,v = z[:4],z[4],z[5:]
    residual, jac = evaluate(x,r)
    u,s,_ = np.linalg.svd(jac)
    left = u[:,-1]
    h = 1e-4
    param = (evaluate(x,r+h)[0]-evaluate(x,r-h)[0])/(2*h)
    second = (evaluate(x+h*v,r)[0]-2*residual+evaluate(x-h*v,r)[0])/h**2
    curvature = -(left@second)/(left@param)
    assert s[-2]>1e-5 and abs(left@param)>1e-7 and abs(left@second)>1e-6
    return dict(level=float(r),eigenvalues=np.linalg.eigvals(np.eye(4)+jac).astype(str).tolist(),
                equation_error=float(np.linalg.norm(equations(z))),curvature=float(curvature),
                projected_parameter_derivative=float(left@param),projected_second_derivative=float(left@second))


def run(root, label, task):
    case = CASES[label]
    f,d,levels = case
    data = np.load(root/'boundary.npz')
    meta = json.loads((root.parent/'manifest.json').read_text())
    tone = meta['frequencies'][f]
    lib = ct.CDLL(str(ROOT/'build/tools/ladder-break-replay/libladder_break_replay.so'))
    lib.LadderBreakPrepare.argtypes = [ct.c_double,ct.c_double,PTR,PTR]
    lib.LadderBreakTone.argtypes = [ct.c_double,ct.c_double,PTR,ct.c_int]
    lib.LadderBreakStep.argtypes = [PTR,PTR,PTR,ct.c_int,ct.c_int,ct.c_int,PTR]
    lib.LadderBreakPrepare.restype = lib.LadderBreakStep.restype = None
    p = np.empty(5,np.float32)
    feedback = np.empty(10000,np.float32)
    lib.LadderBreakPrepare(d/99,data['cutoff'][56],ptr(p),ptr(feedback))
    inputs = np.empty(20000,np.float32)
    amp = meta['amplitude']
    q = lib.LadderBreakTone(tone['target_hz'],amp,ptr(inputs),len(inputs))
    assert q == tone['period_samples'] and tone['input_cycles']==1
    inputs = inputs[:q]
    hz = RATE/q
    report = dict(case=label,frequency=hz,cutoff=data['cutoff'][56],drive=d/99,q=q,levels={})
    if task == 'orbit':
        model = PeriodMap(dict(coefficients=p.tolist(),input=inputs.tolist(),quadrature=np.zeros(q)))
        x = np.zeros(4)
        # Continue the periodic solution past loss of attraction using Newton.
        # Eight substeps per saved knob interval resolve the multiplier crossing.
        trace = []
        for idx in np.unique(np.rint(np.linspace(0,levels[-1]*101,levels[-1]*8+1)).astype(int)):
            step = idx/101
            model.p[2] = feedback[idx]
            candidate,err,_ = model.fit(x)
            if err>1e-9:
                report['root_failure_at'] = step
                report['fold'] = fit_fold(model,x,trace[-1][0],float(feedback[-1]))
                break
            x = candidate
            end,j,y = model.cycle(x)
            eig = np.linalg.eigvals(j)
            lam = eig[np.argmax(abs(eig))]
            trace.append([step,float(abs(lam)),float(lam.real),float(abs(lam.imag)),float(err)])
            if idx%101 == 0 and idx//101 in levels:
                # Independent finite differences of the whole q-sample map.
                direction = np.eye(4)*1e-7
                fd = np.stack([(model.cycle(x+v)[0]-model.cycle(x-v)[0])/2e-7 for v in direction],axis=1)
                fd_err = float(np.max(abs(fd-j))/max(1,np.max(abs(j))))
                assert fd_err<1e-5
                report['levels'][int(idx//101)] = dict(radius=float(abs(lam)),root_residual=float(max(abs(end-x))),
                    derivative_error=fd_err,mode_offset_hz=float(np.angle(complex(lam.real,abs(lam.imag)))*hz/(2*np.pi)))
        report['trace'] = trace
    else:
        state = np.zeros(4,np.float32)
        output = np.empty(RATE,np.float32)
        phase = 0
        def advance(s, phase, n, dest=None):
            lib.LadderBreakStep(ptr(p),ptr(s),ptr(inputs),q,phase,n,ptr(dest) if dest is not None else None)
            return (phase+n)%q
        p[2] = feedback[0]
        phase = advance(state,phase,48000)
        spectra = {}
        for idx in range(levels[-1]*101+1):
            saved = idx%101==0 and idx//101 in levels
            p[2] = feedback[idx]
            phase = advance(state,phase,RATE,output if saved else None)
            if saved:
                level = idx//101
                short = measurements(output[-RATE//2:],q,hz,amp)
                copy = state.copy()
                extra_phase = advance(copy,phase,11*RATE)
                advance(copy,extra_phase,RATE,output)
                long = measurements(output,q,hz,amp)
                report['levels'][level] = dict(grid_error=float(data['cycle56'][f,d,level]),
                    grid_gain=float(data['gain56'][f,d,level]),first=short,after_12s=long)
                # Retain the complete final second for spectra and other lag tests.
                spectra[str(level)] = output.copy()
        np.savez_compressed(root/(label+'-audio.npz'),**spectra)
    (root/(label+'-'+task+'.json')).write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('case',choices=CASES)
    parser.add_argument('task',choices=['orbit','replay'])
    args=parser.parse_args()
    run(args.directory,args.case,args.task)
