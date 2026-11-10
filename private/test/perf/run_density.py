#!/usr/bin/env python3
"""Serial old-sample-grid / prior-phase-grid / compact-inverse comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import random
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
BINARY = ROOT / 'build/tools/perf/Release/perf_density'
SETTINGS = ['linear', 'gentle', 'blend', 'default_full_mult', 'slow_reversal',
            'bad_fours', 'hard_fours', 'hard_fives', 'hard_fives_reverse_skew',
            'hard_twos', 'hard_blend', 'global_full_mult', 'global_mid_mult',
            'hard_threes', 'global_full_mult_reverse_skew',
            'global_mid_mult_reverse_skew', 'hard_twos_reverse_skew',
            'limited_gentle', 'limited_reversing', 'limited_fast',
            'limited_extreme', 'limited_cluster']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['correctness', 'performance'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=5)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    combined = out / (args.mode + '-all.json')
    if combined.exists():
        parser.error(f'refusing to replace existing results: {combined}')
    cases = [(i, b) for i in range(len(SETTINGS)) for b in [16, 18, 20, 24]]
    cases += [(i, b) for i in [0, 1, 2, 3, 5, 7, 11, 14]
              for b in [16.1, 17.3, 19.7, 23.9]]
    random.Random(20261109).shuffle(cases)
    sources = list((ROOT / 'private/src').glob('*.hpp'))
    sources += [ROOT / 'private/test/perf' / name for name in [
        'density.cpp', 'run_density.py', 'reference/LegacyTimeWarpInverter.hpp',
        'reference/PhaseGridTimeWarpInverter.hpp']]
    sources += list((ROOT / 'formal/PhaseTime').glob('*.lean'))
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sources}
    metadata = {'platform': platform.platform(), 'baseline_commit': 'a8a6ca1',
                'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
                                               text=True).strip(),
                'binary_sha256': hashlib.sha256(BINARY.read_bytes()).hexdigest(),
                'sources': hashes, 'build': json.loads((BINARY.parent / 'build.json').read_text()),
                'mode': args.mode, 'cases': cases, 'settings': SETTINGS,
                'repeats': args.repeats, 'cycles': 3.25,
                'oracle': 'latest closed forward crossing; stationary observations do not advance endpoints'}
    (out / (args.mode + '-metadata.json')).write_text(json.dumps(metadata, indent=2) + '\n')
    (out / (args.mode + '-source-diff.patch')).write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=ROOT))
    for source in sources:
        target = out / (args.mode + '-sources') / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
    all_rows = []
    for index, (setting, bits) in enumerate(cases):
        name = f'{SETTINGS[setting]}-period{bits}'
        path = out / f'{args.mode}-{name}.jsonl'
        command = [str(BINARY), args.mode, str(setting), str(bits), str(args.repeats), '3.25']
        started = time.monotonic()
        with path.open('w') as stdout, path.with_suffix('.err').open('w') as stderr:
            result = subprocess.run(command, stdout=stdout, stderr=stderr, cwd=ROOT)
        if result.returncode:
            raise RuntimeError(f'{name}: {path.with_suffix(".err").read_text()}')
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        assert all(r['case'] == f'{SETTINGS[setting]}/period2^{bits}' for r in rows)
        if args.mode == 'correctness':
            by_kind = {r['kind']: r for r in rows}
            assert by_kind['storage']['payload_bytes'] == 32 * 2**20
            for kind in ['delay_raw_host_frames', 'delay_safe_host_frames',
                         'delay_steady_host_frames', 'scope_time_host_frames']:
                row = by_kind[kind]
                if bits == 18:
                    prior = by_kind['prior_' + kind]
                    for key in ['old_p99', 'old_max', 'missing_old', 'spurious_old']:
                        assert row[key] == prior[key], (name, key, row, prior)
                if setting == 0:
                    assert row['old_max'] < 1e-5 and row['new_max'] < 1e-5, (name, row)
        all_rows.extend(rows)
        combined.write_text(json.dumps(all_rows, indent=2) + '\n')
        print(json.dumps({'mode': args.mode, 'completed': index + 1, 'total': len(cases),
                          'case': name, 'elapsed_wall_s': round(time.monotonic() - started, 2)}), flush=True)
    print(json.dumps({'mode': args.mode, 'status': 'complete'}), flush=True)


if __name__ == '__main__':
    main()
