#!/usr/bin/env python3
"""Propose center Jacobians and whole-cell Hessians for exact Lean replay."""

import json
from decimal import Decimal as D
from pathlib import Path

from check_ladder_periodic_enclosure import I, Interval, mm
from generate_ladder_interval_witness import box
from generate_ladder_drift_profile_cell import propose as profile_proposal, vector
from generate_ladder_drift_block_cell import propose as block_proposal, matrix


def tensor(values):
    return '!['+', '.join(matrix(row) for row in values)+']'


def propose(root):
    data = block_proposal(root)
    profile = profile_proposal(root)
    inverse = data['inverse']
    p,dp,ddp = profile['ranges']
    cos,sin = [v+Interval(D('-0.0002'),D('0.0002')) for v in profile['base']]
    rho = p[0]+Interval(D('-0.000002'),D('0.000002'))
    diagonal = [ddp[0]*cos-2*dp[0]*sin-rho*cos,
                ddp[0]*sin+2*dp[0]*cos-rho*sin,ddp[1],ddp[2]]
    cross = [-sin,cos,I(0),I(0)]
    raw_h = [[[diagonal[i] if j==k==0 else cross[i] if (j,k) in [(0,1),(1,0)] else I(0)
               for k in range(4)] for j in range(4)] for i in range(4)]
    h = [[[box(sum((inverse[i][l]*raw_h[l][j][k] for l in range(4)),I(0)),22)
           for k in range(4)] for j in range(4)] for i in range(4)]
    pc,dpc = profile['pointRanges'][:2]
    cc,sc = profile['base']
    raw_j = [[dpc[0]*cc-pc[0]*sc,cc,I(0),I(0)],
             [dpc[0]*sc+pc[0]*cc,sc,I(0),I(0)],
             [dpc[1],I(0),I(1),I(0)],[dpc[2],I(0),I(0),I(1)]]
    center_jac = [[box(v,22) for v in row] for row in mm(inverse,raw_j)]
    stored = json.loads((root/'build/results/ladder-locked-error/witness.json').read_text())['coefficients']
    a,b,k,g,inv = [Interval(D(stored[key][0]),D(stored[key][1])) for key in
                   ['alpha','beta','feedback','gain','inverseNormalization']]
    hessians, centers = [h],[center_jac]
    for n in range(162):
        next_j = data['matrices'][n+1]
        new_center = [[box(b*center_jac[0][j]-a*k*center_jac[3][j],22) for j in range(4)]]
        new_h = [[[box(b*h[0][j][l]-a*k*h[3][j][l],22) for l in range(4)] for j in range(4)]]
        for stage in range(1,4):
            cs,ss = data['center_saturations'][n][stage-1],data['saturations'][n][stage-1]
            center_slope = g*(1-cs*cs)*inv
            slope,curvature = g*(1-ss*ss)*inv,-2*g*g*ss*(1-ss*ss)*inv
            new_center.append([box(b*center_jac[stage][j]+a*center_slope*new_center[-1][j],22) for j in range(4)])
            new_h.append([[box(a*(curvature*next_j[stage-1][j]*next_j[stage-1][l]+
                                  slope*new_h[-1][j][l])+b*h[stage][j][l],22)
                           for l in range(4)] for j in range(4)])
        h,center_jac = new_h,new_center
        hessians.append(h); centers.append(center_jac)
    return {'hessians': hessians, 'centers': centers}


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root/'formal/DSP/Filters/NaiveReuppingLadder/HarmonicBalance/Certificates'
    data = propose(root)
    hessians, centers = data['hessians'], data['centers']
    lines = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData',
             'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.DerivativeEnclosure',
             'import DSP.Analysis.PolarTube.DecodeHessianEnclosure',
             '/-! Generated proposals; all sample checks require kernel replay. -/',
             'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData',
             'open LeanCert.Core Enclosure PolarTube DriftProfileCellData',
             'set_option maxRecDepth 100000','set_option maxHeartbeats 0']
    for name,values,render,typename,fallback in [
            ('hessian',hessians,tensor,'HessianBox','(fun _ _ _ => IntervalRat.singleton 0)'),
            ('centerMatrix',centers,matrix,'MatrixBox','(fun _ _ => IntervalRat.singleton 0)')]:
        lines += [f'def {name}Data : Array {typename} := #[\n'+',\n'.join(map(render,values))+'\n]',
                  f'def {name} (n : ℕ) : {typename} := {name}Data[n]?.getD {fallback}']
    lines.append('end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData')
    (directory/'DriftSecondCellData.lean').write_text('\n'.join(lines)+'\n')
    def mixed(n,nn):
        return (f'∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation {n}) '
                f'(fun i => matrices {nn} i j) (fun i => matrices {nn} i k) '
                f'(fun i => hessian {n} i j k) (fun i => hessian {nn} i j k) = true')
    def center(n,nn):
        return (f'matrixCheck LockedData.coefficients (centerMatrix {n}) (centerMatrix {nn}) '
                f'(centerSaturation {n}) = true')
    for chunk in range(17):
        name=f'DriftSecondCheck{chunk:02}'
        count=min(10,162-10*chunk)
        source=['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData',
                f'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}',
                'open Enclosure DriftBlockCellData DriftSecondCellData',
                'set_option maxRecDepth 100000','set_option maxHeartbeats 0']
        for offset in range(count):
            n=10*chunk+offset
            source += [f'private theorem second_{offset:02} : '+mixed(n,n+1)+' := by decide +kernel',
                       f'private theorem center_{offset:02} : '+center(n,n+1)+' := by decide +kernel']
        for label,prop in [('second',mixed),('center',center)]:
            source += [f'theorem {label} : ∀ j : Fin {count}, let n := {10*chunk}+j.val; '+prop('n','(n+1)')+' := by',
                       '  intro j','  fin_cases j']+[f'  · exact {label}_{j:02}' for j in range(count)]
        source.append(f'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}')
        (directory/(name+'.lean')).write_text('\n'.join(source)+'\n')
    source=[f'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck{chunk:02}' for chunk in range(17)]
    source += ['namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondChecks',
               'open Enclosure DriftBlockCellData DriftSecondCellData']
    for label,prop in [('second',mixed),('center',center)]:
        source += [f'theorem {label} : ∀ n < 162, '+prop('n','(n+1)')+' := by','  intro n hn','  interval_cases n']
        source += [f'  · exact DriftSecondCheck{n//10:02}.{label} ⟨{n%10},by decide⟩' for n in range(162)]
    source.append('end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondChecks')
    (directory/'DriftSecondChecks.lean').write_text('\n'.join(source)+'\n')
    print('Generated Hessian and center-Jacobian witnesses for 162 samples; Lean replay required.')


if __name__ == '__main__':
    main()
