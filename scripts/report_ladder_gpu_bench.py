#!/usr/bin/env python3
"""Compare bounded GPU benchmark measurements; differences are not proof errors."""

import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def read(path):
    with path.open() as stream:
        return [{name: float(value) for name, value in row.items()} for row in csv.DictReader(stream)]


def quantiles(values):
    ordered = sorted(values)
    result = []
    for q in (.5, .95, .99, 1):
        position = q*(len(ordered)-1)
        lower = int(position)
        upper = min(lower+1, len(ordered)-1)
        result.append(ordered[lower]+(position-lower)*(ordered[upper]-ordered[lower]))
    return result


def differences(a, b):
    difference = [abs(x-y) for x, y in zip(a,b)]
    relative = [abs(x-y)/max(abs(x),abs(y)) for x,y in zip(a,b) if max(abs(x),abs(y)) > .0003125]
    return dict(
        absolute_input_power_units_quantiles=quantiles([value/.03125 for value in difference]),
        symmetric_relative_quantiles_above_minus20db_gain=quantiles(relative),
        material_points=len(relative),
        fraction_material_within_1pct=sum(value < .01 for value in relative)/len(relative),
        fraction_material_within_5pct=sum(value < .05 for value in relative)/len(relative),
    )


def report(folder):
    results = {}
    for case in sorted(folder.glob('f*')):
        gpu = read(case/'gpu-trajectories.csv')
        cpu = read(case/'cpu-float-trajectories.csv')
        for key in ('point_id', 'start'):
            assert [row[key] for row in gpu] == [row[key] for row in cpu]
        assert all(math.isfinite(value) for row in gpu+cpu for value in row.values())
        groups = len(gpu)//10
        ids = [int(gpu[10*i]['point_id']) for i in range(groups)]
        assert all(gpu[10*i+j]['point_id'] == ids[i] for i in range(groups) for j in range(10))
        assert len(set(ids)) == groups
        item = dict(complete_10_start_points=groups, gpu_vs_cpu_float={})
        means = {}
        for key in ('total', 'fundamental'):
            a = [sum(row[key] for row in gpu[10*i:10*i+10])/10 for i in range(groups)]
            b = [sum(row[key] for row in cpu[10*i:10*i+10])/10 for i in range(groups)]
            means[key] = a
            item['gpu_vs_cpu_float'][key] = differences(a, b)
        reference = read(case/'double-real-tanh-points.csv')
        lookup = {point: i for i, point in enumerate(ids)}
        item['gpu_rational_vs_double_real_tanh'] = {
            key: differences([means[key][lookup[int(row['point_id'])]] for row in reference], [row[key] for row in reference])
            for key in ('total', 'fundamental')
        }
        timing = json.loads((case/'gpu-timing.json').read_text())
        cpu_timing = json.loads((case/'cpu-timing.json').read_text())
        rates = [row['sample_updates_per_second'] for row in timing]
        item['timing'] = dict(gpu_rate_min=min(rates), gpu_rate_median=statistics.median(rates),
            gpu_rate_max=max(rates), cpu_float_rate=cpu_timing['sample_updates_per_second'],
            median_gpu_over_cpu_float=statistics.median(rates)/cpu_timing['sample_updates_per_second'])
        results[case.name] = item
    return dict(quantile_order=[.5, .95, .99, 1], cases=results,
        interpretation='CPU/GPU float comparisons isolate arithmetic differences on the same model. Rational versus real-tanh intentionally changes the model, normalization, feedback cap and initial-state bounds. Ten starts are a diagnostic, not an equivalence proof.',
        timing_excludes='One-time compilation and buffer allocation, coefficient/input preparation, CSV output and full-survey checkpoint integration.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = report(args.folder)
    (args.folder/'comparison.json').write_text(json.dumps(result, indent=2)+'\n')
    for name, case in result['cases'].items():
        print(name, json.dumps(case))


if __name__ == '__main__':
    main()
