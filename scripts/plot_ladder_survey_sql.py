#!/usr/bin/env python3
"""Plot the small exported SQL analysis artifacts; no raw survey is required."""

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
    args=parser.parse_args()
    root=args.directory
    plt.rcParams.update({'font.size':11,'axes.spines.top':False,'axes.spines.right':False,
                         'figure.facecolor':'#fafafa','axes.facecolor':'#fafafa'})
    data=json.loads((root/'consistency-map.json').read_text())
    fig,axes=plt.subplots(2,2,figsize=(12,10),layout='constrained')
    cmap=plt.get_cmap('magma').copy()
    cmap.set_bad('#dddddd')
    for row,kind in enumerate(('trial','temporal')):
        for column,metric in enumerate(('total','fundamental')):
            values=np.full((100,100),np.nan)
            for point in data:
                value=point[f'{metric}_fraction_{kind}_cv_gt_5pct']
                if value is not None:
                    values[point['r_i'],point['d_i']]=100*value
            ax=axes[row,column]
            im=ax.imshow(values,origin='lower',extent=(-.505,100.505,-.505,100.505),
                         vmin=0,vmax=100,cmap=cmap,aspect='auto',interpolation='nearest')
            ax.set(xlabel='Drive knob (%)',ylabel='Resonance knob (%)',
                   title=f'{metric.title()} power: '+('spread across starts' if kind=='trial' else 'variation within a run'))
    fig.colorbar(im,ax=axes,label='Configurations with power SD / mean > 5% (%)',shrink=.8)
    fig.suptitle('How consistently does the ladder produce the same power?\n'
                 '100 starts per point · A = 0.25 · one second · detector off',fontsize=16)
    fig.supxlabel('Each cell summarizes input frequency × cutoff (20 Hz–20 kHz). Gray = unsampled.\n'
                  'Only k·s³ > 3.5; powers below −40 dB gain excluded. Temporal statistic averages late-window CV over starts.',fontsize=10)
    fig.savefig(root/'consistency.png',dpi=180)
    plt.close(fig)

    report=json.loads((root/'summary.json').read_text())
    fig,axes=plt.subplots(1,2,figsize=(12,4.8),layout='constrained')
    positions=np.arange(4)
    for ax,metric in zip(axes,('total','fundamental')):
        values=[r for r in report['continuity'] if r['kind']=='interpolation']
        for percentile,label,color,offset in [(0,'Median','#26828e',-.18),(2,'95th percentile','#d7892b',0),(3,'99th percentile','#b14559',.18)]:
            ax.bar(positions+offset,[r[f'{metric}_db_quantiles'][percentile] for r in values],width=.18,color=color,label=label)
        ax.set_xticks(positions,[r['axis'].title() for r in values])
        ax.set(yscale='log',ylabel='Absolute midpoint interpolation error (dB)',title=metric.title()+' power')
        ax.grid(axis='y',alpha=.2)
        ax.axhline(.1,color='gray',ls=':',lw=1)
    axes[0].legend(fontsize=9)
    fig.suptitle('How well do the two neighboring points predict the center?\nLinear interpolation of dB in log-frequency / knob position',fontsize=15)
    fig.supxlabel('All three powers ≥ −40 dB gain. This measures resolved curvature and noise; it cannot bound behavior between samples.',fontsize=10)
    fig.savefig(root/'interpolation.png',dpi=180)
    plt.close(fig)

    sweeps=json.loads((root/'sweeps.json').read_text())
    upper=max(24,5*np.ceil(max(p[f'{metric}_max_db'] for sweep in sweeps for p in sweep['points']
                              for metric in ('total','fundamental'))/5))
    fig,axes=plt.subplots((len(sweeps)+1)//2,2,figsize=(13,3.6*((len(sweeps)+1)//2)),layout='constrained',squeeze=False)
    for ax,sweep in zip(axes.flat,sweeps):
        points=sweep['points']
        freq=[p['frequency_hz'] for p in points]
        for metric,color,label in [('total','#127f86','Total power'),('fundamental','#c4562a','Fundamental')]:
            ax.plot(freq,[p[f'{metric}_db'] for p in points],color=color,lw=1.4,label=label)
            ax.fill_between(freq,[p[f'{metric}_min_db'] for p in points],[p[f'{metric}_max_db'] for p in points],color=color,alpha=.18)
        p=points[0]
        ax.axvline(p['cutoff_hz'],color='gray',ls=':',lw=.8)
        ax.set(xscale='log',xlabel='Input frequency (Hz)',ylabel='Power gain (dB)',ylim=(-65,upper),
               title=f"Drive {100*p['drive_knob']:.0f}% · resonance {100*p['resonance_knob']:.0f}% · cutoff {p['cutoff_hz']:.0f} Hz")
        ax.grid(alpha=.2)
    for ax in list(axes.flat)[len(sweeps):]:
        ax.set_visible(False)
    axes[0,0].legend(fontsize=9)
    fig.suptitle('Measured response slices: mean power and full range across 100 starts\n'
                 'Selection includes examples with the largest resolved frequency interpolation errors',fontsize=15)
    fig.savefig(root/'response-slices.png',dpi=180)
    plt.close(fig)


if __name__ == '__main__':
    main()
