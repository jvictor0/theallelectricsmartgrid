"""Plot the resonance test and the corresponding first-crossing power observations."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np


def plot(out):
    fig,axes = plt.subplots(3,2,figsize=(10.6,7.9),sharex='col',layout='constrained',height_ratios=(1.,1.,1.))
    blue,orange,red = '#00699A','#A74418','#704B86'
    for col,(q,m,pair,title) in enumerate([(561,1122,('5-2807','8-4491'),'Near one third of oscillation'),
                                           (183,366,('16-2917','13-2370'),'Near oscillation')]):
        with (out/f'phase-{q}-{m}.csv').open() as f:
            rows = list(csv.DictReader(f))
        switch = float(rows[0]['switch_hz'])
        low,high = [json.loads((out/f'probe-{p}.json').read_text()) for p in pair]
        def curve(label,key):
            points = sorted((float(r['frequency_hz']),float(r[key])) for r in rows if r['branch']==label)
            return np.array(points).T
        x,y = curve('first maximum','knob')
        axes[0,col].plot(x,100*y,color=red,lw=2.5)
        axes[0,col].axhline(100*81/99,c='0.3',ls='--',lw=1)
        axes[0,col].plot(switch,100*81/99,'o',c=red,ms=5)
        axes[0,col].set_title(f'{title}\n{switch:,.3f} Hz',loc='left',weight='semibold')
        for j,key in enumerate(('total_db','fund_db'),1):
            ax = axes[j,col]
            x,quiet = curve('quiet',key)
            _,turn = curve('first maximum',key)
            at = np.argmin(abs(x-switch))
            loudside = x>=switch if col else x<=switch
            quietside = x<=switch if col else x>=switch
            ax.plot(x[loudside],quiet[loudside],color=blue,lw=1.5,ls=':')
            ax.plot(x[quietside],quiet[quietside],color=blue,lw=2.6)
            bright = sorted([(float(r['frequency_hz']),float(r[key])) for r in rows if r['branch']=='outgoing']+[(switch,turn[at])])
            ax.plot(*np.array(bright).T,color=orange,lw=2.6)
            ax.plot([switch,switch],[quiet[at],turn[at]],marker='o',mfc='white',mec='0.3',ms=4,ls='')
            for witness in (low,high):
                name = 'outgoing' if 'outgoing' in witness['powers'] else 'quiet'
                field = ('total' if j==1 else 'fundamental')+'_gain_db'
                ax.plot(witness['input_hz'],np.mean(witness['powers'][name][field]),'D',ms=4,
                        color=orange if name=='outgoing' else blue)
            ax.margins(y=.18)
        for ax in axes[:,col]:
            ax.axvspan(low['input_hz'],high['input_hz'],color='#A6A0AF',alpha=.18,zorder=-2)
            ax.axvline(switch,lw=.9,ls=':',color='#666666',zorder=-1)
            ax.grid(axis='y',alpha=.17)
            ax.spines[['top','right']].set_visible(False)
            ax.ticklabel_format(useOffset=False,axis='x')
        axes[-1,col].set_xlabel('Input frequency (Hz)')
    for ax,label in zip(axes[:,0],('First resonance maximum (%)','Total gain (dB)','Fundamental gain (dB)')):
        ax.set_ylabel(label)
    fig.suptitle('Which waveform first reaches 81.82% resonance?',x=.015,ha='left',weight='bold',fontsize=15)
    handles = [Line2D([],[],color=blue,lw=2.4,label='Quiet first crossing'),
               Line2D([],[],color=orange,lw=2.4,label='Loud first crossing'),
               Line2D([],[],color=blue,ls=':',label='Coexisting quiet waveform'),
               Line2D([],[],color='.35',marker='D',ls='',ms=4,label='Exact-tone power enclosure')]
    fig.legend(handles=handles,loc='outside lower center',ncol=4,frameon=False,fontsize=9)
    fig.savefig(out/'branch-switches.png',dpi=175)
    plt.close(fig)


if __name__=='__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out',type=Path)
    plot(ap.parse_args().out)
