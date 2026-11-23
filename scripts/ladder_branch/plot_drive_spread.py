"""Plot the observed playback and the stability/perceptual cost of stage spacing."""
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def table(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def xy(rows,x,y):
    return np.array(sorted((float(r[x]),float(r[y])) for r in rows)).T


def plot(out,previous):
    trials=table(out/'from-reset.csv')
    hot=[r for r in trials if float(r['g'])>3.8]
    stock=[r for r in hot if float(r['spread'])==1]
    blue,amber,green='#17689a','#a44813','#078271'
    fig,(ax,zoom)=plt.subplots(1,2,figsize=(11.5,4.15),layout='constrained',width_ratios=(1.3,1))
    for field,c,name in [('late_total_db',blue,'Total'),('late_fund_db',amber,'Fundamental')]:
        ax.plot(*xy(stock,'hz',field),c=c,lw=1.5,label=name)
    for f,lab in [(1011.75/3,'⅓ nominal'),(1011.75,'Nominal')]:
        ax.axvline(f,c='0.5',lw=.8,ls=':')
        ax.text(f, .03,lab,transform=ax.get_xaxis_transform(),ha='center',va='bottom',fontsize=9,
                bbox=dict(facecolor='white',alpha=.8,edgecolor='none',pad=.5))
    ax.set(xscale='log',xlim=(100,10000),ylim=(-38,32),title='One-second sine tests from reset')
    ax.set_ylabel('Gain relative to input (dB)')
    ax.legend(frameon=False,loc='upper right')
    src=table(previous/'phase-561-1122.csv')
    switch=float(src[0]['switch_hz'])
    for field,c in [('late_total_db',blue),('late_fund_db',amber)]:
        window=[r for r in stock if 341.6<=float(r['hz'])<=342.3]
        zoom.plot(*xy(window,'hz',field),color=c,lw=2.3,label='Measured '+('total' if field=='late_total_db' else 'fundamental'))
    for label,condition in [('outgoing',lambda f:f<switch),('quiet',lambda f:f>switch)]:
        segment=[r for r in src if r['branch']==label and condition(float(r['frequency_hz']))]
        points=list(zip(*xy(segment,'frequency_hz','total_db')))
        turn=next(r for r in src if r['frequency_hz']==r['switch_hz'] and r['branch']==('first maximum' if label=='outgoing' else 'quiet'))
        points.append((switch,float(turn['total_db'])))
        zoom.plot(*np.array(sorted(points)).T,ls='--',c='0.25',lw=1.6,label='First-crossing total' if label=='quiet' else None)
    zoom.axvline(switch,c='0.6',lw=1,ls=':')
    zoom.set(xlim=(341.6,342.3),ylim=(14.4,22.3),title='At the previously identified branch switch')
    zoom.legend(frameon=False,loc='center right',bbox_to_anchor=(1,.31),fontsize=9)
    for a in [ax,zoom]:
        a.set_xlabel('Input frequency (Hz)')
        a.grid(axis='y',alpha=.18)
    fig.savefig(out/'playback.png',dpi=165);plt.close(fig)

    fig,axes=plt.subplots(2,2,figsize=(10.5,7.2),layout='constrained')
    lim=table(out/'spread-limits.csv')
    lin=table(out/'linear.csv')
    ax=axes[0,0]
    ax.plot(*xy(lim,'spread','critical_loop_gain'),color=green,marker='.',label='Gain at onset')
    ax.axhline(103.855,color='.3',ls='--',label='Example loop gain')
    ax.axvline(4.24153,color='.5',ls=':',lw=.9)
    ax.set(xlabel='Stage spacing q',ylabel='Loop gain',title='Onset of instability at silence')
    ax.legend(frameon=False,fontsize=9)
    ax=axes[0,1]
    for v,color in [(1,blue),(2,amber),(4.5,green)]:
        ax.plot(*xy([r for r in lin if float(r['spread'])==v],'hz','open_db'),label=f'q = {v:g}',c=color)
    ax.axhline(-12.0412,color='.6',ls=':',lw=1)
    ax.set(xscale='log',xlim=(20,20000),ylim=(-85,3),title='Four stages with feedback open',ylabel='Gain relative to DC (dB)')
    ax.legend(frameon=False,fontsize=9)
    ax=axes[1,0]
    for v,color in [(4.5,green),(5,'#70519b')]:
        rs=[r for r in lin if float(r['spread'])==v]
        assert all(r['stable_origin']=='True' for r in rs)
        ax.plot(*xy(rs,'hz','closed_db'),label=f'q = {v:g}',color=color,lw=1.8)
    ax.set(xscale='log',xlim=(20,20000),ylim=(-35,47),title='Stable small-signal closed-loop responses',ylabel='Input to output gain (dB)')
    ax.legend(frameon=False,fontsize=9)
    ax=axes[1,1]
    for v,color in [(1,blue),(2,amber),(4.5,green)]:
        rs=[r for r in hot if float(r['spread'])==v]
        ax.plot(*xy(rs,'hz','late_fund_db'),c=color,lw=1.5,label=f'q = {v:g}')
    ax.set(xscale='log',xlim=(100,10000),ylim=(-40,33),title='Measured fundamental at −18 dBFS',ylabel='Fundamental gain (dB)')
    ax.legend(frameon=False,fontsize=9)
    for ax in axes.ravel():
        ax.grid(axis='y',alpha=.18)
        ax.spines[['top','right']].set_visible(False)
    for ax in [axes[0,1],*axes[1]]:ax.set_xlabel('Frequency (Hz)')
    fig.suptitle('Spreading the four stage coefficients at 995.4 Hz cutoff',weight='semibold')
    fig.savefig(out/'stage-spread.png',dpi=165);plt.close(fig)


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    ap.add_argument('previous',type=Path)
    a=ap.parse_args()
    plot(a.out,a.previous)
