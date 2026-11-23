#!/usr/bin/env python3
"""Propose bounded-size rational witnesses for Lean's enclosure replay.

The producer is untrusted. This file does not assert that the whole fixture is
Lean-certified: the emitted data must pass the semantic replay and final bounds.
Use the original Decimal checker for the independent Euclidean-norm experiment.
"""

import json
import argparse
from decimal import Decimal as D, ROUND_FLOOR, ROUND_CEILING
from fractions import Fraction
from pathlib import Path

import numpy as np

from check_ladder_periodic_enclosure import Interval, I, atan_reciprocal, sine_cosine, tanh, exact_inverse, rational_inverse, mm


def box(value, digits):
    quantum = D(1).scaleb(-digits)
    return Interval(value.lo.quantize(quantum, rounding=ROUND_FLOOR),
                    value.hi.quantize(quantum, rounding=ROUND_CEILING))


def record(value):
    if isinstance(value, Interval):
        return value.record()
    if isinstance(value, dict):
        return {k: record(v) for k, v in value.items()}
    if isinstance(value, list):
        return list(map(record, value))
    return value


def propose():
    root = Path(__file__).resolve().parents[1]
    old = json.loads((root/'build/results/ladder-attraction/periodic-enclosure.json').read_text())
    diagnostics = json.loads((root/'build/results/ladder-attraction/diagnostics.json').read_text())
    fixture = next(r for r in diagnostics if r['model'] == 'tanh' and r['drive'] == 1)
    eigenvalues, vectors = np.linalg.eig(np.array(fixture['periodic']['jacobian']))
    real = [j for j, v in enumerate(eigenvalues) if abs(v.imag) < 1e-12]
    complex_index = next(j for j, v in enumerate(eigenvalues) if v.imag > 0)
    basis = np.c_[vectors[:, real].real, vectors[:, complex_index].real, vectors[:, complex_index].imag]
    transform_decimal = [[D(f'{x:.12f}') for x in row] for row in np.linalg.inv(basis)]
    transform = [[I(x) for x in row] for row in transform_decimal]
    inverse = exact_inverse(transform_decimal)
    inverse_box = [[box(v, 22) for v in row] for row in inverse]
    center = [D(x).quantize(D('1e-22')) for x in old['coordinateCenter']]
    pi = 16*atan_reciprocal(5)-4*atan_reciprocal(239)
    cutoff_sine = sine_cosine(pi/192, True)
    alpha = 2*cutoff_sine*((1+cutoff_sine.square()).sqrt()-cutoff_sine)
    coefficients = {'alpha': box(alpha, 20), 'beta': box(1-alpha, 20),
                    'feedback': box(I(39)/10*(7*I(7).sqrt()-1)/48, 20),
                    'gain': I(5), 'inverseNormalization': box(1/tanh(I(5)), 20)}
    delta = 2*pi*3/170
    cos_delta, sin_delta = box(sine_cosine(delta, False), 20), box(sine_cosine(delta, True), 20)
    cosine, sine = I(1), I(0)
    forcing, carriers = [], []
    for _ in range(170):
        carriers.append([cosine, sine])
        forcing.append(box(cosine/4, 24))
        cosine, sine = box(cosine*cos_delta-sine*sin_delta, 24), box(sine*cos_delta+cosine*sin_delta, 24)
    def initial(radius):
        widths = [sum((I(v.magnitude()) for v in row), I(0))*radius for row in inverse_box]
        return [box(I(x)+Interval(-w.hi, w.hi), 24) for x, w in zip(center, widths)]
    def trace(states, matrix):
        rows, outputs = [], []
        a, b, k, g, inv = [coefficients[key] for key in ['alpha', 'beta', 'feedback', 'gain', 'inverseNormalization']]
        for u in forcing:
            next_state = [box(a*(u-k*states[3])+b*states[0], 24)]
            # Leave room for the verified Taylor enclosure as well as the
            # producer's much narrower 70-digit exponential enclosure.
            saturation = [box(tanh(g*next_state[0])+Interval(D('-1e-20'), D('1e-20')), 22)]
            for j in range(1, 4):
                next_state.append(box(a*(saturation[j-1]*inv)+b*states[j], 24))
                saturation.append(box(tanh(g*next_state[j])+Interval(D('-1e-20'), D('1e-20')), 22))
            if matrix is not None:
                next_matrix = [[box(b*matrix[0][j]-a*k*matrix[3][j], 20) for j in range(4)]]
                for i in range(1, 4):
                    slope = g*(1-saturation[i-1]*saturation[i-1])*inv
                    next_matrix.append([box(b*matrix[i][j]+a*slope*next_matrix[i-1][j], 20) for j in range(4)])
                matrix = next_matrix
            outputs.append(box(saturation[3]*inv*(k+1), 22))
            rows.append({'state': next_state, 'saturation': saturation, 'matrix': matrix})
            states = next_state
        return states, matrix, rows, outputs
    radius = D('0.0001')
    whole_initial = initial(radius)
    _, matrix, whole, _ = trace(whole_initial, inverse_box)
    transformed = mm(transform, matrix)
    contraction = max(sum((I(v.magnitude()) for v in row), I(0)).hi for row in transformed)
    assert contraction < D('0.9'), contraction
    final, _, point, _ = trace(initial(D(0)), None)
    residual_box = mm(transform, [[v-I(x)] for v, x in zip(final, center)])
    residual = max(v[0].magnitude() for v in residual_box)
    distance = (I(residual)/(1-I(contraction))).hi
    small_radius = D('1e-8')
    assert distance < small_radius and residual+contraction*radius < radius, (residual, contraction)
    _, _, small, outputs = trace(initial(small_radius), None)
    coherent_real = 2*sum((y*c[0] for y, c in zip(outputs, carriers)), I(0))/170
    coherent_imag = -2*sum((y*c[1] for y, c in zip(outputs, carriers)), I(0))/170
    powers = {'coherentPower': (coherent_real.square()+coherent_imag.square())/2,
              'totalPower': sum((y.square() for y in outputs), I(0))/170}
    result = {'status': 'untrusted witness proposal; requires Lean replay',
              'radius': str(radius), 'smallRadius': str(small_radius),
              'center': list(map(str, center)), 'transform': transform, 'inverseBox': inverse_box,
              'coefficients': coefficients, 'cosDelta': cos_delta, 'sinDelta': sin_delta,
              'forcing': forcing, 'carriers': carriers, 'wholeInitial': whole_initial,
              'pointInitial': initial(D(0)), 'smallInitial': initial(small_radius),
              'whole': whole, 'point': point, 'small': small,
              'contraction': str(contraction), 'residual': str(residual),
              'distanceBound': str(distance), 'powers': powers}
    out = root/'build/results/ladder-locked-error/witness.json'
    out.write_text(json.dumps(record(result), indent=2)+'\n')
    print(json.dumps(record({k: result[k] for k in ['status', 'contraction', 'residual', 'distanceBound', 'powers']}), indent=2))
    return record(result)


def emit_lean(data, root):
    def rational(value):
        fraction = Fraction(value)
        return str(fraction.numerator) if fraction.denominator == 1 else f'({fraction.numerator}/{fraction.denominator})'
    def interval(value):
        width = Fraction(value[1])-Fraction(value[0])
        assert width >= 0
        return f'interval {rational(value[0])} {width.numerator} {width.denominator}'
    def vector(values):
        return '!['+', '.join(interval(v) for v in values)+']'
    def matrix(values):
        return '!['+', '.join(vector(v) for v in values)+']'
    def rational_vector(values):
        return '!['+', '.join(rational(v) for v in values)+']'
    def rational_matrix(values):
        return '!['+', '.join(rational_vector(v) for v in values)+']'
    transform = [[Fraction(v[0]) for v in row] for row in data['transform']]
    inverse = rational_inverse(transform)
    for i in range(4):
        for j in range(4):
            assert sum(inverse[i][k]*transform[k][j] for k in range(4)) == (i == j)
            assert Fraction(data['inverseBox'][i][j][0]) <= inverse[i][j] <= Fraction(data['inverseBox'][i][j][1])
    lines = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.EnclosureTrace',
             '/-! Generated by scripts/generate_ladder_interval_witness.py. Definitions alone assert no bounds. -/',
             'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData',
             'open LeanCert.Core Enclosure', 'set_option maxRecDepth 100000', 'set_option maxHeartbeats 0',
             '/-- Encoding a nonnegative rational width makes interval validity structural. -/',
             'def interval (lo : ℚ) (widthNumerator widthDenominator : ℕ) : IntervalRat :=',
             '  ⟨lo, lo+widthNumerator/widthDenominator, le_add_of_nonneg_right',
             '    (div_nonneg (Nat.cast_nonneg _) (Nat.cast_nonneg _))⟩',
             'def coefficients : CoefficientBox := ⟨'+', '.join(interval(data['coefficients'][k]) for k in
                 ['alpha', 'beta', 'feedback', 'gain', 'inverseNormalization'])+'⟩']
    lines += ['def centerRat : Fin 4 → ℚ := '+rational_vector(data['center']),
              'def transformRat : Matrix (Fin 4) (Fin 4) ℚ := '+rational_matrix(transform),
              'def inverseRat : Matrix (Fin 4) (Fin 4) ℚ := '+rational_matrix(inverse),
              'def cosDelta : IntervalRat := '+interval(data['cosDelta']),
              'def sinDelta : IntervalRat := '+interval(data['sinDelta']),
              'def carrierData : Array (Fin 2 → IntervalRat) := #[\n'+',\n'.join(vector(v) for v in data['carriers'])+'\n]',
              'def carrier (n : ℕ) : Fin 2 → IntervalRat := carrierData[n]?.getD (fun _ => IntervalRat.singleton 0)']
    lines += ['def inputData : Array IntervalRat := #[\n'+',\n'.join(interval(v) for v in data['forcing'])+'\n]',
              'def input (n : ℕ) : IntervalRat := inputData[n]?.getD (IntervalRat.singleton 0)']
    for key in ['whole', 'point', 'small']:
        states = [data[key+'Initial']]+[v['state'] for v in data[key]]
        lines += [f'def {key}HistoryData : Array Box := #[\n'+',\n'.join(map(vector, states))+'\n]',
                  f'def {key}History (n : ℕ) : Box := {key}HistoryData[n]?.getD (fun _ => IntervalRat.singleton 0)',
                  f'def {key}SaturationData : Array Box := #[\n'+',\n'.join(vector(v['saturation']) for v in data[key])+'\n]',
                  f'def {key}Saturation (n : ℕ) : Box := {key}SaturationData[n]?.getD (fun _ => IntervalRat.singleton 0)']
    matrices = [data['inverseBox']]+[v['matrix'] for v in data['whole']]
    lines += ['def matrixData : Array MatrixBox := #[\n'+',\n'.join(map(matrix, matrices))+'\n]',
              'def matrices (n : ℕ) : MatrixBox := matrixData[n]?.getD (fun _ _ => IntervalRat.singleton 0)',
              'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData']
    directory = root/'formal/DSP/Filters/NaiveReuppingLadder/HarmonicBalance/Certificates'
    directory.mkdir(exist_ok=True)
    (directory/'LockedData.lean').write_text('\n'.join(lines)+'\n')
    diagnostic = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData',
                  'open DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure', 'open DSP.NaiveReuppingLadder.HarmonicBalance.Certificates',
                  'open DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData',
                  'set_option maxRecDepth 100000', 'set_option maxHeartbeats 0']
    for key in ['whole', 'point', 'small']:
        diagnostic.append(f'#eval (List.range 170).filter (fun n => !(valuesCheck coefficients (LockedData.input n) '
                          f'({key}History n) ({key}History (n+1)) ({key}Saturation n) 24))')
    diagnostic.append('#eval (List.range 170).filter (fun n => !(matrixCheck coefficients '
                      '(matrices n) (matrices (n+1)) (wholeSaturation n)))')
    (root/'build/results/ladder-locked-error/probes/FullEvaluate.lean').write_text('\n'.join(diagnostic)+'\n')
    # Separate small proof modules bound peak memory and allow ordinary Lake
    # build parallelism. Every numerical proposition uses kernel reduction.
    propositions = {
        'wholeValues': 'valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true',
        'pointValues': 'valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true',
        'smallValues': 'valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true',
        'wholeMatrices': 'matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true',
    }
    for chunk in range(17):
        name = f'LockedCheck{chunk:02}'
        source = ['import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData',
                  '/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/',
                  f'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}',
                  'open Enclosure LockedData', 'set_option maxRecDepth 100000', 'set_option maxHeartbeats 0']
        for theorem, proposition in propositions.items():
            for offset in range(10):
                concrete = proposition.replace('(LockedData.input n)', f'(LockedData.input {10*chunk+offset})')
                concrete = concrete.replace('(n+1)', str(10*chunk+offset+1)).replace(' n)', f' {10*chunk+offset})')
                source += [f'private theorem {theorem}_{offset:02} : {concrete} := by decide +kernel']
            source += [f'theorem {theorem} : ∀ j : Fin 10,',
                       f'    let n := {10*chunk}+j.val; {proposition} := by', '  intro j', '  fin_cases j']
            source += [f'  · exact {theorem}_{offset:02}' for offset in range(10)]
        source += [f'end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.{name}']
        (directory/(name+'.lean')).write_text('\n'.join(source)+'\n')
    source = [f'import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck{chunk:02}' for chunk in range(17)]
    source += ['/-! Collect the checked chunks into whole-period certificates. -/',
               'namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedChecks',
               'open Enclosure LockedData',
               'private theorem from_chunks (P : ℕ → Prop) (h : ∀ i : Fin 17, ∀ j : Fin 10, P (10*i.val+j.val))',
               '    (n : ℕ) (hn : n < 170) : P n := by',
               '  have hi : n/10 < 17 := by omega', '  have hj : n%10 < 10 := Nat.mod_lt _ (by norm_num)',
               '  have he : 10*(n/10)+n%10 = n := by omega',
               '  simpa only [he] using h ⟨n/10,hi⟩ ⟨n%10,hj⟩']
    for theorem, proposition in propositions.items():
        source += [f'theorem {theorem} : ∀ n < 170, {proposition} := by',
                   '  apply from_chunks', '  intro i', '  fin_cases i']
        source += [f'  · exact LockedCheck{chunk:02}.{theorem}' for chunk in range(17)]
    source += ['end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedChecks']
    (directory/'LockedChecks.lean').write_text('\n'.join(source)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emit-only', action='store_true', help='reuse the existing witness JSON')
    args = parser.parse_args()
    root_dir = Path(__file__).resolve().parents[1]
    witness = json.loads((root_dir/'build/results/ladder-locked-error/witness.json').read_text()) if args.emit_only else propose()
    emit_lean(witness, root_dir)
