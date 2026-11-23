#!/usr/bin/env python3
"""Plot all persistent cases, with complete engine-band spectra."""
import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path)
    root=parser.parse_args().root
    report=json.loads((root/'spectral-report.json').read_text());rows=report['captures']
    data=np.load(root/'spectral-plots.npz')
    order=[82905776,82985375,75993866,89931773,75983886,76973888,61887140]
    letters={p:chr(65+i) for i,p in enumerate(order)}
    notes={82905776:'5.080 kHz oscillation + mixing  /  follows input',
           82985375:'4.787 kHz oscillation + mixing  /  follows input',
           75993866:'2.307 kHz oscillation + mixing  /  follows input',
           89931773:'Input ÷ 3 dominates  /  follows input',
           75983886:'Spectral families on input ÷ 7  /  input ÷ 3',
           76973888:'Half-integer series + extra structure  /  follows input',
           61887140:'Both dominated by input ÷ 4; tiny input line differs'}
    plt.rcParams.update({'font.size':10,'figure.facecolor':'#fafafa','axes.facecolor':'#fafafa',
                         'axes.spines.top':False,'axes.spines.right':False})
    colors={'low':'#007f88','high':'#d06328'}
    fig,axes=plt.subplots(4,2,figsize=(14,12.6),layout='constrained')
    for ax,point in zip(axes.flat,order):
        chosen=[(i,r) for i,r in enumerate(rows) if r['point_id']==point and r['level'] in colors]
        for label in colors:
            group=[(i,r) for i,r in chosen if r['level']==label]
            for j,(i,r) in enumerate(group):
                ax.plot(data[f'hz_{i}'],data[f'db_{i}'],color=colors[label],lw=.7,alpha=.85 if j==0 else .3)
        r=chosen[0][1]
        ax.axvline(r['frequency_hz'],color='#333333',ls='--',lw=1,alpha=.7)
        ax.axvline(24000,color='#808080',ls=':',lw=1)
        ax.axvspan(24000,96000,color='#808080',alpha=.045)
        ax.set(xscale='log',xlim=(20,96000),ylim=(-65,27),ylabel='Line power / input power (dB)',xlabel='Output frequency (Hz)')
        ax.set_xticks([20,100,1000,10000,96000],['20','100','1k','10k','96k'])
        ax.set_yticks([-60,-40,-20,0,20]);ax.grid(alpha=.15)
        ax.set_title(f"{letters[point]}   Input {r['frequency_hz']:,.1f} Hz · cutoff {r['cutoff_hz']:,.1f} Hz\n"
                     f"Drive {100*r['drive_knob']:.1f}% · resonance {100*r['resonance_knob']:.1f}% · starts {r['cluster_counts'][0]}/{r['cluster_counts'][1]}",fontsize=11,loc='left')
        ax.text(.025,.05,notes[point],transform=ax.transAxes,fontsize=9,
                bbox={'facecolor':'#fafafa','alpha':.9,'edgecolor':'none','pad':3})
    ax=axes.flat[-1];ax.axis('off')
    handles=[Line2D([0],[0],color=colors['low'],lw=2,label='Lower fundamental-power response'),
             Line2D([0],[0],color=colors['high'],lw=2,label='Higher fundamental-power response'),
             Line2D([0],[0],color='#333333',ls='--',label='Input sinusoid frequency'),
             Line2D([0],[0],color='#808080',ls=':',label='24 kHz host Nyquist')]
    ax.legend(handles=handles,loc='upper left',frameon=False,fontsize=11)
    ax.text(.02,.52,'A = 0.25 · 192 kHz engine · detector disabled\nFloat/FMA rational saturation\n\n16+ seconds captured after 600 seconds of evolution.\nTwo examples per level where available; reset also checked.\nCounts refer to the original 100 randomized histories.\n\nHann line spectrum; strongest bin retained when reducing\nthe display. Shaded region is above host Nyquist,\nbefore any app downsampling. DC is omitted.',transform=ax.transAxes,va='top',fontsize=10,linespacing=1.5)
    fig.suptitle('Persistent responses have different spectra, not just different gains',fontsize=18)
    fig.savefig(root/'persistent-spectra.png',dpi=170);plt.close(fig)
    # One audible-band close-up makes the new oscillation and mixing family visible.
    fig,axes=plt.subplots(1,2,figsize=(12,4.6),layout='constrained')
    for ax,point in zip(axes,[82905776,89931773]):
        for level in colors:
            i,r=next((i,r) for i,r in enumerate(rows) if r['point_id']==point and r['level']==level)
            ax.plot(data[f'hz_{i}'],data[f'db_{i}'],color=colors[level],lw=.9,label=level.capitalize()+' fundamental')
        ax.set(xlim=(0,21000),ylim=(-40,25),xlabel='Output frequency (Hz)',ylabel='Line power / input power (dB)')
        ax.set_xticks([0,5000,10000,15000,20000],['0','5k','10k','15k','20k']);ax.grid(alpha=.2)
        ax.axvline(r['frequency_hz'],color='gray',ls='--',lw=1)
        ax.set_title(notes[point].replace('  /  ','\nversus '),fontsize=12)
    axes[0].legend(fontsize=9,loc='lower right')
    fig.suptitle('Two ways the filter can choose a different note',fontsize=17)
    fig.savefig(root/'spectral-examples.png',dpi=180);plt.close(fig)


if __name__=='__main__':
    main()
