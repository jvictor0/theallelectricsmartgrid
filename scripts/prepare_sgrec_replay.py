#!/usr/bin/env python3
"""Prepare a validated v6 control timeline for perf_sgrec.

This is control replay, not deterministic audio reconstruction. The initial
patch and clock phase are restored; delay history, RNG state, raw external
inputs and sample assets are not stored by this format. Bulk patch events are
rejected until the native replay supports their exact semantics.
"""

import argparse
import json
from pathlib import Path

from sgrec import Reader, RecordingError, param_events


def prepare(source_path, output_path):
    events = []
    initial_phase = None
    with Path(source_path).open('rb') as source:
        reader = Reader(source)
        header = reader.m_header
        if header['format_version'] != 6 or 'initial_patch' not in header:
            raise RecordingError('Control replay requires a v6 initial patch and transport events')
        if header.get('parameters') != ['global_phase_unmodulated']:
            raise RecordingError('Expected the recorded global phase parameter')
        for block in reader.Blocks((), include_events=True):
            if initial_phase is None:
                # Decode only the first parameter block; the rest still receives
                # container framing/CRC checks in Reader.Blocks.
                with Path(source_path).open('rb') as phase_source:
                    phase_reader = Reader(phase_source)
                    first = next(phase_reader.Blocks((), include_parameters=True))
                    initial_phase = first.m_parameters['global_phase_unmodulated'][0]
            for event in sorted(param_events(block, 6), key=lambda e: (e.m_sample, e.m_order)):
                if event.m_type in (6, 7):
                    raise RecordingError('Bulk patch load/reset replay is not implemented')
                value = list(event.m_value) if isinstance(event.m_value, bytes) else event.m_value
                events.append([event.m_sample, event.m_type, event.m_name, event.m_scene,
                               event.m_gesture, list(event.m_path), value, event.m_order])
            del block
        if not events or events[0][0] != 0 or events[0][1] not in (8, 9):
            raise RecordingError('Missing initial transport state')
        result = {
            'replay_version': 1,
            'source': str(Path(source_path).resolve()),
            'git_commit_sha': header['git_commit_sha'],
            'sample_rate': header['sample_rate'],
            'frames': reader.m_total_frames,
            'initial_phase': initial_phase,
            'initial_patch': header['initial_patch'],
            'events': events,
        }
    with Path(output_path).open('x') as output:
        json.dump(result, output, separators=(',', ':'), allow_nan=False)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        result = prepare(args.recording, args.output)
    except (OSError, RecordingError) as error:
        parser.exit(1, f'{error}\n')
    print(f"Prepared {len(result['events'])} events, {result['frames'] / result['sample_rate']:.3f} seconds")


if __name__ == '__main__':
    main()
