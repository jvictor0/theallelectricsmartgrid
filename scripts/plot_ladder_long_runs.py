#!/usr/bin/env python3
"""Summarize representative long runs and show averaging versus settling."""
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
    report=json.loads((root/'report.json').read_text())
    data=np.load(root/'checkpoint-trials.npz')
    cases=report['cases']; ids=data['point_ids']; values=data['values']; times=data['times']
    short=np.load(root.parent/'two-minutes/checkpoint-trials.npz')
    assert np.array_equal(ids,short['point_ids'])
    assert np.array_equal(values[:,:len(short['times'])],short['values'])
    summary={'repeat_check':'All overlapping checkpoint values match the independent two-minute run exactly.',
             'not_a_population_estimate':True,'groups':{},'persistent_clusters':[]}
    for group in sorted(set(c['group'] for c in cases)):
        chosen=[c for c in cases if c['group']==group]
        summary['groups'][group]={'count':len(chosen),
            'initial_cv':[c['checkpoints'][0]['late_half']['fundamental']['cv'] for c in chosen],
            'final_long_cv':[c['checkpoints'][-1]['late_half']['fundamental']['cv'] for c in chosen],
            'final_short_cv':[c['checkpoints'][-1]['late_0_5s']['fundamental']['cv'] for c in chosen],
            'mean_change_db_120_to_600':[10*np.log10(c['checkpoints'][-1]['late_half']['fundamental']['mean']/c['checkpoints'][-2]['late_half']['fundamental']['mean']) for c in chosen]}
    for i,c in enumerate(cases):
        power=values[i,-1,0,:100,1]
        if power.std()/power.mean()<=.05:
            continue
        db=10*np.log10(np.maximum(power,1e-30)/.03125)
        order=np.argsort(db); split=int(np.argmax(np.diff(db[order])))+1
        low=set(order[:split].tolist())
        changes=[len(low.symmetric_difference(set(np.argsort(values[i,t,0,:100,1])[:split].tolist()))) for t in range(len(times))]
        summary['persistent_clusters'].append({'point_id':c['point_id'],'low_count':split,'high_count':100-split,
            'low_db_range':[float(db[order[0]]),float(db[order[split-1]])],
            'high_db_range':[float(db[order[split]]),float(db[order[-1]])],
            'membership_difference_from_final_by_checkpoint':changes,
            'reset_db':float(10*np.log10(values[i,-1,0,100,1]/.03125))})
    variable=[c for c in cases if c['group'] not in ('stable_control','decaying_control')]
    summary['variable_cases']=len(variable)
    summary['below_1_percent_final']=sum(c['checkpoints'][-1]['late_half']['fundamental']['cv']<.01 for c in variable)
    (root/'findings.json').write_text(json.dumps(summary,indent=2)+'\n')
    plt.rcParams.update({'font.size':11,'figure.facecolor':'#fafafa','axes.facecolor':'#fafafa',
                         'axes.spines.top':False,'axes.spines.right':False})
    titles={3936113:'Persistent wobble; longer averaging works',11159501:'A transient that actually settles',
            55999511:'A weak fundamental needs longer averaging',89931773:'Different responses remain different'}
    fig,axes=plt.subplots(2,2,figsize=(12,8),layout='constrained')
    for ax,(point,title) in zip(axes.flat,titles.items()):
        c=next(c for c in cases if c['point_id']==point)
        for mode,label,color in [('late_0_5s','Final 0.5-second window','#cb6b36'),('late_half','Final half of each run','#227f90')]:
            cv=np.array([x[mode]['fundamental']['cv']*100 for x in c['checkpoints']])
            ax.plot(times,np.maximum(cv,.0001),'-o',ms=4,color=color,label=label)
        ax.axhline(5,color='gray',ls=':',lw=1)
        ax.set(xscale='log',yscale='log',xlabel='Elapsed simulated time (seconds)',ylabel='Fundamental power SD / mean (%)',
               title=f"{title}\nf = {c['frequency_hz']:.1f} Hz · cutoff = {c['cutoff_hz']:.1f} Hz",ylim=(.00008,200))
        ax.set_xticks([1,10,60,600],['1','10','60','600']); ax.grid(alpha=.2)
    axes[0,0].legend(fontsize=9)
    fig.suptitle('Settling longer and measuring longer are different experiments',fontsize=17)
    fig.supxlabel('Same 100 randomized starts · A = 0.25 · detector off · production-style float/rational model\n'
                  'At 600 seconds, the blue measurement spans 300 seconds. Values below 0.0001% are plotted at the floor.',fontsize=10)
    fig.savefig(root/'averaging-versus-settling.png',dpi=180); plt.close(fig)
    fig,axes=plt.subplots(2,2,figsize=(12,7.5),layout='constrained')
    for ax,point in zip(axes.flat,[82905776,82985375,75993866,89931773]):
        i=int(np.where(ids==point)[0][0]); c=cases[i]
        db=np.sort(10*np.log10(values[i,-1,0,:100,1]/.03125))
        reset=10*np.log10(values[i,-1,0,100,1]/.03125)
        ax.scatter(np.arange(1,101),db,s=15,color='#227f90',label='Randomized starts')
        ax.axhline(reset,color='#cb6b36',ls='--',lw=1.4,label='Reset-state control')
        ax.set(xlabel='Start, sorted by final power',ylabel='Fundamental power gain (dB)',
               title=f"f = {c['frequency_hz']:.0f} Hz · cutoff = {c['cutoff_hz']:.0f} Hz\nDrive {100*c['drive_knob']:.1f}% · resonance {100*c['resonance_knob']:.1f}%")
        ax.grid(alpha=.2)
    axes[0,0].legend(fontsize=9)
    fig.suptitle('Separated fundamental-power levels remain after ten minutes\nEach dot is one trajectory, measured over seconds 300–600',fontsize=16)
    fig.savefig(root/'persistent-levels.png',dpi=180); plt.close(fig)
    print(json.dumps({'variable_cases':summary['variable_cases'],'below_1_percent_final':summary['below_1_percent_final'],
                      'persistent_clusters':len(summary['persistent_clusters'])}))


if __name__=='__main__':
    main()
