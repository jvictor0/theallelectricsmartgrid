#!/usr/bin/env python3
"""Propose rounded output jets; Lean checks every containment and inequality."""

import json
from decimal import Decimal as D
from fractions import Fraction
from pathlib import Path

from check_ladder_periodic_enclosure import I, Interval, mm
from generate_ladder_interval_witness import box
from generate_ladder_drift_block_cell import propose as block_proposal, matrix
from generate_ladder_drift_output_cell import propose as output_proposal
from generate_ladder_drift_second_cell import propose as second_proposal, tensor
from generate_ladder_drift_witness import literal


def phase_first(z, v, inv_square):
    return (z[0]*v[1]-z[1]*v[0])*inv_square


def radius_first(z, v, inv_radius):
    return (z[0]*v[0]+z[1]*v[1])*inv_radius


def first(dp, z, v, inv_square, inv_radius):
    phase = phase_first(z, v, inv_square)
    return [phase, radius_first(z, v, inv_radius)-dp[0]*phase,
            v[2]-dp[1]*phase, v[3]-dp[2]*phase]


def mixed(dp, ddp, z, l, r, m, inv_square, inv_radius):
    cross = l[0]*r[1]+l[1]*r[0]
    phase_second = (2*z[0]*z[1]*(l[0]*r[0])+(z[1]*z[1]-z[0]*z[0])*cross-
                    2*z[0]*z[1]*(l[1]*r[1]))*(inv_square*inv_square)
    radius_second = (z[1]*z[1]*(l[0]*r[0])-z[0]*z[1]*cross+
                     z[0]*z[0]*(l[1]*r[1]))*((inv_radius*inv_radius)*inv_radius)
    lp, rp = phase_first(z, l, inv_square), phase_first(z, r, inv_square)
    mp = phase_first(z, m, inv_square)+phase_second
    mr = radius_first(z, m, inv_radius)+radius_second
    correction = [(v*lp)*rp+d*mp for v, d in zip(ddp, dp)]
    return [mp, mr-correction[0], m[2]-correction[1], m[3]-correction[2]]


def propose(root):
    block, second, output = block_proposal(root), second_proposal(root), output_proposal(root)
    source = json.loads((root/'build/results/ladder-drift-certificate/averages.json').read_text(), parse_float=Fraction)
    transform = [[I(v) for v in row] for row in source['chart']['transform']]
    def transformed(v):
        return [row[0] for row in mm(transform, [[v[i]] for i in range(4)])]
    center_j, whole_j, whole_h = second['centers'][-1], block['matrices'][-1], second['hessians'][-1]
    gradient = [first(output['profiles'][0][1], output['states'][0],
                      transformed([center_j[i][j] for i in range(4)]),
                      output['inverse_squares'][0], output['inverse_roots'][0]) for j in range(4)]
    hessian = [[mixed(output['profiles'][1][1], output['profiles'][1][2], output['states'][1],
                      transformed([whole_j[i][j] for i in range(4)]),
                      transformed([whole_j[i][k] for i in range(4)]),
                      transformed([whole_h[i][j][k] for i in range(4)]),
                      output['inverse_squares'][1], output['inverse_roots'][1])
                for k in range(4)] for j in range(4)]
    # Use output-coordinate first indexing, matching a Jacobian/Hessian.
    gradient = [[box(gradient[j][i], 12) for j in range(4)] for i in range(4)]
    hessian = [[[box(hessian[j][k][i], 8) for k in range(4)] for j in range(4)] for i in range(4)]
    width = [D('0.0002')]+[D('0.000002')]*3
    whole = [[box(gradient[i][j]+Interval(-sum(h.magnitude()*w for h,w in zip(hessian[i][j],width)),
                                          sum(h.magnitude()*w for h,w in zip(hessian[i][j],width))), 10)
              for j in range(4)] for i in range(4)]
    return gradient, hessian, whole, width


def main():
    root = Path(__file__).resolve().parents[1]
    gradient, hessian, whole, width = propose(root)
    source = [
        'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondOutput',
        'import DSP.Analysis.IntervalTaylor',
        '/-! Rounded output-jet proposals, checked in DriftOutputBounds. -/',
        'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBoundsData',
        'open LeanCert.Core PolarTube DriftProfileCellData',
        'set_option maxRecDepth 100000',
        'def gradient : Fin 4 → StateBox := '+matrix(gradient),
        'def hessian : HessianBox := '+tensor(hessian),
        'def wholeGradient : Fin 4 → StateBox := '+matrix(whole),
        'def width : Fin 4 → ℚ := '+literal(width),
        'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBoundsData']
    directory = root/'formal/DSP/Filters/NaiveReuppingLadder/HarmonicBalance/Certificates'
    (directory/'DriftOutputBoundsData.lean').write_text('\n'.join(source)+'\n')
    print('Proposed whole-cell Jacobian:')
    for row in whole:
        print([v.record() for v in row])
    print('First-order transverse image radii, plus 1e-8 center defect:')
    print([str(sum(v.magnitude()*w for v,w in zip(row,width))+D('1e-8')) for row in whole[1:]])
    print('Lean replay and derivative-to-cell theorem required.')


if __name__ == '__main__':
    main()
