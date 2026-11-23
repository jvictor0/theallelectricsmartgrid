"""Check saved parameter charts, their junctions and the frequency witnesses."""
import argparse
import json
from pathlib import Path

import numpy as np

from phase_tube import Ball, Tone, check
from resonance_chain import Chain
from arc_certificate import Arc


def require(ok, message):
    if not ok:
        raise ValueError(message)


def replay(chain_file, graph_file, recalculate):
    rows=list(map(json.loads,chain_file.open()))
    setup=rows.pop(0)
    require(rows and all(r.get('kind')=='cell' for r in rows),'incomplete chain')
    wave=np.load(chain_file.with_suffix('.npz'))
    states=wave['states']
    require(len(states)==len(rows)+1,'missing junction')
    q,scale=setup['q'],np.array(setup['scale'])
    t=Tone(q,setup['cutoff'],setup['drive'])
    if 'normals' in wave:
        chain=Arc(t,scale)
        normals=wave['normals']
        require(np.array_equal(normals[0],np.eye(15)[-1]) and np.array_equal(normals[-1],normals[0]),'endpoint gauges')
    else:
        chain=Chain(t,scale)
        normals=None
    require(rows[0]['lo']==0,'missing unique feedforward anchor')
    last=0.
    extrema=[]
    for i,r in enumerate(rows):
        lo,hi,rad=r['lo'],r['hi'],r['radius']
        require(lo==last and rad>0,'missing shared parameter endpoint')
        left,right=states[i:i+2]
        if recalculate:
            if normals is None: ok,lip,eta=chain.check(lo,hi,left,right,rad)
            else: ok,lip,eta=chain.check_arc(left,right,*normals[i:i+2],rad)
        else:
            lip,eta=r['contraction'],r['defect']
            ok=(Ball(lip)*rad+eta).hi()<rad
        require(ok,f'failed cell {i}')
        err=float((Ball(eta)/(1.-Ball(lip))).hi())
        if normals is not None:
            require(np.all(left[:,-1]==lo) and np.all(right[:,-1]==hi),'mismatched arclength chart')
            extrema.append((lo,hi,err))
        last=hi
    t.set_resonance(81/99)
    require(last==t.coeffs[1],'incomplete resonance coverage')
    report=dict(cells=len(rows),q=q,base_feedback=last)
    if extrema:
        turning=np.where(np.diff(np.sign([b-a for a,b,_ in extrema]))!=0)[0]
        require(len(turning)==2,'unexpected numerical turn count')
        first,second=turning
        mid=(first+second)//2
        high=extrema[:mid]
        low=extrema[mid:]
        # Every chart contains each endpoint orbit inside its endpoint error
        # ball. Its full parameter range lies between its proposed K edges ±err.
        report['max_K_before_return']=[max((Ball(max(a,b))-e).lo() for a,b,e in high),
                                      max((Ball(max(a,b))+e).hi() for a,b,e in high)]
        report['min_K_after_return']=[min((Ball(min(a,b))-e).lo() for a,b,e in low),
                                     min((Ball(min(a,b))+e).hi() for a,b,e in low)]
    if graph_file:
        g=json.loads(graph_file.read_text())
        v=np.load(graph_file.with_suffix('.npz'))
        require(int(v['q'])==q and np.array_equal(v['coefficients'],t.coeffs),'frequency model mismatch')
        if recalculate:
            checked=check(v['table'],v['tangent'],v['coefficients'],q,g['radius'],v['scale'],
                    g['freq_radius_hz'],g['base_feedback_radius'],int(np.log2(g['block_samples'])),8)
            for key in ('total_power','projection_real','projection_imag','effective_feedback'):
                require(np.array_equal(g[key],checked[key]),f'reported {key} differs from replay')
            g=checked
        require(g['accepted'],'frequency enclosure failed')
        nodes=v['table'][::len(v['table'])//q]
        difference=(Ball(states[-1])-Ball(nodes)).magnitude()
        union=(Ball(difference)+Ball(scale)*err)/Ball(v['scale'])
        distance=float(union.hi().max())
        require(distance < g['radius'],'zero-resonance branch outside the invariant graph tube')
        report.update(endpoint_error=distance,graph_radius=g['radius'])
    return report


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('chain',type=Path)
    ap.add_argument('--graph',type=Path)
    ap.add_argument('--recalculate',action='store_true')
    args=ap.parse_args()
    print(json.dumps(replay(args.chain,args.graph,args.recalculate),indent=2))
