"""Check how much upper-band ripple the original frequency grid can show."""
import json

import numpy as np
from numpy.polynomial.legendre import leggauss
from scipy.stats import qmc

from evaluate import (DEPTH, FS, INTRO_TAPS, MIX, OUT, SAT_GAIN, SETTINGS, TANK_TAPS,
                      WIDENERS, allpass, angular_order, damping, knot_weights, lfo_cdf, taps)

WINDOWS = [(350, 450), (650, 750), (1000, 1100), (3000, 3100), (8000, 8100), (12000, 12100)]
FREQUENCY = np.array([np.arange(lo, hi+0.25, .5) for lo,hi in WINDOWS])
CASES = [1, 2]
data = np.load(OUT/'data-curves.npz')
original_freq = data["frequency"]
original_db = 10*np.log10(data["full_power"])

grid, cdf = lfo_cdf(48, 12)
u = (np.arange(16384)+.5)/16384
wander = 2*np.interp(u,cdf,grid)-1
sobol = qmc.Sobol(8, scramble=False).random_base2(13)
rank = np.minimum((sobol*len(wander)).astype(int), len(wander)-1).T
age, weights = taps(DEPTH*(1+wander))
age += 1-DEPTH

# Characteristic function reduced to one quadrature over the knot phase.
t, g = leggauss(96)
shape = knot_weights((t+1)/2)
def coherence(hz):
    return float(abs(np.sum(g*np.prod(np.sinc((2*hz*DEPTH/FS)*shape), axis=0))/2))

def response(j, w, factors, intro_power):
    _, ms, feedback, lo,hi = SETTINGS[j]
    q = np.exp(-1j*w)
    whole, weight = taps(ms*FS/1000*WIDENERS)
    main = np.exp(-1j*w*whole)*np.sum(weight*np.exp(1j*w*np.arange(4)),axis=-1)
    forward = (damping(w,lo,hi)*SAT_GAIN)*factors[..., :, None]*(MIX*main)
    ret = q*((1.25*(9**feedback-1)/8)/SAT_GAIN)*forward
    h = np.linalg.solve(np.eye(4)-ret,forward)
    power = np.sum(np.abs(h)**2,axis=-1)/4
    if power.ndim==1:
        return 10*np.log10(power*intro_power)
    return 10*np.log10(power.mean(axis=0)*intro_power)

curves = np.empty((len(CASES),len(WINDOWS),3,FREQUENCY.shape[1],4))
for k, frequency in enumerate(FREQUENCY):
    for i, f in enumerate(frequency):
        w=2*np.pi*f/FS
        base_z=np.exp(-1j*w*age)*np.sum(weights*np.exp(1j*w*np.arange(4)),axis=1)
        intro=allpass(np.exp(-1j*w*INTRO_TAPS[:,None])*base_z)
        intro_power=np.prod(np.mean(np.abs(intro)**2,axis=1))
        marginals=allpass(np.exp(-1j*w*TANK_TAPS[:,None])*base_z)
        packed=np.stack([a[angular_order(a)[indices]] for a,indices in zip(marginals,rank)])
        factors=(packed[:4]*packed[4:]).T
        fixed=allpass(np.exp(-1j*w*TANK_TAPS)).reshape(2,4).prod(axis=0)
        for case,j in enumerate(CASES):
            curves[case,k,0,i]=response(j,w,factors,intro_power)
            curves[case,k,1,i]=response(j,w,fixed,1.)
            curves[case,k,2,i]=[np.interp(f,original_freq,original_db[j,:,lane]) for lane in range(4)]
    print("Checked", WINDOWS[k], flush=True)

def ripple(y):
    x=np.linspace(-.5,.5,y.shape[-2])
    trend = np.sum(y*x[:,None],axis=-2,keepdims=True)/np.sum(x*x)
    residual=y-trend*x[:,None]
    return float(np.median(np.quantile(residual,.95,axis=-2)-np.quantile(residual,.05,axis=-2)))

stats={"raw_delay_coherence": {f: coherence(f) for f in [250,500,750,1000,1250,1500,2000,3000,5000,10000]}, "ripple":[]}
for case,j in enumerate(CASES):
    for k,(lo,hi) in enumerate(WINDOWS):
        a,b,c=curves[case,k]
        stats["ripple"].append(dict(case=SETTINGS[j][0], low=lo,high=hi,
            original_grid_count=int(np.sum((original_freq>=lo)&(original_freq<=hi))),
            dense_wander_db=ripple(a), dense_fixed_db=ripple(b), original_interp_db=ripple(c)))
print(json.dumps(stats,indent=2))
(OUT/'ripple-check.json').write_text(json.dumps(stats,indent=2)+'\n')
np.savez_compressed(OUT/'ripple-check.npz',freq=FREQUENCY,curves=curves)
