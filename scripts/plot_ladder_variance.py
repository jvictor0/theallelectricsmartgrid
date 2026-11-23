#!/usr/bin/env python3
"""Plot conditional high-variance rates from characterize_ladder_variance.py."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    root=parser.parse_args().directory
    plt.rcParams.update({'font.size':11,'figure.facecolor':'#fafafa','axes.facecolor':'#fafafa'})
    cmap=plt.get_cmap('magma').copy()
    cmap.set_bad('#ddd')
    data=json.loads((root/'frequency-cutoff-map.json').read_text())
    fig,axes=plt.subplots(2,2,figsize=(12,10),layout='constrained')
    low,high=np.log2([20,20000])
    step=(high-low)/99
    extent=(low-step/2,high+step/2,low-step/2,high+step/2)
    ticks=[20,100,1000,10000,20000]
    labels=['20','100','1k','10k','20k']
    for column,metric in enumerate(('total','fundamental')):
        for row,kind in enumerate(('trial','temporal')):
            values=np.full((100,100),np.nan)
            for point in data:
                n=point[metric+'_eligible']
                if n:
                    values[point['c_i'],point['f_i']]=100*point[metric+'_high_'+kind]/n
            ax=axes[row,column]
            im=ax.imshow(values,extent=extent,origin='lower',aspect='auto',cmap=cmap,vmin=0,vmax=100,interpolation='nearest')
            ax.set_xticks(np.log2(ticks),labels)
            ax.set_yticks(np.log2(ticks),labels)
            ax.set(xlabel='Input frequency target (Hz, log scale)',ylabel='Cutoff (Hz, log scale)',
                   title=metric.title()+' power: '+('across starts' if kind=='trial' else 'within runs'))
            fc=np.geomspace(20,20000,300)
            sine=np.sin(np.pi*fc/192000)
            alpha=2*sine*(np.sqrt(1+sine*sine)-sine)
            nominal=192000/np.pi*np.arcsin(alpha/(2*(1-alpha)))
            mask=nominal<=20000
            ax.plot(np.log2(nominal[mask]),np.log2(fc[mask]),color='#59c6cf',ls='--',lw=1)
    fig.colorbar(im,ax=axes,label='Eligible configurations with power CV >5% (%)',shrink=.8)
    fig.suptitle('Higher variance concentrates at low cutoffs\nRates over sampled drive/resonance settings at each frequency pair',fontsize=16)
    fig.supxlabel('A = 0.25 · 100 starts · one second · detector off · k·s³ > 3.5 · output gain ≥ −40 dB\n'
                  'Dashed line: input equals nominal oscillation frequency. Within-run CV is averaged over starts.',fontsize=10)
    fig.savefig(root/'variance-frequency-cutoff.png',dpi=180)
    plt.close(fig)

    data=json.loads((root/'ratio-eta-map.json').read_text())
    minimum=min(r['ratio_bin'] for r in data)
    maximum=max(r['ratio_bin'] for r in data)
    eta_labels=['<0.9','0.9–1','1–1.05','1.05–1.1','1.1–1.25','1.25–1.5','1.5–2','2–4','4–8','8–16','≥16']
    fig,axes=plt.subplots(1,2,figsize=(13,6),layout='constrained')
    for ax,metric in zip(axes,('total','fundamental')):
        values=np.full((len(eta_labels),maximum-minimum+1),np.nan)
        for r in data:
            if r[metric+'_eligible']:
                values[r['eta_bin'],r['ratio_bin']-minimum]=100*r[metric+'_high_trial']/r[metric+'_eligible']
        im=ax.imshow(values,origin='lower',extent=(minimum/2,(maximum+1)/2,-.5,len(eta_labels)-.5),aspect='auto',
                     cmap=cmap,vmin=0,vmax=100,interpolation='nearest')
        ax.set_yticks(range(len(eta_labels)),eta_labels)
        ax.set_xticks([-6,-4,-2,0,2,4,6,8,10],['1/64','1/16','1/4','1','4','16','64','256','1024'])
        ax.axhline(1.5,color='#59c6cf',ls='--',lw=1)
        ax.axvline(0,color='#59c6cf',ls=':',lw=1)
        ax.set(xlabel='Input / nominal oscillation frequency',ylabel='Effective feedback / critical feedback (η)',
               title=metric.title()+' power: variation across starts',xlim=(-6,10))
    fig.colorbar(im,ax=axes,label='Eligible configurations with power CV >5% (%)',shrink=.85)
    fig.suptitle('Total-power variance hugs the instability threshold; fundamental variance extends above it',fontsize=14)
    fig.supxlabel('η = k·s³ / kcritical; dashed horizontal line is η = 1. These are diagnostic linearization coordinates, not measured nonlinear frequencies.\n'
                  'Unequal η bins shown with equal heights; rates pool the remaining parameters. Gray = no eligible observations.',fontsize=10)
    fig.savefig(root/'variance-feedback-frequency.png',dpi=180)
    plt.close(fig)


if __name__=='__main__':
    main()
