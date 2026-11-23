"""Plot the measured first-loss boundary and selected waveform replays."""
import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter
import numpy as np

TICKS = [20,50,100,200,500,1000,2000,5000,10000,20000]
HZ = FuncFormatter(lambda x,_: f'{x/1000:g}k' if x>=1000 else f'{x:g}')


def frequency_axis(ax, vertical=False):
    axis = ax.yaxis if vertical else ax.xaxis
    if vertical:
        ax.set_yscale('log')
        ax.set_ylim(20,20000)
    else:
        ax.set_xscale('log')
        ax.set_xlim(20,20000)
    axis.set_ticks(TICKS)
    axis.set_major_formatter(HZ)


def boundary(root):
    a=np.load(root/'boundary.npz')
    f,c,nominal=a['freq'],a['cutoff'],a['osc']
    fig,axes=plt.subplots(2,2,figsize=(14,9.2),layout='constrained')
    ax=axes[0,0]
    colors=['#425693','#007763','#b95013','#782657']
    for color,d in zip(colors,[50,75,90,99]):
        loss=a['loss'][:,d,56]
        valid=(loss<100)&(loss!=a['first'][:,d,56])
        y=loss.astype(float)*100/99
        ax.plot(f,np.where(valid,y,np.nan),color=color,lw=1.7,marker='.',ms=3,
                label=f'{100*d/99:.0f}% drive')
        ax.scatter(f[loss==100],np.full(np.count_nonzero(loss==100),102),marker='^',s=17,color=color)
    ax.axvline(nominal[56],color='0.45',lw=1,ls=':')
    ax.set(ylim=(0,106),ylabel='Resonance knob (%)',title='First failed grid point at 995 Hz cutoff')
    ax.legend(loc='lower left',ncol=2,fontsize=8)
    ax.text(.015,.98,'▲ Passed through maximum resonance',va='top',transform=ax.transAxes,fontsize=8)
    ax=axes[0,1]
    loss=a['loss'][:,90,:].T
    first=a['first'][:,90,:].T
    cmap=plt.get_cmap('viridis').copy()
    cmap.set_under('#e39c78')
    cmap.set_over('#e9edf1')
    mapped=np.where(loss==first,-10,np.where(loss==100,110,loss.astype(float)*100/99))
    m=ax.pcolormesh(f,c,mapped,shading='nearest',cmap=cmap,vmin=0,vmax=100,rasterized=True)
    ax.plot(nominal,c,lw=1,ls='--',color='white',alpha=.9)
    ax.axhline(c[56],lw=.7,color='black',alpha=.6)
    ax.set(ylabel='Cutoff frequency (Hz)',title='First loss across cutoffs at 91% drive')
    frequency_axis(ax,True)
    fig.colorbar(m,ax=ax,label='Resonance knob (%)')
    ax.legend(handles=[Patch(color='#e9edf1',label='Passed all saved points'),
                       Patch(color='#e39c78',label='Failed at first saved point')],fontsize=8,loc='upper left')
    status=a['status56'][:,90,:].T
    ax=axes[1,0]
    category=np.where(status==0,0,np.where((status&2)==0,1,2))
    palette=['#e9edf1','#007963','#ca6036']
    ax.pcolormesh(f,np.arange(100)*100/99,category,shading='nearest',cmap=ListedColormap(palette),
                  vmin=-.5,vmax=2.5,rasterized=True)
    ax.set(ylabel='Resonance knob (%)',title='Every recorded point at 995 Hz cutoff, 91% drive')
    ax.legend(handles=[Patch(color=c,label=l) for c,l in zip(palette,['Below selection','Repeats','Fails'])],
              loc='lower left',ncol=3,fontsize=8)
    ax=axes[1,1]
    m=ax.pcolormesh(f,np.arange(100)*100/99,a['gain56'][:,90,:].T,shading='nearest',
                    cmap='magma',vmin=-60,vmax=30,rasterized=True)
    ax.plot(f,np.where(a['loss'][:,90,56]<100,a['loss'][:,90,56]*1./99*100,np.nan),
            color='cyan',lw=1.1,label='First failed point')
    ax.legend(loc='upper left',fontsize=8)
    ax.set(ylabel='Resonance knob (%)',title='Fundamental gain on the same slice')
    fig.colorbar(m,ax=ax,label='Gain (dB)',extend='both')
    for ax in axes.flat:
        frequency_axis(ax)
        ax.set_xlabel('Input frequency (Hz)')
    fig.suptitle('Where the ladder stops repeating after one input cycle',fontsize=18)
    fig.supxlabel('−18 dBFS input  •  failure = cycle RMS difference ≥ 1% of output RMS  •  '
                  'nominal oscillation frequency shown dotted (top left) and dashed (top right)',fontsize=10)
    fig.savefig(root/'breaking-curves.png',dpi=155)
    plt.close(fig)


def spectra(root):
    cases=[('high',16,17),('double',16,17),('near',80,81),('regain',31,32)]
    fig,axes=plt.subplots(2,2,figsize=(13,7.5),layout='constrained')
    for ax,(name,before,after) in zip(axes.flat,cases):
        report=json.loads((root/(name+'-replay.json')).read_text())
        audio=np.load(root/(name+'-audio.npz'))
        for level,color in zip([before,after],['#21627c','#cf5a25']):
            y=audio[str(level)]
            win=np.hanning(len(y))
            amp=2*abs(np.fft.rfft(y*win))/win.sum()
            x=np.fft.rfftfreq(len(y),1/192000)
            db=20*np.log10(np.maximum(amp,1e-8))
            peaks=np.flatnonzero((db[1:-1]>db[:-2])&(db[1:-1]>db[2:]))+1
            peaks=peaks[(db[peaks]>-72)&(x[peaks]<16000)]
            ax.vlines(x[peaks],-75,db[peaks],alpha=.65,color=color,lw=1.1)
            ax.scatter(x[peaks],db[peaks],s=9,label=f'{level*100/99:.1f}% resonance',color=color)
        ax.axvline(1011.75,ls=':',color='0.3',lw=.8)
        ax.axvline(report['frequency'],ls='--',color='0.3',lw=.8)
        ax.set(xscale='log',xlim=(80,16000),ylim=(-73,16),ylabel='Output peak amplitude (dBFS)',
               xlabel='Output frequency (Hz)',title=f"{report['frequency']:,.0f} Hz input, {100*report['drive']:.0f}% drive")
        ax.set_xticks(TICKS[2:-1])
        ax.xaxis.set_major_formatter(HZ)
        if name in ('near','regain'):
            ax.set_xscale('linear')
            if name=='near':
                ax.set_xlim(1020,1160)
                ax.set_xticks([1020,1040,1060,1080,1100,1120,1140,1160])
            else:
                ax.set_xlim(983,1017)
                ax.set_xticks([985,990,995,1000,1005,1010,1015])
            ax.xaxis.set_major_formatter(FuncFormatter(lambda x,_:f'{x:g}'))
            ax.set_xlabel('Output frequency near the oscillation (Hz)')
        ax.legend(fontsize=9)
    fig.suptitle('Spectral lines after holding resonance for 12 extra seconds',fontsize=17)
    fig.supxlabel('Hann-windowed final second  •  dashed = input frequency  •  dotted = 1,012 Hz nominal oscillation  •  cutoff fixed at 995 Hz',fontsize=10)
    fig.savefig(root/'replay-spectra.png',dpi=150)
    plt.close(fig)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path)
    p.add_argument('figure',choices=['boundary','spectra'])
    args=p.parse_args()
    (boundary if args.figure=='boundary' else spectra)(args.directory)
