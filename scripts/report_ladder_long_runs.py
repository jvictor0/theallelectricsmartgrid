#!/usr/bin/env python3
"""Compare settling and averaging length, preserving coherent per-trial projections."""
import argparse
import json
from pathlib import Path
import struct
import numpy as np

NAMES=('total','fundamental','real','imaginary','dc','residual')
MODES=('late_half','late_0_5s','late_2s','late_10s','prefix')


def summarize(values):
    result={}
    for j,name in enumerate(NAMES):
        x=values[:100,j]
        mean=float(x.mean()); sd=float(x.std())
        result[name]={'mean':mean,'sd':sd,'cv':sd/abs(mean) if mean else 0,
                      'min':float(x.min()),'max':float(x.max()),'reset':float(values[100,j])}
    return result


def metrics(sums,samples):
    total=sums[...,0]/samples
    real=8*sums[...,1]/samples
    imaginary=8*sums[...,2]/samples
    dc=sums[...,3]/samples
    fundamental=(real*real+imaginary*imaginary)/2
    residual=total-fundamental-dc*dc
    assert np.all(residual>=-2e-6*np.maximum(total,1e-20)),float(residual.min())
    return np.stack((total,fundamental,real,imaginary,dc,np.maximum(0,residual)),axis=-1)


def run(root,selection):
    meta=json.loads((root/'metadata.json').read_text())
    assert json.loads((root/'progress.json').read_text())['status']=='complete'
    original=json.loads(selection.read_text())
    lookup={c['baseline']['point_id']:c for c in original['cases']}
    with (root/'partials.bin').open('rb') as stream:
        header=struct.unpack('<6Q',stream.read(48))
    assert header==(0x31474e4f4c44414c,len(meta['cases']),101,meta['slots'],192000,20)
    shape=(meta['slots'],len(meta['cases']),101,5)
    assert (root/'partials.bin').stat().st_size==48+np.prod(shape)*4
    raw=np.memmap(root/'partials.bin',mode='r',dtype='<f4',offset=48,shape=shape)
    checkpoints=sorted(set([t for t in [1,2,5,10,30,60,120,meta['seconds']] if t<=meta['seconds']]))
    trials=np.zeros((len(meta['cases']),len(checkpoints),len(MODES),101,6))
    reports=[]; series=[]; max_error=0; comparisons=0
    for ci,case in enumerate(meta['cases']):
        intervals=np.array(case['intervals'],dtype='int64')
        assert intervals[0,0]==0 and np.all(intervals[1:,0]==intervals[:-1,1])
        boundaries=np.r_[0,intervals[:,1]]
        cumulative=np.concatenate((np.zeros((1,101,4)),np.cumsum(raw[:len(intervals),ci,:,:4],axis=0,dtype='float64')))
        assert np.all(np.isfinite(cumulative))
        def interval(begin,end):
            i,j=np.searchsorted(boundaries,[begin,end])
            assert boundaries[i]==begin and boundaries[j]==end,(case['point_id'],begin,end)
            return metrics(cumulative[j]-cumulative[i],end-begin)
        old=lookup[case['point_id']]['baseline']
        frequency=original['manifest']['frequencies'][old['f_i']]
        assert case['actual_hz']==frequency['actual_hz']
        for scope,bounds in zip(('quarter','half','full','late'),frequency['scopes']):
            observed=interval(*bounds)[:100]
            aggregates=np.stack((observed.mean(axis=0),observed.std(axis=0),observed.min(axis=0),observed.max(axis=0)),axis=-1)
            for j,name in enumerate(NAMES):
                for k,stat in enumerate(('mean','sd','min','max')):
                    expected=old[f'{scope}_{name}_{stat}']
                    error=abs(aggregates[j,k]-expected)/max(1,abs(expected))
                    max_error=max(max_error,float(error)); comparisons+=1
        q=case['period_samples']; w=case['window_samples']
        case_report={'point_id':case['point_id'],'group':lookup[case['point_id']]['group'],
                     'frequency_hz':case['actual_hz'],'cutoff_hz':old['cutoff_hz'],'drive_knob':old['drive_knob'],
                     'resonance_knob':old['resonance_knob'],'checkpoints':[]}
        complete=meta['seconds']*192000//w
        starts=np.arange(complete)*w
        ends=starts+w
        begin_indices=np.searchsorted(boundaries,starts); end_indices=np.searchsorted(boundaries,ends)
        assert np.all(boundaries[begin_indices]==starts) and np.all(boundaries[end_indices]==ends)
        windows=metrics(cumulative[end_indices]-cumulative[begin_indices],w)
        for ti,t in enumerate(checkpoints):
            end=t*192000//q*q
            entries={}
            for mi,mode in enumerate(MODES):
                duration={'late_half':t/2,'late_0_5s':.5,'late_2s':2,'late_10s':10,'prefix':t}[mode]
                begin=int(max(0,t-duration)*192000)
                begin=(begin+q-1)//q*q
                values=interval(begin,end)
                trials[ci,ti,mi]=values
                entries[mode]={'begin_seconds':begin/192000,'end_seconds':end/192000,**summarize(values)}
            indices=(starts>=t*192000/2)&(ends<=t*192000)
            late_windows=windows[indices,:100]
            means=late_windows.mean(axis=0); deviations=late_windows.std(axis=0)
            temporal=deviations[:,:2]/np.maximum(means[:,:2],1e-30)
            entries['time_seconds']=t
            entries['temporal_total_cv']=float(temporal[:,0].mean())
            entries['temporal_fundamental_cv']=float(temporal[:,1].mean())
            entries['coherence']=float(np.mean((means[:,2]**2+means[:,3]**2)/(2*np.maximum(means[:,1],1e-30))))
            case_report['checkpoints'].append(entries)
        # Retain a compact ensemble time course; raw data still holds every start/window.
        indices=np.unique(np.r_[np.arange(min(20,complete)),np.linspace(0,complete-1,min(240,complete),dtype=int)])
        series.append({'point_id':case['point_id'],'times':((starts[indices]+ends[indices])/384000).tolist(),
                       'total_mean':windows[indices,:100,0].mean(axis=1).tolist(),
                       'fundamental_mean':windows[indices,:100,1].mean(axis=1).tolist(),
                       'total_sd':windows[indices,:100,0].std(axis=1).tolist(),
                       'fundamental_sd':windows[indices,:100,1].std(axis=1).tolist()})
        reports.append(case_report)
    validation={'comparisons':comparisons,'max_scaled_error_against_original':max_error,'passed':max_error<2e-5}
    (root/'baseline-validation.json').write_text(json.dumps(validation,indent=2)+'\n')
    assert validation['passed'],validation
    np.savez_compressed(root/'checkpoint-trials.npz',values=trials,point_ids=np.array([c['point_id'] for c in meta['cases']]),
                        times=np.array(checkpoints),modes=np.array(MODES),metrics=np.array(NAMES))
    (root/'report.json').write_text(json.dumps({'checkpoints':checkpoints,'cases':reports},indent=2,allow_nan=False)+'\n')
    (root/'ensemble-windows.json').write_text(json.dumps(series,allow_nan=False)+'\n')
    print(json.dumps(validation))
    for c in reports:
        first=c['checkpoints'][0]; last=c['checkpoints'][-1]
        print(c['point_id'],c['group'],'fund CV half',round(first['late_half']['fundamental']['cv'],4),'->',round(last['late_half']['fundamental']['cv'],4),
              'fixed .5s',round(last['late_0_5s']['fundamental']['cv'],4),'time CV',round(last['temporal_fundamental_cv'],4),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('selection',type=Path)
    args=parser.parse_args()
    run(args.directory,args.selection)
