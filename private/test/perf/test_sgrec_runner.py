#!/usr/bin/env python3
"""Exercise callback summaries and the SGREC-to-results command end to end."""

import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch

import run_sgrec
from run_sgrec import ROOT, summarize

sys.path.insert(0, str(ROOT / 'scripts/tests'))
from test_prepare_sgrec_replay import PrepareReplayTests


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='sgrec-runner-') as directory:
        root = Path(directory)
        raw = root / 'known.jsonl'
        rows = [dict(callback=index, begin_frame=index * 512, second=index * 512 / 48000,
                     frames=512 if index < 4 else 100, events=1,
                     audio_seconds=(512 if index < 4 else 100) / 48000,
                     cpu_seconds=value, wall_seconds=value * 2)
                for index, value in enumerate((.001, .002, .003, .02, .05))]

        def save(records):
            raw.write_text(''.join(json.dumps(row) + '\n' for row in records))

        save(rows)
        result = summarize(raw, 48000, 512, 2148, 5)
        assert result['callbacks'] == 5 and result['full_callbacks'] == 4
        assert result['partial_tail_frames'] == 100
        assert result['cpu']['p50_ms'] == 2 and result['cpu']['p99_ms'] == 20
        assert result['cpu']['max_ms'] == 20 and result['wall']['max_ms'] == 40
        assert result['cpu']['over_budget_callbacks'] == 1
        assert result['cpu']['worst_callbacks'][0]['begin_frame'] == 1536
        assert math.isclose(result['cpu']['total_seconds'], .076)
        for invalid in (rows[:-1], rows + [rows[-1]], [dict(rows[0], cpu_seconds=float('nan'))] + rows[1:],
                        [dict(rows[0], events=0)] + rows[1:], [dict(rows[0], begin_frame=1)] + rows[1:]):
            save(invalid)
            try:
                summarize(raw, 48000, 512, 2148, 5)
            except ValueError:
                pass
            else:
                raise AssertionError('Invalid callback results accepted')

        recording = root / 'input.sgrec'
        recording.write_bytes(PrepareReplayTests().recording())
        output = root / 'results'
        command = [sys.executable, str(ROOT / 'private/test/perf/run_sgrec.py'), str(recording),
                   '--callback-frames', '3', '--repeats', '2', '--output', str(output),
                   '--bin-dir', str(binary.parent)]
        run = subprocess.run(command, capture_output=True, text=True)
        assert run.returncode == 0, run.stdout + run.stderr
        summary = json.loads((output / 'summary.json').read_text())
        assert summary['metadata']['status'] == 'complete'
        assert summary['metadata']['callback_frames'] == 3
        assert summary['metadata']['recorded_at_utc']
        assert len(summary['runs']) == 2
        for result in summary['runs']:
            assert result['frames'] == 4 and result['events'] == 1
            assert result['full_callbacks'] == 1 and result['partial_tail_frames'] == 1
        before = (output / 'summary.json').read_bytes()
        assert subprocess.run(command, capture_output=True).returncode != 0
        assert (output / 'summary.json').read_bytes() == before
        assert (output / 'final-patch-1.json').read_bytes() == (output / 'final-patch-2.json').read_bytes()

        interrupted = root / 'interrupted-results'
        interrupted_args = command[1:]
        interrupted_args[interrupted_args.index('--output') + 1] = str(interrupted)
        real_popen = subprocess.Popen

        def interrupt_native_replay(arguments, *args, **kwargs):
            if arguments[0] == str(binary):
                raise KeyboardInterrupt
            return real_popen(arguments, *args, **kwargs)

        with patch.object(sys, 'argv', interrupted_args), patch.object(subprocess, 'Popen', interrupt_native_replay):
            try:
                run_sgrec.main()
            except KeyboardInterrupt:
                pass
            else:
                raise AssertionError('Replay interruption was swallowed')
        assert json.loads((interrupted / 'metadata.json').read_text())['status'] == 'interrupted'
        interrupted_summary = json.loads((interrupted / 'summary.json').read_text())
        assert interrupted_summary['metadata']['status'] == 'interrupted'
        assert interrupted_summary['runs'] == []

        invalid_output = root / 'invalid-results'
        recording.write_bytes(recording.read_bytes()[:-16])
        command[command.index('--output') + 1] = str(invalid_output)
        assert subprocess.run(command, capture_output=True).returncode != 0
        assert not (invalid_output / 'summary.json').exists()
    print('SGREC runner: summaries, full pipeline, overwrite protection and interruption status passed')


if __name__ == '__main__':
    main()
