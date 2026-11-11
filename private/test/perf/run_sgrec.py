#!/usr/bin/env python3
"""Benchmark an SGREC control timeline through SynthRig in callback-sized windows."""

import argparse
from array import array
import datetime
import heapq
import json
import math
from pathlib import Path
import platform
import subprocess
import sys

from run import ROOT, child_env, digest, positive

sys.path.insert(0, str(ROOT / 'scripts'))
from prepare_sgrec_replay import prepare
from sgrec import RecordingError


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def summarize(path, sample_rate, callback_frames, expected_frames, expected_events):
    """Validate coverage; summarize full callbacks separately from a partial tail."""
    samples = {kind: array('d') for kind in ('cpu', 'wall')}
    worst = {kind: [] for kind in samples}
    totals = {kind: 0.0 for kind in samples}
    budget = callback_frames / sample_rate
    frames = events = count = partial_frames = 0
    with path.open() as source:
        for line in source:
            row = json.loads(line)
            size = min(callback_frames, expected_frames - frames)
            if (size <= 0 or row['callback'] != count or row['begin_frame'] != frames
                    or row['frames'] != size or type(row['events']) is not int or row['events'] < 0
                    or not math.isclose(row['second'], frames / sample_rate, abs_tol=1e-12)
                    or not math.isclose(row['audio_seconds'], size / sample_rate, abs_tol=1e-12)):
                raise ValueError(f'Invalid callback coverage at row {count}')
            for kind in samples:
                value = row[f'{kind}_seconds']
                if not math.isfinite(value) or value < 0:
                    raise ValueError(f'Invalid {kind} timing at row {count}')
                totals[kind] += value
                if size == callback_frames:
                    samples[kind].append(value)
                    item = (value, count, row)
                    if len(worst[kind]) < 10:
                        heapq.heappush(worst[kind], item)
                    else:
                        heapq.heappushpop(worst[kind], item)
            if size < callback_frames:
                partial_frames = size
            frames += size
            events += row['events']
            count += 1
    if frames != expected_frames or events != expected_events:
        raise ValueError(f'Incomplete replay: {frames}/{expected_frames} frames, {events}/{expected_events} events')
    result = {'frames': frames, 'audio_seconds': frames / sample_rate, 'events': events,
              'callbacks': count, 'full_callbacks': len(samples['cpu']),
              'partial_tail_frames': partial_frames, 'callback_budget_ms': budget * 1000,
              'percentile_method': 'nearest rank; full callbacks only'}
    for kind, values in samples.items():
        values = sorted(values)
        metrics = {'total_seconds': totals[kind],
                   'mean_ms_per_audio_second': totals[kind] * 1000 / result['audio_seconds'],
                   'over_budget_callbacks': sum(value > budget for value in values),
                   'worst_callbacks': [item[2] for item in sorted(worst[kind], reverse=True)]}
        for name, fraction in (('p50_ms', .5), ('p95_ms', .95), ('p99_ms', .99),
                               ('p99_9_ms', .999), ('max_ms', 1)):
            metrics[name] = values[max(0, math.ceil(len(values) * fraction) - 1)] * 1000 if values else None
        result[kind] = metrics
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording', type=Path)
    parser.add_argument('--callback-frames', type=int, required=True,
                        help='host frames per device callback, from the app route log; not SGREC block_frames')
    parser.add_argument('--seconds', type=positive, help='render from frame zero up to this duration; default: entire recording')
    parser.add_argument('--repeats', type=int, default=1, help='independent serial replays (default: 1)')
    parser.add_argument('--output', type=Path, help='new result directory; existing directories are refused')
    parser.add_argument('--bin-dir', type=Path, default=ROOT / 'build/tools/perf/Release')
    args = parser.parse_args()
    if not 1 <= args.callback_frames <= 48000:
        parser.error('--callback-frames must be in [1, 48000]')
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    binary = (args.bin_dir / 'perf_sgrec').resolve()
    if not binary.is_file():
        parser.error(f'{binary} is missing; run make perf-build')
    recording = args.recording.resolve(strict=True)
    output = (args.output or ROOT / 'build/results' /
              datetime.datetime.now().strftime('sgrec-perf-%Y%m%d-%H%M%S-%f')).resolve()
    output.mkdir(parents=True, exist_ok=False)
    prepared_path = output / 'replay.json'
    print(f'Validating {recording}...', flush=True)
    replay = prepare(recording, prepared_path)
    rate = replay['sample_rate']
    if rate != 48000:
        raise ValueError('Replay sample rate does not match the 48000 Hz engine')
    frames = min(replay['frames'], int(args.seconds * rate)) if args.seconds is not None else replay['frames']
    if frames < 1:
        raise ValueError('Duration must contain at least one host frame')
    events = sum(row[0] < frames for row in replay['events'])

    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(ROOT), *arguments], text=True).strip()

    metadata = {
        'schema': 1, 'created': datetime.datetime.now().astimezone().isoformat(),
        'status': 'running', 'recording': str(recording), 'recording_sha256': digest(recording),
        'recording_revision': replay['git_commit_sha'], 'recorded_at_utc': replay['recorded_at_utc'],
        'checkout_revision': git('rev-parse', 'HEAD'), 'checkout_status': git('status', '--short'),
        'platform': platform.platform(), 'machine': platform.machine(),
        'build': json.loads((args.bin_dir / 'build.json').read_text()),
        'binary': str(binary), 'binary_sha256': digest(binary), 'replay_sha256': digest(prepared_path),
        'sample_rate': rate, 'callback_frames': args.callback_frames, 'frames': frames,
        'callback_alignment': 'recording frame zero; original device boundary is not recorded',
        'measurement': 'offline SynthRig calling-thread CPU and elapsed wall time; not device xruns',
        'repeats': args.repeats, 'commands': [],
    }
    (output / 'source.diff').write_text(git('diff', 'HEAD') + '\n')
    write_json(output / 'metadata.json', metadata)
    if metadata['recording_revision'] != metadata['checkout_revision']:
        print('Recording and checkout revisions differ; see metadata.json.', flush=True)
    results = []
    try:
        for repeat in range(args.repeats):
            raw = output / f'callbacks-{repeat + 1}.jsonl'
            command = [str(binary), str(prepared_path), str(args.callback_frames),
                       str(args.seconds) if args.seconds is not None else 'all', str(raw),
                       str(output / f'final-patch-{repeat + 1}.json')]
            metadata['commands'].append(command)
            write_json(output / 'metadata.json', metadata)
            print(f'Replay {repeat + 1}/{args.repeats}: {frames / rate:.3f} s, '
                  f'{args.callback_frames} frames/callback ({1000 * args.callback_frames / rate:.3f} ms)', flush=True)
            with (output / f'replay-{repeat + 1}.log').open('w') as log:
                with subprocess.Popen(command, env=child_env(), text=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as process:
                    try:
                        for line in process.stdout:
                            log.write(line)
                            if line.startswith('Replayed '):
                                print(line.rstrip(), flush=True)
                        if process.wait():
                            raise RuntimeError(f'Replay failed; see {log.name}')
                    except KeyboardInterrupt:
                        process.terminate()
                        process.wait()
                        raise
            result = summarize(raw, rate, args.callback_frames, frames, events)
            result['repetition'] = repeat + 1
            result['raw'] = raw.name
            results.append(result)
            write_json(output / 'summary.json', {'metadata': metadata, 'runs': results})
        metadata['status'] = 'complete'
    except KeyboardInterrupt:
        metadata['status'] = 'interrupted'
        raise
    except Exception:
        metadata['status'] = 'failed'
        raise
    finally:
        write_json(output / 'metadata.json', metadata)
        write_json(output / 'summary.json', {'metadata': metadata, 'runs': results})
    print(f'Results: {output / "summary.json"}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, RecordingError) as error:
        sys.exit(str(error))
