"""Knob mappings and a return-map branch in the detector-disabled ladder survey."""
import argparse
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def knob_plot(out):
    x = np.linspace(0,1,1001)
    gain = .25*20**x
    rational = np.minimum(1,gain*(gain**2+27)/(9*gain**2+27))
    feedback = 3.9*np.expm1(np.log(49)*x)/48
    tanh_slope = gain/np.tanh(gain)
    grid_color = '#dfe5e7'
    fig,axes = plt.subplots(2,2,figsize=(10.4,7.3),layout='constrained')
    for ax,title,knob in zip(axes.flat,
            ['Drive gain  g','Interstage slope at zero  s','Three interstage slopes  s³','Requested feedback  k'],
            ['Drive','Drive','Drive','Resonance']):
        ax.set(xlim=(0,100),xlabel=knob+' knob (%)',title=title)
        ax.set_xticks([0,25,50,75,100])
        ax.grid(color=grid_color,linewidth=.7)
        ax.spines[['top','right']].set_visible(False)
        if knob=='Drive':
            ax.axvline(100*np.log(4)/np.log(20),color='0.6',ls=':',lw=1)
    axes[0,0].plot(100*x,gain,color='#127568',lw=2.5)
    axes[0,0].plot(100*np.log(4)/np.log(20),1,marker='o',color='#127568')
    axes[0,0].annotate('g = 1 at 46.3%',(100*np.log(4)/np.log(20),1),
                       xytext=(-8,17),textcoords='offset points',ha='center')
    axes[0,0].set_ylim(0,5.25)
    for ax,power in [(axes[0,1],1),(axes[1,0],3)]:
        ax.plot(100*x,(gain/rational)**power,color='#127568',lw=3,label='Survey · rational tanh')
        ax.plot(100*x,tanh_slope**power,color='#943d29',ls='--',lw=1.7,label='Real tanh')
        ax.legend(loc='upper left',frameon=False,fontsize=9)
    axes[1,0].set(ylim=(0,130),yticks=[0,25,50,75,100,125])
    axes[1,1].plot(100*x,feedback,color='#365c94',lw=2.5)
    axes[1,1].plot(50,3.9/8,'o',color='#365c94')
    axes[1,1].annotate('k = 0.4875 at 50%',(50,3.9/8),xytext=(0,18),
                       textcoords='offset points',ha='center')
    axes[1,1].set_ylim(0,4.1)
    fig.suptitle('Naive reupping ladder knob mappings',fontsize=16)
    fig.supxlabel('Feedback shown before the cutoff-dependent cap and envelope reduction.',fontsize=10)
    fig.savefig(out/'knob-mappings.png',dpi=160)
    plt.close(fig)


def branch_plot(out):
    data=np.load(out/'arc-0.005.npy')
    r=100*data[:,4]
    turns=np.flatnonzero(np.diff(r)[1:]*np.diff(r)[:-1]<0)+1
    assert len(turns)==2
    idx=np.arange(len(r))
    fig,axes=plt.subplots(1,2,figsize=(11,4.2),layout='constrained')
    colors=['#117363','#b9501e','#6c4d91']
    labels=['From zero-feedback solution','After first turn','After second turn']
    for ax,v,label in zip(axes,[data[:,0],data[:,6]],['First filter state at input crest','Fundamental gain (dB)']):
        for a,b,col,name,ls in zip([0,*turns],[*turns,len(r)-1],colors,labels,['-','--',':']):
            use = (idx>=a)&(idx<=b)
            ax.plot(r[use],v[use],color=col,ls=ls,lw=2.2,label=name)
        ax.scatter(r[turns],v[turns],s=24,color='black',zorder=5)
        ax.set(xlabel='Resonance knob (%)',ylabel=label,xlim=(0,100))
        ax.grid(alpha=.25)
        ax.axvline(70,color='.6',ls=':',lw=1)
        ax.spines[['top','right']].set_visible(False)
    axes[0].legend(frameon=False,fontsize=9)
    fig.suptitle('One connected family of repeating waveforms turns twice',fontsize=14)
    fig.supxlabel('1,066.67 Hz input at −18 dBFS  ·  995.4 Hz cutoff  ·  90.9% drive  ·  dots mark the folds',fontsize=10)
    fig.savefig(out/'periodic-branch.png',dpi=160)
    plt.close(fig)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    out=parser.parse_args().directory
    knob_plot(out)
    branch_plot(out)
