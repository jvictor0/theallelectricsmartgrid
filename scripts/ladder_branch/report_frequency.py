"""Summarize anchored tone solves and plot the three resonance crossings."""
import argparse
import csv
import gzip
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np

from pilot import ACTIVE, AMPLITUDE, LIB, RATE, Tone

def gain(p):
    return 10*np.log10(p/(AMPLITUDE**2/2))


def sample_states(tone,row):
    states=[]
    out=np.empty(tone.q)
    for a,b,z in zip(row['edges'],row['edges'][1:],row['block_states']):
        state=np.array(z)
        for n in range(a,b):
            states.append(state.copy())
            LIB.ReturnMap(*tone.params,tone.input[n:n+1],1,1,state,out[n:n+1])
    return np.array(states),out


def run(directory):
    with gzip.open(directory/'frequency.jsonl.gz') as f: logs=list(map(json.loads,f))
    arcs=list(map(json.loads,(directory/'sweep-arcs.jsonl').open()))
    if (directory/'edge-segments.jsonl').exists():
        with gzip.open(directory/'edges.jsonl.gz') as f: logs.extend(map(json.loads,f))
        with (directory/'edge-arcs.jsonl').open() as f: arcs.extend(map(json.loads,f))
    angles=list(map(json.loads,(directory/'nominal-arcs.jsonl').open()))
    segmented={(r['q'],r['step']):r for r in map(json.loads,(directory/'segments.jsonl').open())}
    direct={(r['q'],r['spacing']):r for r in logs if r['kind']=='target' and r['r']>.8}
    gap={r['q']:r['state_scaled_inf'] for r in logs if r['kind']=='mesh_check' and r['r']>.8}
    other={(r['q'],r['step']):r for r in arcs}
    if (directory/'edge-segments.jsonl').exists():
        with (directory/'edge-segments.jsonl').open() as f:
            segmented.update({(r['q'],r['step']):r for r in map(json.loads,f)})
    rows=[]
    for q in sorted({r['q'] for r in logs}):
        row=dict(period=q,input_hz=RATE/q,method='unresolved')
        obs=None
        if q in gap and gap[q]<=1e-5 and all(direct[q,s]['metal_jac_relative_inf']<=.01 and direct[q,s]['full_return_residual']<=1e-8 for s in (.01,.02)):
            obs=direct[q,.01]
            row.update(method='resonance',mesh_gap=gap[q])
        elif all(other.get((q,s),{}).get('reason')=='target' for s in (.02,.04)):
            a,b=[other[q,s]['target'] for s in (.02,.04)]
            z,w=np.asarray(a['state']),np.asarray(b['state'])
            scale=np.maximum(.025,abs(z[ACTIVE]))
            scale[-1]=max(1e-8,abs(z[8]),abs(z[-1]))
            dist=max(abs((z-w)[ACTIVE])/scale)
            if dist<=1e-5 and max(a['full_return_residual'],b['full_return_residual'])<=1e-8:
                obs=a
                row.update(method='arclength',mesh_gap=dist)
        elif all(segmented.get((q,h),{}).get('reason')=='target' for h in (.02,.04)):
            a,b=[segmented[q,h] for h in (.02,.04)]
            tone=Tone(q,20*1000**(56/99),90/99)
            tone.set_resonance(81/99)
            z,ya=sample_states(tone,a); w,yb=sample_states(tone,b)
            scale=np.maximum(.025,np.max(abs(z),axis=0))
            scale[[8,13]]=np.maximum(1e-8,np.max(abs(z[:,[8,13]]),axis=0))
            state_gap=np.max(abs(z-w)/scale)
            output_gap=np.linalg.norm(ya-yb)/np.linalg.norm(ya)
            error=max(a['block_residual'],b['block_residual'])
            if state_gap<=1e-5 and output_gap<=1e-6 and error<=1e-8:
                seed=z[0].copy()
                full=float(max(abs(tone.period(seed)-seed)))
                obs=dict(**a,rho_wet=a['wet_multiplier'],full_return_residual=full)
                row.update(method='multiple_shooting',mesh_gap=state_gap,output_relative_rms_gap=output_gap,
                           worst_block_residual=error)
        if obs:
            row.update(fundamental_gain_db=gain(obs['fundamental_power']),total_gain_db=gain(obs['total_power']),
                       fundamental_phase_deg=np.degrees(obs['phase_rad']),wet_multiplier=obs['rho_wet'],return_residual=obs['full_return_residual'])
        rows.append(row)
    with (directory/'response.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(dict.fromkeys(k for r in rows for k in r)))
        w.writeheader(); w.writerows(rows)
    t=Tone(190,20*1000**(56/99),90/99)
    a=t.coeffs[0]; nominal=RATE*np.arcsin(a/(2*(1-a)))/np.pi
    fig,axes=plt.subplots(1,2,figsize=(10.8,4.4),layout='constrained')
    colors=('#146789','#bc601a')
    for ax,center in zip(axes,[nominal/3,nominal]):
        band=[r for r in rows if (r['period']>300)==(center<500)]
        ax.axvline(center,color='.35',ls=':',lw=1.3)
        ax.set_title(f'{"One third of nominal" if center<500 else "Nominal oscillation"} · {center:,.1f} Hz')
        for r in band:
            if r['method']=='unresolved':
                ax.plot(r['input_hz'],.035,'x',color='.56',ms=4,transform=ax.get_xaxis_transform())
            else:
                marker={'resonance':'o','arclength':'s','multiple_shooting':'^'}[r['method']]
                for c,key in zip(colors,['total_gain_db','fundamental_gain_db']):
                    ax.plot(r['input_hz'],r[key],marker=marker,ms=7 if key=='total_gain_db' else 4,color=c,ls='none')
        ax.set_xlabel('Input frequency (Hz)');ax.grid(axis='y',alpha=.22)
        ax.set_ylim(12,30)
    axes[0].set_ylabel('Gain relative to input power (dB)')
    axes[0].legend(handles=[Line2D([],[],marker='o',ls='',color=c,label=lab) for c,lab in zip(colors,['Total','Fundamental'])],loc='upper left')
    axes[1].legend(handles=[Line2D([],[],marker=m,color='.35',ls='',label=txt) for m,txt in [('o','Resonance steps'),('s','Arclength'),('^','Segmented arclength')]],loc='upper left')
    fig.savefig(directory/'frequency-sweep.png',dpi=165);plt.close(fig)
    arc=next(r for r in angles if r['q']==190 and r['step']==.02)
    knob=[]; db=[]; ph=[]
    for pt in arc['path']:
        t.set_resonance(pt['r']);t.period(np.array(pt['state']))
        v=2*np.mean(t.output*t.phase);knob.append(100*pt['r']);db.append(gain(abs(v)**2/2));ph.append(np.angle(v))
    ph=np.degrees(np.unwrap(ph));x=np.asarray(knob);y=np.asarray(db)
    fig,axes=plt.subplots(2,1,figsize=(8,6.4),sharex=True,layout='constrained')
    cuts=[0,*list(np.flatnonzero(np.diff(np.sign(np.diff(x))))+1),len(x)-1]
    for ax,vals,label in zip(axes,[y,ph],['Fundamental gain (dB)','Fundamental phase (degrees)']):
        for i,(l,h) in enumerate(zip(cuts,cuts[1:])):
            ax.plot(x[l:h+1],vals[l:h+1],lw=2.1,color=['#087f72','#b07500','#63429c'][i],
                    label=['From zero','Returning','After second turn'][i])
            m=(l+h)//2
            ax.annotate('',xy=(x[m+2],vals[m+2]),xytext=(x[m-2],vals[m-2]),arrowprops=dict(arrowstyle='->',color=['#087f72','#b07500','#63429c'][i],lw=2))
        ax.axvline(60,color='.5',ls=':',lw=1.1);ax.set_ylabel(label);ax.grid(axis='y',alpha=.23)
    axes[0].legend(frameon=False,fontsize=9);axes[1].set_xlabel('Resonance knob (%)')
    axes[0].set_title(f'Following the periodic solution at {RATE/190:,.2f} Hz')
    fig.savefig(directory/'resonance-turns.png',dpi=165);plt.close(fig)
    print({key:sum(r['method']==key for r in rows) for key in ('resonance','arclength','multiple_shooting','unresolved')})


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('directory',type=Path)
    run(ap.parse_args().directory)
