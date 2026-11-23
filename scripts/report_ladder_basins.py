#!/usr/bin/env python3
"""Summarize basin tests and compare forward iteration with root continuation."""
import argparse
from collections import Counter,defaultdict
import csv
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def read(path):
    return [r for r in csv.DictReader(path.open()) if r['half']=='1']


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path)
    root=parser.parse_args().root
    order=[82905776,82985375,75993866,89931773]
    figures=[];report=[]
    colors={'low':'#087f8c','high':'#c55d25'}
    for point,letter in zip(order,'ABCD'):
        rows=read(root/f'{point}-direct.csv')
        baseline={r['family']:r for r in rows if r['family'] in ['saved_low','saved_high']}
        centers={k:float(v['fundamental']) for k,v in baseline.items()}
        threshold=np.sqrt(centers['saved_low']*centers['saved_high'])
        classify=lambda r:'high' if float(r['fundamental'])>threshold else 'low'
        groups=defaultdict(Counter)
        error=0
        for r in rows:
            level=classify(r);groups[(r['family'],r['epsilon'])][level]+=1
            error=max(error,abs(10*np.log10(float(r['fundamental'])/centers['saved_'+level])))
        assert error<.01,error
        paths=[]
        for path in sorted(root.glob(f'{point}-*.csv')):
            if path.name.endswith('-direct.csv'):continue
            data=read(path);counts=Counter(classify(r) for r in data)
            paths.append({'route':data[0]['route'],'ramp_seconds':float(data[0]['ramp_seconds']),
                          'trials':len(data),'counts':dict(counts)})
        roots=json.loads((root/f'{point}-periodic.json').read_text())
        stable=[r for r in roots['roots'] if r['spectral_radius']<1]
        assert len(stable)==1
        good=stable[0]
        agreement=abs(10*np.log10(good['fundamental']/centers['saved_high']))
        assert agreement<.0001,agreement
        item={'point_id':point,'letter':letter,'direct_trials':len(rows),
              'groups':[{'family':k[0],'epsilon':float(k[1]),'counts':dict(v)} for k,v in groups.items()],
              'paths':paths,'max_fundamental_distance_to_reference_db':float(error),
              'periodic_root_power_agreement_db':float(agreement),
              'periodic_root_count_found':len(roots['roots']),
              'periodic_root_spectral_radius':good['spectral_radius'],
              'newton_seed_count':roots['seed_count'],'newton_failed_seeds':roots['failed_seeds'],
              'continuation_unstable_fraction_ranges':{}}
        for route,path in roots['continuations'].items():
            unstable=[r['fraction'] for r in path['steps'] if r.get('radius',0)>1]
            item['continuation_unstable_fraction_ranges'][route]=[min(unstable),max(unstable)] if unstable else []
        extra=root/f'{point}-period-3.json'
        if extra.exists():
            period3=json.loads(extra.read_text())
            item['period_three_families']=[{'root_indices':family,
                'spectral_radius':period3['roots'][family[0]]['spectral_radius'],
                'power':period3['roots'][family[0]]['power']} for family in period3['cycle_families']]
            item['unstable_cycle_perturbations']=[{'sign':r['sign'],'power':r['final']['power']}
                for r in period3['unstable_eigenvector_escapes']]
        report.append(item)
        fresh=[r for r in rows if r['family']=='fresh_random']
        model=json.loads((root/f'{point}-model.json').read_text());p=model['coefficients']
        scale=np.array([.25+p[2]/p[4]]+[1/p[4]]*3)
        states=np.array([[float(r['final_'+c]) for c in 'abcd'] for r in fresh])/scale
        center=states.mean(axis=0);_,_,basis=np.linalg.svd(states-center,full_matrices=False)
        projected=(states-center)@basis[:2].T
        figures.append((fresh,projected,roots))
    (root/'findings.json').write_text(json.dumps({'cases':report,
         'limits':'Selected points, sampled initial histories and paths, finite durations, unvalidated binary64 root/eigenvalue estimates; no completeness or interval existence proof.'},indent=2)+'\n')
    plt.rcParams.update({'font.size':10,'figure.facecolor':'#fafafa','axes.facecolor':'#fafafa',
                         'axes.spines.top':False,'axes.spines.right':False})
    fig,axes=plt.subplots(2,4,figsize=(15,7.5),layout='constrained')
    route_colors={'feedback':'#6554a4','drive':'#c55d25','both':'#5a7880'}
    for col,(item,figure) in enumerate(zip(report,figures)):
        rows,points,roots=figure;counts=next(g['counts'] for g in item['groups'] if g['family']=='fresh_random')
        threshold=np.sqrt(min(float(r['fundamental']) for r in rows)*max(float(r['fundamental']) for r in rows))
        for level in ['low','high']:
            mask=np.array([(float(r['fundamental'])>threshold)==(level=='high') for r in rows])
            axes[0,col].scatter(points[mask,0],points[mask,1],s=4 if level=='low' else 18,alpha=.25 if level=='low' else .8,color=colors[level])
        axes[0,col].set(title=f"{item['letter']} · random starts: {counts['low']} / {counts['high']}",xlabel='State projection 1',ylabel='State projection 2')
        axes[0,col].set_aspect('equal',adjustable='datalim');axes[0,col].grid(alpha=.15)
        ax=axes[1,col]
        for route,path in roots['continuations'].items():
            xs=[s['fraction'] for s in path['steps'] if 'radius' in s];ys=[s['radius'] for s in path['steps'] if 'radius' in s]
            ax.plot(xs,ys,color=route_colors[route],lw=1.6,label={'feedback':'Raise feedback only','drive':'Raise drive only','both':'Raise both'}[route])
        ax.axhline(1,color='#333',ls='--',lw=1);ax.axhspan(1,20,color='#b95b45',alpha=.07)
        ax.set(yscale='log',ylim=(.01,15),xlim=(0,1),xlabel='Progress toward target settings',ylabel='Largest return-map multiplier',
               title=f"Final multiplier: {item['periodic_root_spectral_radius']:.3f}")
        ax.grid(alpha=.15)
    axes[1,0].legend(loc='lower right',fontsize=8)
    fig.suptitle('Two observed response families; the route into the target matters',fontsize=18)
    fig.supxlabel('Top: final states at a fixed input phase after 60 seconds; teal = lower fundamental, orange = input-dominated.\n'
                  'Bottom: numerical periodic solution continued through 129 settings; below 1 predicts local attraction, above 1 instability.',fontsize=10)
    fig.savefig(root/'basins-and-continuation.png',dpi=180);plt.close(fig)
    for r in report:
        print(json.dumps({k:v for k,v in r.items() if k not in ['groups','paths']}))


if __name__=='__main__':main()
