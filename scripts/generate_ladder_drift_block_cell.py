#!/usr/bin/env python3
"""Propose exact centered replay data for one 162-sample curved-chart cell.

The producer is untrusted. Generated Lean checks establish containment; this
file does not claim an attracting curve or certify a full phase grid.
"""

import json
from decimal import Decimal as D
from fractions import Fraction
from pathlib import Path

from check_ladder_periodic_enclosure import I, Interval, sine_cosine, tanh, mm, rational_inverse
from generate_ladder_interval_witness import box
from generate_ladder_drift_profile_cell import propose as profile_proposal, interval, vector


def matrix(values):
    return "![" + ", ".join(vector(row) for row in values) + "]"


def proposed_tanh(value):
    return box(tanh(value)+Interval(D('-1e-20'), D('1e-20')), 22)


def propose(root):
    profile = profile_proposal(root)
    source = json.loads((root/'build/results/ladder-drift-certificate/averages.json').read_text(), parse_float=Fraction)
    inverse = [[box(I(v), 24) for v in row] for row in rational_inverse(source['chart']['transform'])]
    stored = json.loads((root/'build/results/ladder-locked-error/witness.json').read_text())['coefficients']
    a,b,k,g,inv = [Interval(D(stored[key][0]),D(stored[key][1])) for key in
                   ['alpha','beta','feedback','gain','inverseNormalization']]
    pi = Interval(D('3.14159265358979323846'), D('3.14159265358979323847'))
    forcing_base = [box(sine_cosine(pi/27, kind)+Interval(D('-1e-20'),D('1e-20')),22)
                    for kind in [False,True]]
    forcing_carriers = [[I(1),I(0)]]
    for _ in range(53):
        c,s = forcing_carriers[-1]
        forcing_carriers.append([box(c*forcing_base[0]-s*forcing_base[1],24),
                                 box(s*forcing_base[0]+c*forcing_base[1],24)])
    inputs = [box(v[0]/4,24) for v in forcing_carriers]
    widths = [D('0.0002')]+[D('0.000002')]*3
    radial = Interval(-widths[1],widths[1])
    cosine,sine = [v+Interval(-widths[0],widths[0]) for v in profile['base']]
    p,dp = profile['ranges'][:2]
    raw_jac = [[dp[0]*cosine-(p[0]+radial)*sine,cosine,I(0),I(0)],
               [dp[0]*sine+(p[0]+radial)*cosine,sine,I(0),I(0)],
               [dp[1],I(0),I(1),I(0)], [dp[2],I(0),I(0),I(1)]]
    jac = [[box(v,22) for v in row] for row in mm(inverse,raw_jac)]
    p0 = profile['pointRanges'][0]
    raw_center = [p0[0]*profile['base'][0],p0[0]*profile['base'][1],p0[1],p0[2]]
    center = [box(v[0],24) for v in mm(inverse,[[x] for x in raw_center])]
    def full_value(value,row):
        radius = sum((I(v.magnitude())*w for v,w in zip(row,widths)),I(0)).hi
        return box(value+Interval(-radius,radius),24)
    whole = [full_value(v,row) for v,row in zip(center,jac)]
    centers,wholes,matrices = [center],[whole],[jac]
    center_saturations,saturations = [],[]
    for n in range(162):
        u = inputs[n%54]
        center_rows = [box(a*(u-k*center[3])+b*center[0],24)]
        rows = [[box(b*jac[0][j]-a*k*jac[3][j],22) for j in range(4)]]
        states = [full_value(center_rows[0],rows[0])]
        cs,ss = [proposed_tanh(g*center_rows[0])],[proposed_tanh(g*states[0])]
        for stage in range(1,4):
            center_rows.append(box(a*cs[-1]*inv+b*center[stage],24))
            slope = g*(1-ss[-1]*ss[-1])*inv
            rows.append([box(b*jac[stage][j]+a*slope*rows[-1][j],22) for j in range(4)])
            states.append(full_value(center_rows[-1],rows[-1]))
            cs.append(proposed_tanh(g*center_rows[-1]))
            ss.append(proposed_tanh(g*states[-1]))
        center,jac,whole = center_rows,rows,states
        centers.append(center); matrices.append(jac); wholes.append(whole)
        center_saturations.append(cs); saturations.append(ss)
    return dict(inverse=inverse, forcing_base=forcing_base, forcing_carriers=forcing_carriers,
                inputs=inputs, centers=centers, wholes=wholes, matrices=matrices,
                center_saturations=center_saturations, saturations=saturations)


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root/'formal/DSP/Filters/NaiveReuppingLadder/HarmonicBalance/Certificates'
    data = propose(root)
    inverse, forcing_base, forcing_carriers, inputs, centers, wholes, matrices, center_saturations, saturations = (
        data[key] for key in ['inverse', 'forcing_base', 'forcing_carriers', 'inputs', 'centers',
                             'wholes', 'matrices', 'center_saturations', 'saturations'])
    lines = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCellData',
             'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedParameters',
             'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.CenteredEnclosure',
             '/-! Generated proposal; dynamics bounds require kernel replay. -/',
             'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData',
             'open LeanCert.Core Enclosure DriftProfileCellData',
             'set_option maxRecDepth 100000', 'set_option maxHeartbeats 0',
             'def width : Fin 4 → ℚ := ![1/5000,1/500000,1/500000,1/500000]',
             'def inverseBox : MatrixBox := '+matrix(inverse),
             'def forcingBase : TrigPolynomial.Carrier := '+vector(forcing_base)]
    def array(name, values, render, typename, fallback, modulo=''):
        lines.extend([f'def {name}Data : Array {typename} := #[\n'+',\n'.join(map(render,values))+'\n]',
                      f'def {name} (n : ℕ) : {typename} := {name}Data[n{modulo}]?.getD {fallback}'])
    array('forcingCarrier',forcing_carriers,vector,'TrigPolynomial.Carrier','(fun _ => IntervalRat.singleton 0)')
    array('input',inputs,interval,'IntervalRat','(IntervalRat.singleton 0)','%54')
    for name,values in [('center',centers),('whole',wholes),('centerSaturation',center_saturations),('saturation',saturations)]:
        array(name,values,vector,'Box','(fun _ => IntervalRat.singleton 0)')
    array('matrices',matrices,matrix,'MatrixBox','(fun _ _ => IntervalRat.singleton 0)')
    lines.append('end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData')
    (directory/'DriftBlockCellData.lean').write_text('\n'.join(lines)+'\n')
    for chunk in range(17):
        name=f'DriftBlockCheck{chunk:02}'
        count=min(10,162-10*chunk)
        source=['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData',
                f'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}',
                'open Enclosure DriftBlockCellData', 'set_option maxRecDepth 100000', 'set_option maxHeartbeats 0']
        def proposition(n,next_n):
            return (f'centeredCheck LockedData.coefficients (DriftBlockCellData.input {n}) '
                    f'(center {n}) (center {next_n}) (centerSaturation {n}) (whole {next_n}) '
                    f'(DriftBlockCellData.saturation {n}) (matrices {n}) (matrices {next_n}) width 24 = true')
        for offset in range(count):
            n=10*chunk+offset
            source.append(f'private theorem step_{offset:02} : '+proposition(n,n+1)+' := by decide +kernel')
        source += [f'theorem steps : ∀ j : Fin {count}, let n := {10*chunk}+j.val; '+proposition('n','(n+1)')+' := by',
                   '  intro j', '  fin_cases j']
        source += [f'  · exact step_{j:02}' for j in range(count)]
        source.append(f'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}')
        (directory/(name+'.lean')).write_text('\n'.join(source)+'\n')
    source=[f'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck{chunk:02}' for chunk in range(17)]
    source += ['namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockChecks',
               'open Enclosure DriftBlockCellData',
               'theorem steps : ∀ n < 162, '+proposition('n','(n+1)')+' := by',
               '  intro n hn', '  interval_cases n']
    source += [f'  · exact DriftBlockCheck{n//10:02}.steps ⟨{n%10},by decide⟩' for n in range(162)]
    source.append('end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockChecks')
    (directory/'DriftBlockChecks.lean').write_text('\n'.join(source)+'\n')
    print('Generated one centered 162-sample proposal, requiring Lean replay.')
    print('Final state widths:',[str(v.hi-v.lo) for v in wholes[-1]])
    print('Center widths:',[str(v.hi-v.lo) for v in centers[-1]])


if __name__ == '__main__':
    main()
