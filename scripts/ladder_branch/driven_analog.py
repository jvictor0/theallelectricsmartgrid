"""Periodic boundary-value experiment for a continuous-time reupping core and detector."""
import argparse
import json
from pathlib import Path

import numpy as np
from scipy.integrate import solve_bvp, solve_ivp

from pilot import AMPLITUDE,RATE,Tone
from switch_frequency import CUTOFF,DRIVE,TARGET,resample


def rational(x):
    x=np.clip(x,-3.,3.)
    return x*(27+x*x)/(27+9*x*x)


def run(seed_path):
    t=Tone(562,CUTOFF,DRIVE)
    t.set_resonance(TARGET)
    a,k,g,norm=t.coeffs[:4]
    nominal=RATE*np.arcsin(a/(2*(1-a)))/np.pi
    # Preserve the test tone's detuning from one third of the nominal frequency.
    f=(RATE/562)*CUTOFF/nominal
    w=2*np.pi*CUTOFF/f
    decay=-RATE*np.log1p(-t.coeffs[7])/f
    knots=np.linspace(0.,1.,225)
    fine=np.arange(8192)/8192
    def outputs(z):
        eff=k/(1+g**3/norm**3*1e-4*np.maximum(0,z[13]-z[8]/(4-k)**2)/(z[8]+1e-8))
        return eff,(1+eff)*rational(g*z[3])/norm
    def ode(theta,z):
        eff,_=outputs(z)
        u=AMPLITUDE*np.cos(2*np.pi*theta)
        dz=np.empty_like(z)
        dz[0]=w*(u-eff*z[3]-z[0])
        dz[1:4]=w*(rational(g*z[:3])/norm-z[1:4])
        for j,v in ((4,u),(9,z[3])):
            b,l,c,m,p=z[j:j+5]
            dz[j:j+5]=np.array([w*(v-b-l),w*b,w*(b-c-m),w*c,decay*(c*c-p)])
        return dz
    rows=[]
    with np.load(seed_path) as waves:
        for branch in ('quiet','outgoing','returning'):
            arr=waves['341.637010676-'+branch]
            guess=resample(arr,knots%1).T
            sol=solve_bvp(ode,lambda a,b:a-b,knots,guess,tol=1e-7,max_nodes=6000)
            row=dict(seed=branch,frequency=f,success=sol.success,solver_message=sol.message)
            if sol.success:
                z=sol.sol(fine)
                _,y=outputs(z)
                ph=2*np.mean(y*np.exp(-2j*np.pi*fine))
                # Check the interpolated solution by independent initial-value
                # integration over short segments, including the wrap at phase 1.
                gaps=[]
                for l,r in zip(np.arange(16)/16,np.arange(1,17)/16):
                    iv=solve_ivp(ode,(l,r),sol.sol(l),method='DOP853',rtol=3e-11,atol=3e-13)
                    if not iv.success: raise ArithmeticError(iv.message)
                    gaps.append(max(abs(iv.y[:,-1]-sol.sol(r%1))))
                row.update(fund_db=20*np.log10(abs(ph)/AMPLITUDE),total_db=10*np.log10(2*np.mean(y*y)/AMPLITUDE**2),
                           phase_deg=np.angle(ph,deg=True),third_fraction=2*abs(np.mean(y*np.exp(-6j*np.pi*fine)))**2/np.mean(y*y),
                           max_segment_gap=max(gaps),period_gap=max(abs(sol.sol(0)-sol.sol(1))))
            rows.append(row)
    return rows


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('seeds',type=Path)
    ap.add_argument('out',type=Path)
    a=ap.parse_args()
    rows=run(a.seeds)
    a.out.write_text(json.dumps(rows,indent=2)+'\n')
    for row in rows: print(row)
