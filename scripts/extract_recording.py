#!/usr/bin/env python3
"""Inspect SmartGrid recordings and extract exact PCM24 masters or stems.

Examples:
    python scripts/extract_recording.py info session.sgrec --verify
    python scripts/extract_recording.py extract session.sgrec --master stereo
    python scripts/extract_recording.py extract session.sgrec --track 3 -o stem.wav

Incomplete recordings retain the WAV of their validated prefix and return a
nonzero exit status. Panned mono exports only sample_val, without applying pan.
"""

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import sys

from sgrec import Reader, RecordingError, x_quad_corners, x_stream_names

x_pcm_guid = bytes.fromhex('0100000000001000800000aa00389b71')
x_chunk_frames = 8192


@dataclass
class ExtractionResult:
    m_output: Path
    m_frames: int
    m_complete: bool
    m_error: str | None


def wav_header(sample_rate, channels, frames):
    """Reserve ds64 space as JUNK in RIFF, permitting in-place RF64 promotion."""
    if channels not in (1, 2, 4) or not 0 < sample_rate * channels * 3 <= 0xffffffff:
        raise ValueError('Sample rate/channel count cannot be represented in PCM24 WAV')
    if not 0 <= frames <= 0xffffffffffffffff:
        raise ValueError('WAV frame count is outside u64')
    fmt = struct.pack('<HHIIHH', 0xfffe if channels == 4 else 1, channels, sample_rate, sample_rate * channels * 3, channels * 3, 24)
    if channels == 4:
        fmt += struct.pack('<HHI', 22, 24, 0) + x_pcm_guid
    data_size = frames * channels * 3
    header_size = 12 + 36 + 8 + len(fmt) + 8
    riff_size = header_size - 8 + data_size + data_size % 2
    if riff_size > 0xffffffffffffffff:
        raise ValueError('RF64 output size is outside u64')
    if riff_size > 0xffffffff or data_size > 0xffffffff:
        prefix = struct.pack('<4sI4s4sIQQQI', b'RF64', 0xffffffff, b'WAVE', b'ds64', 28, riff_size, data_size, frames, 0)
        data_length = 0xffffffff
    else:
        prefix = struct.pack('<4sI4s4sI', b'RIFF', riff_size, b'WAVE', b'JUNK', 28) + bytes(28)
        data_length = data_size
    return prefix + struct.pack('<4sI', b'fmt ', len(fmt)) + fmt + struct.pack('<4sI', b'data', data_length)


class Pcm24Wav:
    """Write interleaved PCM in small chunks and finalize the actual frame count."""

    def __init__(self, output, sample_rate, channels):
        self.m_output = output
        self.m_sample_rate = sample_rate
        self.m_channels = channels
        self.m_frames = 0
        output.write(wav_header(sample_rate, channels, 0))

    def WriteBlock(self, streams, frames):
        for start in range(0, frames, x_chunk_frames):
            end = min(frames, start + x_chunk_frames)
            payload = bytearray((end - start) * self.m_channels * 3)
            offset = 0
            for index in range(start, end):
                for channel in range(self.m_channels):
                    value = streams[channel][index]
                    payload[offset] = value & 0xff
                    payload[offset + 1] = (value >> 8) & 0xff
                    payload[offset + 2] = (value >> 16) & 0xff
                    offset += 3
            self.m_output.write(payload)
            self.m_frames += end - start

    def Finalize(self):
        if (self.m_frames * self.m_channels * 3) % 2:
            self.m_output.write(b'\0')
        self.m_output.seek(0)
        self.m_output.write(wav_header(self.m_sample_rate, self.m_channels, self.m_frames))
        self.m_output.flush()


def select_track(reader, master=None, track_id=None):
    if (master is None) == (track_id is None):
        raise RecordingError('Select exactly one --master or --track')
    if master is not None:
        if master not in ('stereo', 'quad'):
            raise RecordingError('Master must be stereo or quad')
        matches = [track for track in reader.m_tracks_by_id.values() if track['role'] == f'master_{master}']
        if len(matches) != 1:
            raise RecordingError(f'No unique master_{master} track in recording')
        return matches[0]
    if track_id not in reader.m_tracks_by_id:
        raise RecordingError(f'Unknown selected track id: {track_id}')
    return reader.m_tracks_by_id[track_id]


def extract(input_path, output_path=None, *, master=None, track_id=None, overwrite=False):
    """Export validated blocks, returning completion status even for damaged tails.

    Header/selection errors raise RecordingError before an output is created.
    Filesystem failures raise OSError, including an existing output unless the
    caller explicitly supplies overwrite=True.
    """
    input_path = Path(input_path)
    with input_path.open('rb') as source:
        reader = Reader(source)
        track = select_track(reader, master, track_id)
        channels = 1 if track['type'] == 'panned_mono' else len(x_stream_names[track['type']])
        wav_header(reader.m_header['sample_rate'], channels, 0)
        suffix = master if master is not None else f'track_{track["id"]}'
        output_path = Path(output_path) if output_path is not None else input_path.with_name(f'{input_path.stem}_{suffix}.wav')
        if output_path.resolve() == input_path.resolve() or (output_path.exists() and output_path.samefile(input_path)):
            raise ValueError('Output must differ from the input recording')
        with output_path.open('wb' if overwrite else 'xb') as output:
            writer = Pcm24Wav(output, reader.m_header['sample_rate'], channels)
            error = None
            try:
                for block in reader.Blocks():
                    writer.WriteBlock(block.m_tracks[track['id']], block.m_frame_count)
                    del block
            except RecordingError as failure:
                error = str(failure)
            writer.Finalize()
        return ExtractionResult(output_path, writer.m_frames, reader.m_complete, error)


def inspect(input_path, verify=False):
    with Path(input_path).open('rb') as source:
        reader = Reader(source)
        header = reader.m_header
        for field in ('format_version', 'recorded_at_utc', 'git_commit_sha', 'sample_rate', 'block_frames'):
            print(f'{field}: {header[field]}')
        print('Audio: signed PCM24 integers, scale 8388607; direct extraction preserves codes.')
        print(f'Quad q0, q1, q2, q3 corners: {x_quad_corners}; unspecified WAV speaker mask.')
        for track in header['tracks']:
            print(f'{track["id"]}: {track["name"]} | {track["type"]} | role={track["role"]} | tap={track["tap"]}')
            print(f'  streams: {", ".join(x_stream_names[track["type"]])}')
            if track['type'] == 'panned_mono':
                print('  x/y: positions in [0, 8388607], divide by 8388607; extraction writes sample_val as mono.')
        if verify:
            try:
                for block in reader.Blocks():
                    del block
            except RecordingError as error:
                print(f'Incomplete after {reader.m_total_frames} frames: {error}', file=sys.stderr)
                return 1
            print(f'Complete: {reader.m_total_frames} frames')
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    info = commands.add_parser('info', help='Show metadata without decoding audio')
    info.add_argument('input', type=Path)
    info.add_argument('--verify', action='store_true', help='Scan every block and the completion marker')
    export = commands.add_parser('extract', help='Extract direct PCM24 audio; incomplete files keep a playable prefix and exit nonzero')
    export.add_argument('input', type=Path)
    selectors = export.add_mutually_exclusive_group(required=True)
    selectors.add_argument('--master', choices=('stereo', 'quad'))
    selectors.add_argument('--track', type=int)
    export.add_argument('-o', '--output', type=Path)
    export.add_argument('--overwrite', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.command == 'info':
            return inspect(args.input, args.verify)
        result = extract(args.input, args.output, master=args.master, track_id=args.track, overwrite=args.overwrite)
        if not result.m_complete:
            print(f'Incomplete: extracted {result.m_frames} frames to {result.m_output}: {result.m_error}', file=sys.stderr)
            return 1
        print(f'Extracted {result.m_frames} frames to {result.m_output}')
        return 0
    except (OSError, ValueError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
