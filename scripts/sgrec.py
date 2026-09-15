"""Bounded, validating reader for version-one SmartGrid recordings.

The caller owns the binary input stream. Consume Reader.Blocks() through EOF to
validate completion, and discard each block before requesting the next one to
keep memory bounded. Each track contains type-ordered signed int32 arrays.
"""

from array import array
from dataclasses import dataclass
from datetime import datetime, timedelta
import json
import re
import struct
import zlib

x_magic = b'SMRTGRID'
x_max_header_bytes = 1024 * 1024
x_max_tracks = 128
x_max_record_bytes = 64 * 1024 * 1024
x_max_decoded_bytes = 64 * 1024 * 1024
x_stream_names = {
    'mono': ('sample_val',),
    'panned_mono': ('sample_val', 'x', 'y'),
    'stereo': ('left', 'right'),
    'quad': ('q0', 'q1', 'q2', 'q3'),
}
x_quad_corners = ((0, 1), (1, 1), (1, 0), (0, 0))


class RecordingError(ValueError):
    """The recording is malformed or lacks validated clean completion."""


@dataclass
class Block:
    m_start_frame: int
    m_frame_count: int
    m_tracks: dict


def read_exact(source, count, label):
    data = source.read(count)
    if len(data) != count:
        raise RecordingError(f'Truncated {label}: expected {count} bytes, got {len(data)}')
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RecordingError(f'Duplicate JSON field: {key}')
        result[key] = value
    return result


def reject_constant(value):
    raise RecordingError(f'Invalid JSON number: {value}')


def require_u32(value, name, minimum=0):
    if type(value) is not int or not minimum <= value <= 0xffffffff:
        raise RecordingError(f'{name} must be an integer in [{minimum}, 4294967295]')


def validate_header(header):
    if not isinstance(header, dict):
        raise RecordingError('Session header must be a JSON object')
    required = ('format_version', 'recorded_at_utc', 'git_commit_sha', 'sample_rate', 'block_frames', 'tracks')
    for field in required:
        if field not in header:
            raise RecordingError(f'Missing session field: {field}')
    if type(header['format_version']) is not int or header['format_version'] != 1:
        raise RecordingError('Unsupported format_version; expected 1')
    timestamp = header['recorded_at_utc']
    if not isinstance(timestamp, str) or not re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|\+00:00)', timestamp):
        raise RecordingError('recorded_at_utc must be an ISO 8601 UTC timestamp')
    try:
        parsed = datetime.fromisoformat(timestamp.replace('Z', '+00:00'))
        if parsed.utcoffset() != timedelta(0):
            raise ValueError('not UTC')
    except ValueError as error:
        raise RecordingError('Invalid recorded_at_utc timestamp') from error
    sha = header['git_commit_sha']
    if not isinstance(sha, str) or not re.fullmatch(r'(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})', sha):
        raise RecordingError('git_commit_sha must be a full hexadecimal Git object ID')
    require_u32(header['sample_rate'], 'sample_rate', 1)
    require_u32(header['block_frames'], 'block_frames', 1)
    tracks = header['tracks']
    if not isinstance(tracks, list) or len(tracks) > x_max_tracks:
        raise RecordingError('tracks must be an array with at most 128 tracks')
    tracks_by_id = {}
    master_roles = set()
    stream_count = 0
    for track in tracks:
        if not isinstance(track, dict) or any(field not in track for field in ('id', 'name', 'type', 'role', 'tap')):
            raise RecordingError('Each track requires id, name, type, role, and tap')
        require_u32(track['id'], 'track id')
        if track['id'] in tracks_by_id:
            raise RecordingError(f'Duplicate track id: {track["id"]}')
        for field in ('name', 'type', 'role', 'tap'):
            if not isinstance(track[field], str) or not track[field]:
                raise RecordingError(f'Track {track["id"]} requires a nonempty {field}')
        if track['type'] not in x_stream_names:
            raise RecordingError(f'Unknown track type: {track["type"]}')
        role = track['role']
        if role in ('master_stereo', 'master_quad'):
            if role in master_roles or track['type'] != role.removeprefix('master_') or track['tap'] != 'post_mastering_pre_master_volume':
                raise RecordingError(f'{role} requires a unique matching track type and pre-volume mastering tap')
            master_roles.add(role)
        tracks_by_id[track['id']] = track
        stream_count += len(x_stream_names[track['type']])
    if array('i').itemsize != 4:
        raise RecordingError('This reader requires a platform with four-byte array integers')
    if header['block_frames'] * stream_count * 4 > x_max_decoded_bytes:
        raise RecordingError('Declared decoded block exceeds the 64 MiB limit')
    return tracks_by_id


def pcm24(data, offset):
    value = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
    return value - 0x1000000 if value & 0x800000 else value


def decode_stream(payload, frame_count, encoding, width, coordinate):
    values = array('i', [0]) * frame_count
    minimum = 0 if coordinate else -8388608
    if encoding == 0:
        for index in range(frame_count):
            value = pcm24(payload, index * 3)
            if value < minimum:
                raise RecordingError('Position value outside [0, 8388607]')
            values[index] = value
        return values
    value = pcm24(payload, 0)
    if not minimum <= value <= 8388607:
        raise RecordingError('Initial value outside PCM24/position range')
    values[0] = value
    if width == 0:
        for index in range(1, frame_count):
            values[index] = value
        return values
    mask = (1 << width) - 1
    sign = 1 << (width - 1)
    bits = 0
    available = 0
    offset = 3
    for index in range(1, frame_count):
        while available < width:
            bits |= payload[offset] << available
            available += 8
            offset += 1
        delta = bits & mask
        bits >>= width
        available -= width
        value += delta - (1 << width) if delta & sign else delta
        if not minimum <= value <= 8388607:
            raise RecordingError('Reconstructed value outside PCM24/position range')
        values[index] = value
    if bits:
        raise RecordingError('Nonzero delta padding bits')
    return values


class Reader:
    """Read metadata immediately; validate data and completion while iterating."""

    def __init__(self, source):
        self.m_source = source
        self.m_total_frames = 0
        self.m_complete = False
        self.m_started = False
        self.m_short_block = False
        if read_exact(source, 8, 'file magic') != x_magic:
            raise RecordingError('Invalid recording magic; expected SMRTGRID')
        length = struct.unpack('<I', read_exact(source, 4, 'JSON length'))[0]
        if not 0 < length <= x_max_header_bytes:
            raise RecordingError('JSON header size exceeds the 1 MiB limit or is empty')
        try:
            self.m_header = json.loads(read_exact(source, length, 'JSON header').decode('utf-8'), object_pairs_hook=unique_object, parse_constant=reject_constant)
        except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
            raise RecordingError(f'Invalid JSON header: {error}') from error
        self.m_tracks_by_id = validate_header(self.m_header)

    def Blocks(self):
        if self.m_started:
            raise RecordingError('The recording stream has already been consumed')
        self.m_started = True
        while True:
            tag = self.m_source.read(4)
            if not tag:
                raise RecordingError(f'Missing END1 completion marker after {self.m_total_frames} frames')
            if len(tag) != 4:
                raise RecordingError('Truncated record tag')
            if tag == b'END1':
                end = tag + read_exact(self.m_source, 12, 'END1 completion marker')
                self.CheckCrc(end)
                if struct.unpack_from('<Q', end, 4)[0] != self.m_total_frames:
                    raise RecordingError('END1 frame count does not match recorded frames')
                if self.m_source.read(1):
                    raise RecordingError('Data follows END1 completion marker')
                self.m_complete = True
                return
            if tag != b'BLK1':
                raise RecordingError(f'Unknown record tag at frame {self.m_total_frames}: {tag!r}')
            size_bytes = read_exact(self.m_source, 4, 'BLK1 size')
            size = struct.unpack('<I', size_bytes)[0]
            if not 26 <= size <= x_max_record_bytes:
                raise RecordingError('BLK1 size is outside the 26-byte to 64 MiB record limit')
            raw = bytearray(size)
            raw[:8] = tag + size_bytes
            view = memoryview(raw)
            offset = 8
            while offset < size:
                count = self.m_source.readinto(view[offset:])
                if not count:
                    raise RecordingError(f'Truncated BLK1 record: expected {size} bytes, got {offset}')
                offset += count
            del view
            self.CheckCrc(raw)
            block = self.DecodeBlock(memoryview(raw))
            self.m_total_frames += block.m_frame_count
            self.m_short_block = block.m_frame_count < self.m_header['block_frames']
            del raw
            yield block
            del block

    def CheckCrc(self, record):
        actual = zlib.crc32(memoryview(record)[:-4])
        expected = struct.unpack_from('<I', record, len(record) - 4)[0]
        if actual != expected:
            raise RecordingError(f'CRC mismatch at frame {self.m_total_frames}')

    def DecodeBlock(self, raw):
        start, frames, track_count = struct.unpack_from('<QIH', raw, 8)
        if start != self.m_total_frames:
            raise RecordingError(f'Frame discontinuity: expected {self.m_total_frames}, found {start}')
        if self.m_short_block:
            raise RecordingError('A short block must be the final data block')
        if not 1 <= frames <= self.m_header['block_frames']:
            raise RecordingError('BLK1 frame count is outside the declared block size')
        if self.m_total_frames + frames > 0xffffffffffffffff:
            raise RecordingError('Total frame count exceeds u64')
        if track_count > len(self.m_tracks_by_id):
            raise RecordingError('BLK1 includes more tracks than the session declares')
        descriptors = []
        offset = 22
        payload_bytes = 0
        previous_id = -1
        for _ in range(track_count):
            if offset + 4 > len(raw) - 4:
                raise RecordingError('Truncated track descriptor')
            track_id = struct.unpack_from('<I', raw, offset)[0]
            offset += 4
            if track_id not in self.m_tracks_by_id:
                raise RecordingError(f'Unknown included track id: {track_id}')
            if track_id <= previous_id:
                raise RecordingError('Track descriptors must have unique ascending IDs')
            previous_id = track_id
            track = self.m_tracks_by_id[track_id]
            for stream_index in range(len(x_stream_names[track['type']])):
                if offset + 2 > len(raw) - 4:
                    raise RecordingError('Truncated stream descriptor')
                encoding, width = raw[offset:offset + 2]
                offset += 2
                if encoding == 0 and width == 24:
                    length = frames * 3
                elif encoding == 1 and width <= 25:
                    length = 3 + ((frames - 1) * width + 7) // 8
                else:
                    raise RecordingError(f'Invalid stream encoding/width: {encoding}/{width}')
                descriptors.append((track_id, stream_index, encoding, width, length))
                payload_bytes += length
        if offset + payload_bytes + 4 != len(raw):
            raise RecordingError('Derived descriptor/payload lengths do not match BLK1 size')
        tracks = {track_id: [None] * len(x_stream_names[track['type']]) for track_id, track in self.m_tracks_by_id.items()}
        for track_id, stream_index, encoding, width, length in descriptors:
            coordinate = self.m_tracks_by_id[track_id]['type'] == 'panned_mono' and stream_index > 0
            tracks[track_id][stream_index] = decode_stream(raw[offset:offset + length], frames, encoding, width, coordinate)
            offset += length
        for streams in tracks.values():
            for index, values in enumerate(streams):
                if values is None:
                    streams[index] = array('i', [0]) * frames
        return Block(start, frames, {track_id: tuple(streams) for track_id, streams in tracks.items()})
