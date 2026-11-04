#!/usr/bin/env python3
"""Run repeatable component and encoder workloads serially; retain raw results."""
import argparse
import copy
import datetime
import fnmatch
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FIXTURE = HERE / "fixtures/modulation-stress.json"
SAVED_FIXTURE = HERE.parent / "fixtures_espace_etale_2026-06-24T10-40-13.json"


def child_env():
    # Keep profiler children free of inherited credentials and unrelated configuration.
    return {key: os.environ[key] for key in ("PATH", "HOME", "TMPDIR", "LANG", "LC_ALL") if key in os.environ}


def execute(command):
    result = subprocess.run([str(part) for part in command], env=child_env(), text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"Failed: {' '.join(map(str, command))}\n{result.stderr[-4000:]}")
    # SynthRig may log on stdout. Require exactly one JSON result record.
    lines = [line for line in result.stdout.splitlines() if line.startswith('{"kind":')]
    if len(lines) != 1:
        raise RuntimeError(f"Expected one result: {result.stdout[-2000:]}")
    row = json.loads(lines[0])
    for key in ("ms_per_audio_second", "us_per_transform", "cpu_seconds", "checksum", "gesture_checksum"):
        if key in row and (not math.isfinite(row[key]) or (key not in ("checksum", "gesture_checksum") and row[key] < 0)):
            raise RuntimeError(f"Invalid metric {key}: {row[key]}")
    return row


def digest(path):
    with Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else hashlib.sha256(source.read()).hexdigest()


def cases(suite):
    result = []
    def add(executable, kind, lanes, modes):
        for lane in lanes:
            for mode in modes:
                result.append((f"{kind}/{lane}/{mode}", executable, kind, lane, mode))
    if suite in ("components", "all"):
        for kind in ("ladder", "svf", "physical", "vps", "dual"):
            add("voices", kind, (1, 3), range(4))
        for kind in ("reverb", "delay", "partial", "quad_master"):
            add("effects", kind, (4,), range(4))
        add("effects", "stereo_master", (2,), range(4))
        add("effects", "random", (3,), range(4))
        add("effects", "polyxfader", (1, 3, 4), range(4))
        for kind in ("lissajous", "sub_osc", "sub_kernel"):
            add("extra", kind, (1, 3), (1, 3))
        for kind in ("mixer_inputs", "mixer_full"):
            add("extra", kind, (4,), (1, 3))
        add("auxiliary", "ahd", (3,), range(3))
        add("auxiliary", "meter", (1, 3), (0,))
        add("auxiliary", "empty", (3,), (0,))
        for kind in ("ifft", "ifft_upsample"):
            add("auxiliary", kind, (4,), (0,))
    if suite in ("encoders", "all"):
        for topology in ("flat", "nested", "gestures"):
            for mode in ("fixed", "right", "mid", "sweep"):
                result.append((f"matrix/{topology}/{mode}", "encoders", "matrix", topology, mode))
        for mode in ("fixed", "right", "mid", "sweep", "scenes"):
            result.append((f"full/{mode}", "encoders", "full", "flat", mode))
        for name, kind, mode in (("fixed", "full", "fixed"),
                                 ("blend", "full", "sweep"),
                                 ("gestures", "full_gestures", "fixed"),
                                 ("both", "full_gestures", "sweep"),
                                 ("scenes", "full_gestures", "scenes")):
            result.append((f"full/mixed/{name}", "encoders", kind, "mixed", mode))
        for topology, kind in (("flat", "consumer"), ("mixed", "consumer_gestures")):
            for mode in ("fixed", "sweep"):
                result.append((f"consumer/{topology}/{mode}", "encoders", kind, topology, mode))
    if suite in ("saved", "all"):
        for name, kind, mode in (("hold", "full", "hold"),
                                 ("blend", "full", "sweep"),
                                 ("gestures", "full_saved_gestures", "hold"),
                                 ("both", "full_saved_gestures", "sweep")):
            result.append((f"saved/{name}", "encoders", kind, "saved", mode))
    return result


def derive_fixtures(output, source):
    base = json.loads(Path(source).read_text())
    nested = copy.deepcopy(base)
    for root in nested["squiggleBoy"].values():
        for index, amount in enumerate(root.get("modulators", [])):
            if amount:
                values = amount["values"]["values"]
                child = {"values": {"values": [0.65 if v > 0 else -0.65 if v < 0 else 0.0 for v in values]}}
                amount["modulators"] = [child if i == index else None for i in range(15)]
    gesture = copy.deepcopy(nested)
    def attach(node, slot, bipolar):
        values = [0.1 if i % 2 == 0 else 0.8 for i in range(8)]
        if bipolar:
            values = [2 * v - 1 for v in values]
        node["gestures"] = [None] * 16
        node["gestures"][slot] = {"values": {"values": values}, "active": [i % 3 != 1 for i in range(8)]}
    for index, root in enumerate(gesture["squiggleBoy"].values()):
        attach(root, index % 16, False)
        for amount in root.get("modulators", []):
            if amount:
                attach(amount, (index + 3) % 16, True)
    mixed = copy.deepcopy(base)
    mixed["faders"] = [0.5] * 16
    for index, name in enumerate(sorted(mixed["squiggleBoy"])):
        root = mixed["squiggleBoy"][name]
        root.pop("gestures", None)
        if index % 2 == 0:
            root.pop("modulators", None)
            values = root["values"]["values"]
            slot = (index // 2) % 16
            root["gestures"] = [None] * 16
            for offset, rotation, inactive in ((0, 3, 1), (5, 5, 2)):
                root["gestures"][(slot + offset) % 16] = {
                    "values": {"values": [values[(scene + rotation) % 8] for scene in range(8)]},
                    "active": [scene % 3 != inactive for scene in range(8)]}
    paths = {"flat": Path(source).resolve()}
    for name, value in (("nested", nested), ("gestures", gesture), ("mixed", mixed)):
        path = output / f"{name}-patch.json"
        path.write_text(json.dumps(value, separators=(',', ':')) + '\n')
        paths[name] = path
    return paths


def positive(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0 or value > 86400:
        raise argparse.ArgumentTypeError("must be finite and in (0, 86400]")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/tools/perf/Release')
    parser.add_argument('--suite', choices=('components', 'encoders', 'saved', 'all'), default='all')
    parser.add_argument('--case', action='append', help='case glob; repeat to select several')
    parser.add_argument('--seconds', type=positive, default=6)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--patch', type=Path, help='patch to load; saved cases preserve its routing and gestures')
    parser.add_argument('--replay', type=Path, help='reuse exactly the same captured input across revisions')
    parser.add_argument('--smoke', action='store_true', help='all selected cases, once, 0.02 seconds; no speed assertions')
    parser.add_argument('--list', action='store_true')
    args = parser.parse_args()
    selected = [case for case in cases(args.suite)
                if not args.case or any(fnmatch.fnmatchcase(case[0], pattern) for pattern in args.case)]
    if not selected:
        parser.error('no matching cases')
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    if args.list:
        print('\n'.join(case[0] for case in selected))
        return
    seconds, repeats = (0.02, 1) if args.smoke else (args.seconds, args.repeats)
    output = (args.output or ROOT / 'build/results' / datetime.datetime.now().strftime('perf-%Y%m%d-%H%M%S')).resolve()
    output.mkdir(parents=True, exist_ok=True)
    binaries = {case[1]: (args.bin_dir / f'perf_{case[1]}').resolve() for case in selected}
    for binary in binaries.values():
        if not binary.is_file():
            parser.error(f'{binary} is missing; run make perf-build')
    source = args.patch or FIXTURE
    patches = {'flat': source.resolve(), 'saved': (args.patch or SAVED_FIXTURE).resolve()}
    replay = args.replay.resolve() if args.replay else output / 'replay.bin'
    if any(case[3] in ('nested', 'gestures', 'mixed') for case in selected):
        patches.update(derive_fixtures(output, source))
    replay_kinds = ('matrix', 'consumer', 'consumer_gestures')
    if any(case[2] in replay_kinds for case in selected):
        if not args.replay:
            print('Capturing reusable modulation inputs...', flush=True)
            execute([binaries['encoders'], 'record', source, 'sweep', 0.02 if args.smoke else 1, replay])
    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(ROOT), *arguments], text=True).strip()
    metadata = {'schema': 1, 'created': datetime.datetime.now().astimezone().isoformat(),
                'revision': git('rev-parse', 'HEAD'), 'status': git('status', '--short'),
                'tracked_diff_sha256': hashlib.sha256(git('diff', 'HEAD').encode()).hexdigest(),
                'platform': platform.platform(), 'machine': platform.machine(),
                'seconds': seconds, 'repeats': repeats, 'smoke': args.smoke,
                'build': json.loads((args.bin_dir / 'build.json').read_text()),
                'binaries': {name: digest(path) for name, path in binaries.items()},
                'fixtures': {name: digest(path) for name, path in patches.items()}}
    if any(case[2] in replay_kinds for case in selected):
        metadata['replay_sha256'] = digest(replay)
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    jobs = [(case, repeat) for repeat in range(repeats) for case in selected]
    random.Random(1000).shuffle(jobs)
    rows = []
    with (output / 'raw.jsonl').open('w') as log:
        for index, (case, repeat) in enumerate(jobs):
            name, executable, kind, lanes, mode = case
            if executable == 'encoders':
                command = [binaries[executable], 'matrix_gestures' if lanes == 'gestures' else kind,
                           patches[lanes], mode, seconds]
                if kind in replay_kinds:
                    command.append(replay)
            else:
                command = [binaries[executable], kind, lanes, mode, seconds]
            row = execute(command)
            if row.get('finite') is not True:
                raise RuntimeError(f'Failed finite-output check: {name}')
            row.update(case=name, repetition=repeat, command=list(map(str, command)))
            log.write(json.dumps(row) + '\n')
            log.flush()
            rows.append(row)
            metric = 'us_per_transform' if 'us_per_transform' in row else 'ms_per_audio_second'
            print(f'[{index+1}/{len(jobs)}] {name}: {row[metric]:.3f} {metric}', flush=True)
    summary = {}
    for case in selected:
        samples = [row for row in rows if row['case'] == case[0]]
        metric = 'us_per_transform' if 'us_per_transform' in samples[0] else 'ms_per_audio_second'
        values = [row[metric] for row in samples]
        summary[case[0]] = {'unit': metric, 'median': statistics.median(values), 'min': min(values), 'max': max(values)}
    (output / 'summary.json').write_text(json.dumps({'metadata': metadata, 'cases': summary}, indent=2) + '\n')
    print(f'Results: {output / "summary.json"}')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        sys.exit(str(error))
