"""Compare the ordinate near each peak over one original cutoff cell."""
from pathlib import Path
import argparse
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def main(directory):
    ref=directory/'refinement'
    groups=[np.load(ref/f'group-{j}.npz') for j in range(4)]
    controls=groups[0]['controls']
    gains=10*np.log10(np.mean([g['wet'] for g in groups],axis=0)/np.mean([g['dry'] for g in groups],axis=0))
    summary=json.loads((directory/'summary.json').read_text())
    band_centers=np.load(directory/'curves.npz')['centers']
    fig,axes=plt.subplots(1,3,figsize=(11,3.9),layout='constrained')
    for ax,case in zip(axes,summary['rows'][:3]):
        d,r,c=case['controls']
        center=case['largest_center_hz']
        band=np.argmin(abs(band_centers-center))
        match=np.isclose(controls[:,0],d,rtol=0,atol=1e-14)&np.isclose(controls[:,1],r,rtol=0,atol=1e-14)
        order=np.flatnonzero(match)[np.argsort(controls[match,2])]
        hz=controls[order,2];db=gains[order,band]
        target=np.argmin(abs(hz-c));keep=np.arange(len(hz))!=target
        ax.plot(hz[[0,-1]],db[[0,-1]],'--',color='#b56831',lw=2,label='Original interpolation')
        ax.plot(hz[keep],db[keep],'.-',color='#14758a',lw=1.6,ms=8,label='Refined measurements')
        ax.plot(c,db[target],'o',ms=6,color='#26292f',label='Direct measurement')
        ax.set_xscale('log');ax.set_xticks([hz[0],c,hz[-1]],labels=[f'{h:.1f}' for h in (hz[0],c,hz[-1])]);ax.minorticks_off()
        ax.tick_params(axis='x',labelrotation=20)
        ax.set_title(f'{center:.1f} Hz output band')
        ax.set_xlabel('Filter cutoff (Hz)')
        ax.grid(axis='y',alpha=.2)
    axes[0].set_ylabel('Wet / dry power (dB)')
    fig.legend(*axes[0].get_legend_handles_labels(),loc='outside lower center',ncols=3,frameon=False)
    fig.suptitle('One cutoff grid cell near resonance',fontsize=15)
    fig.savefig(directory/'cutoff-interpolation.png',dpi=160)
    plt.close(fig)

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('directory',type=Path)
    main(ap.parse_args().directory)
