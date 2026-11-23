#!/usr/bin/env python3
"""Propose output-chart witnesses; every interval requires Lean replay."""

import json
import math
from decimal import Decimal as D, ROUND_CEILING
from fractions import Fraction
from pathlib import Path

from check_ladder_periodic_enclosure import I, Interval, mm
from generate_ladder_interval_witness import box
from generate_ladder_drift_witness import literal
from generate_ladder_drift_profile_cell import propose as profile_proposal, interval, vector
from generate_ladder_drift_block_cell import propose as block_proposal


def propose(root):
    source = json.loads((root/'build/results/ladder-drift-certificate/averages.json').read_text(), parse_float=Fraction)
    transform = [[I(v) for v in row] for row in source['chart']['transform']]
    block = block_proposal(root)
    states = [[box(v[0],24) for v in mm(transform,[[x] for x in block[k][-1]])] for k in ['centers','wholes']]
    anchor = D(str(math.atan2(float(states[0][1].mid()),float(states[0][0].mid()))))
    prepared = profile_proposal(root,anchor,0)
    cosine,sine = prepared['base']
    radii, roots, inverse_roots, inverse_squares, profiles = [], [], [], [], []
    for values in states:
        alignment = values[0]*cosine+values[1]*sine
        cross = values[1]*cosine-values[0]*sine
        assert alignment.lo > 0
        radius = (cross.magnitude()/alignment.lo).quantize(D('1e-22'),rounding=ROUND_CEILING)
        radii.append(radius)
        norm_square = values[0]*values[0]+values[1]*values[1]
        root_box = box(norm_square.sqrt(),22)
        roots.append(root_box)
        inverse_roots.append(box(1/root_box,22))
        inverse_squares.append(box(1/norm_square,22))
        profiles.append(profile_proposal(root,anchor,radius,22)['ranges'])
    return dict(anchor=anchor, prepared=prepared, states=states, radii=radii, roots=roots,
                inverse_roots=inverse_roots, inverse_squares=inverse_squares, profiles=profiles)


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root/'formal/DSP/Filters/NaiveReuppingLadder/HarmonicBalance/Certificates'
    data = propose(root)
    anchor, prepared, states, radii, roots, inverse_roots, inverse_squares, profiles = (
        data[key] for key in ['anchor','prepared','states','radii','roots','inverse_roots','inverse_squares','profiles'])
    lines = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockGeometry',
             'import DSP.Analysis.PolarTube.PhaseEnclosure',
             'import DSP.Analysis.PolarTube.EncodeEnclosure',
             '/-! Generated output-chart proposals; checked by DriftOutputCell. -/',
             'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCellData',
             'open LeanCert.Core PolarTube Enclosure DriftProfileCellData',
             'set_option maxRecDepth 100000',
             f'def anchor : ℚ := {literal(anchor)}',
             'def base : TrigPolynomial.Carrier := '+vector(prepared['base']),
             'def carrierData : Array TrigPolynomial.Carrier := #[\n'+',\n'.join(map(vector,prepared['carriers']))+'\n]',
             'def carrier (n : ℕ) : TrigPolynomial.Carrier := carrierData[n]?.getD (fun _ => IntervalRat.singleton 0)',
             'def state : Fin 2 → StateBox := !['+',\n'.join(map(vector,states))+']',
             'def phaseRadius : Fin 2 → ℚ := '+literal(radii),
             'def angle (k : Fin 2) : IntervalRat := interval (anchor-phaseRadius k) (2*(phaseRadius k).num.natAbs) (phaseRadius k).den',
             'def radius : Fin 2 → IntervalRat := '+vector(roots),
             'def inverseRadius : Fin 2 → IntervalRat := '+vector(inverse_roots),
             'def inverseSquare : Fin 2 → IntervalRat := '+vector(inverse_squares),
             'def ranges : Fin 2 → Fin 3 → Fin 3 → IntervalRat := !['+
               ',\n'.join('!['+',\n'.join(map(vector,p))+']' for p in profiles)+']',
             'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCellData']
    (directory/'DriftOutputCellData.lean').write_text('\n'.join(lines)+'\n')
    print('Proposed output anchor:',anchor)
    print('Center and whole-cell phase radii:',[str(v) for v in radii])
    print('Lean replay is required; no attraction assertion.')


if __name__ == '__main__':
    main()
