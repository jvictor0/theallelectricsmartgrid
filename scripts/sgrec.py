"""Streaming audio and sample-specific patch reader for SmartGrid recordings.

The caller owns the binary input stream. Consume Reader.Blocks() through EOF to
validate completion, and discard each block before requesting the next one to
keep memory bounded. Pass track IDs to Blocks() to decode only those tracks
while retaining record structure and CRC validation. Each track contains
type-ordered signed int32 arrays.
"""

from array import array
from dataclasses import dataclass
from datetime import datetime, timedelta
import copy
import json
import math
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
    m_event_data: memoryview | None = None


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
    if type(header['format_version']) is not int or header['format_version'] not in (1, 2, 3):
        raise RecordingError('Unsupported format_version; expected 1, 2, or 3')
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
        self.m_data_offset = 12 + length

    def Blocks(self, track_ids=None, *, include_events=False):
        if self.m_started:
            raise RecordingError('The recording stream has already been consumed')
        selected_track_ids = None if track_ids is None else frozenset(track_ids)
        if selected_track_ids is not None:
            unknown = selected_track_ids.difference(self.m_tracks_by_id)
            if unknown:
                raise RecordingError(f'Unknown selected track id: {min(unknown)}')
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
            block_tag = f'BLK{self.m_header["format_version"]}'.encode('ascii')
            if tag != block_tag:
                raise RecordingError(f'Unknown record tag at frame {self.m_total_frames}: {tag!r}')
            size_bytes = read_exact(self.m_source, 4, 'block size')
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
            block = self.DecodeBlock(memoryview(raw), selected_track_ids, include_events=include_events)
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

    def DecodeBlock(self, raw, selected_track_ids=None, *, include_events=False):
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
        audio_end = offset + payload_bytes
        if audio_end + 4 > len(raw) or (self.m_header['format_version'] == 1 and audio_end + 4 != len(raw)):
            raise RecordingError('Derived descriptor/payload lengths do not match block size')
        tracks = {
            track_id: [None] * len(x_stream_names[track['type']])
            for track_id, track in self.m_tracks_by_id.items()
            if selected_track_ids is None or track_id in selected_track_ids
        }
        for track_id, stream_index, encoding, width, length in descriptors:
            if track_id in tracks:
                coordinate = self.m_tracks_by_id[track_id]['type'] == 'panned_mono' and stream_index > 0
                tracks[track_id][stream_index] = decode_stream(raw[offset:offset + length], frames, encoding, width, coordinate)
            offset += length
        for streams in tracks.values():
            for index, values in enumerate(streams):
                if values is None:
                    streams[index] = array('i', [0]) * frames
        events = raw[audio_end:-4] if include_events and self.m_header['format_version'] >= 2 else None
        return Block(start, frames, {track_id: tuple(streams) for track_id, streams in tracks.items()}, events)

    def PatchAtSample(self, sample):
        """Replay tracked edits through sample (inclusive) from the initial patch.

        Each query seeks to the recording's first block. It validates whole
        blocks through the target, without decoding audio or reading the tail.
        """
        if type(sample) is not int or sample < 0:
            raise RecordingError('Patch sample must be a nonnegative integer')
        initial = self.m_header.get('initial_patch')
        if self.m_header['format_version'] < 2 or not isinstance(initial, dict):
            raise RecordingError('Recording has no initial patch for reconstruction')
        patch = copy.deepcopy(initial)
        self.m_source.seek(self.m_data_offset)
        self.m_total_frames = 0
        self.m_complete = False
        self.m_started = False
        self.m_short_block = False
        for block in self.Blocks((), include_events=True):
            events = param_events(block, self.m_header['format_version'])
            for event in sorted(events, key=lambda event: (event.m_sample, event.m_order)):
                if event.m_sample <= sample:
                    apply_param_event(patch, event)
            if sample < block.m_start_frame + block.m_frame_count:
                return patch
        if sample == 0 and self.m_total_frames == 0:
            return patch
        raise RecordingError(f'Patch sample {sample} is outside the recording ({self.m_total_frames} frames)')


@dataclass
class ParamEvent:
    m_type: int
    m_name: str
    m_sample: int
    m_value: bytes | float | bool | dict
    m_scene: int = 0
    m_track: int = 0
    m_gesture: int = 0
    m_path: tuple = ()
    m_order: int = 0
    m_restore_faders: bool = False


def param_events(block, version=2):
    """Decode only the fields used by each event type, when replay needs them."""
    data = block.m_event_data
    offset = 0

    def fields(fmt):
        nonlocal offset
        size = struct.calcsize(fmt)
        if data is None or offset + size > len(data):
            raise RecordingError('Truncated event section')
        result = struct.unpack_from(fmt, data, offset)
        offset += size
        return result

    group_count, = fields('<I')
    for _ in range(group_count):
        event_type, width, name_length, count = fields('<BBHI')
        if event_type not in ((1, 2, 3, 4, 5, 6, 7) if version >= 3 else (1, 2, 3, 4, 5)):
            raise RecordingError(f'Unsupported patch event type: {event_type}')
        if ((event_type == 1 and width not in (1, 2, 4, 8))
                or (event_type in (2, 3, 4) and width != 4)
                or (event_type == 5 and width != 1)
                or (event_type in (6, 7) and width != 0)):
            raise RecordingError('Invalid event value width')
        if bool(name_length) != (event_type in (1, 4, 5)):
            raise RecordingError('Invalid event name length')
        try:
            name = fields(f'<{name_length}s')[0].decode('utf-8')
        except UnicodeDecodeError as error:
            raise RecordingError('Invalid event name') from error
        for _ in range(count):
            sample_offset, = fields('<I')
            if sample_offset >= block.m_frame_count:
                raise RecordingError('Event sample lies outside its block')
            event = ParamEvent(event_type, name, block.m_start_frame + sample_offset, b'')
            if version >= 3:
                event.m_order, = fields('<I')
            if event_type in (6, 7):
                if event_type == 6:
                    restore_faders, = fields('<B')
                    if restore_faders > 1:
                        raise RecordingError('Invalid PatchLoad restoreFaders flag')
                    event.m_restore_faders = bool(restore_faders)
                length, = fields('<I')
                try:
                    event.m_value = json.loads(fields(f'<{length}s')[0].decode('utf-8'),
                                              object_pairs_hook=unique_object, parse_constant=reject_constant)
                except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
                    raise RecordingError(f'Invalid patch event JSON: {error}') from error
                if not isinstance(event.m_value, dict):
                    raise RecordingError('Patch event JSON must be an object')
                yield event
                continue
            if event_type == 1:
                event.m_scene, = fields('<B')
            elif event_type == 2:
                event.m_gesture, = fields('<B')
                if event.m_gesture >= 16:
                    raise RecordingError('Invalid gesture index')
            elif event_type in (4, 5):
                event.m_scene, event.m_track, path_length = fields('<BBB')
                if event.m_track >= 16 or path_length > 16:
                    raise RecordingError('Invalid encoder track or path length')
                event.m_path = fields(f'<{path_length}B')
                if any(not (hop < 15 or 128 <= hop < 144) for hop in event.m_path):
                    raise RecordingError('Invalid encoder path')
            if event.m_scene >= 8:
                raise RecordingError('Invalid event scene')
            if event_type == 1:
                event.m_value, = fields(f'<{width}s')
            elif event_type == 5:
                active, = fields('<B')
                if active > 1 or not event.m_path or event.m_path[-1] < 128:
                    raise RecordingError('Invalid encoder activation')
                event.m_value = bool(active)
            else:
                event.m_value, = fields('<f')
                if not math.isfinite(event.m_value):
                    raise RecordingError('Invalid parameter value')
            yield event
    if offset != len(data):
        raise RecordingError('Trailing bytes in event section')


def apply_param_event(patch, event):
    if event.m_type == 1:
        apply_state_change(patch, event.m_name, event.m_scene, event.m_value)
    elif event.m_type == 2:
        try:
            patch['faders'][event.m_gesture] = event.m_value
        except (KeyError, IndexError, TypeError) as error:
            raise RecordingError('Cannot locate patch fader') from error
    elif event.m_type == 3:
        patch['blend'] = event.m_value
    elif event.m_type == 6:
        apply_patch_load(patch, event.m_value, event.m_restore_faders)
    elif event.m_type == 7:
        patch.clear()
        patch.update(copy.deepcopy(event.m_value))
    else:
        apply_encoder_event(patch, event)


def apply_patch_load(patch, loaded, restore_faders):
    """Apply the engine's partial patch load rules to the recorded state."""
    try:
        for section, scenes in (('nonagon', 8), ('stateSaver', 1)):
            incoming = loaded.get(section)
            if incoming is None:
                continue
            for name, values in patch.get(section, {}).items():
                source = incoming.get(name)
                if source is None:
                    continue
                width = len(values) // scenes
                if width not in (1, 2, 4, 8):
                    raise RecordingError(f'Cannot determine patch state width: {name}')
                for start in range(0, len(values), width):
                    if start >= len(source):
                        break
                    for index in range(start, start + width):
                        byte = int(source[index]) & 255 if index < len(source) else 0
                        values[index] = byte if byte < 128 else byte - 256
        apply_config_load(patch, loaded.get('configGrid'))
        incoming = loaded.get('squiggleBoy')
        if incoming is not None:
            for name, node in patch.get('squiggleBoy', {}).items():
                source = incoming.get(name)
                if source is not None:
                    load_encoder(node, source)
        if restore_faders:
            if loaded.get('blend') is not None:
                patch['blend'] = patch_float(loaded['blend'])
            faders = loaded.get('faders')
            if faders is not None and len(faders) >= 16:
                patch['faders'] = [patch_float(value) for value in faders[:16]]
    except (KeyError, IndexError, TypeError, AttributeError, ValueError, OverflowError, RecursionError, struct.error) as error:
        raise RecordingError(f'Cannot apply patch load: {error}') from error


def patch_float(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def load_encoder(node, source, *, bipolar=False, gesture=False):
    values = node['values']['values']
    incoming = source.get('values')
    if incoming is not None:
        rows = incoming.get('values', [])
        track_count = len(rows[7]) if len(rows) >= 8 else 0
        for scene in range(8):
            row = rows[scene] if scene < len(rows) else []
            previous = values[scene]
            values[scene] = [previous[track] if track < len(previous) else 0.0 for track in range(track_count)]
            for track, value in enumerate(row[:track_count]):
                value = patch_float(value)
                if bipolar:
                    value = patch_float(patch_float(patch_float(value + 1.0) * 0.5) * 2.0 - 1.0)
                values[scene][track] = value
    if gesture:
        active = source.get('active') or []
        node['active'] = [active[index] is True if index < len(active) else False for index in range(128)]
    for key, size in (('modulators', 15), ('gestures', 16)):
        node.pop(key, None)
        incoming_children = source.get(key)
        if incoming_children is None:
            continue
        children = [None] * size
        for index, child_source in enumerate(incoming_children[:size]):
            if child_source is None:
                continue
            child = {'values': {'values': [[0.0] * len(values[0]) for _ in range(8)]}}
            is_gesture = key == 'gestures'
            load_encoder(child, child_source, bipolar=bipolar or not is_gesture, gesture=is_gesture)
            keep = (any(child['active']) if is_gesture else
                    ('modulators' in child or 'gestures' in child
                     or any(value != 0.0 for row in child['values']['values'] for value in row)))
            if keep:
                children[index] = child
        if any(child is not None for child in children):
            node[key] = children


def apply_config_load(patch, incoming):
    config = patch.get('configGrid')
    states = patch.get('stateSaver', {})
    if not isinstance(config, dict):
        return
    stereo = config.get('sourceStereo', [])
    selected = config.get('sourceSelected', [])
    for index in range(len(stereo)):
        state = states.get(f'sourceWidth_{index}')
        if state is not None:
            stereo[index] = any(state)
    for trio, row in enumerate(selected):
        for index in range(len(row)):
            state = states.get(f'sourceSelected_{trio}_{index}')
            if state is not None:
                row[index] = bool(state[0])
    if incoming is None:
        return
    for index, value in enumerate((incoming.get('sourceStereo') or [])[:len(stereo)]):
        stereo[index] = value is True
        name = f'sourceWidth_{index}'
        if name in states:
            states[name] = [int(stereo[index])] + [0] * (len(states[name]) - 1)
    incoming_selected = incoming.get('sourceSelected') or []
    for trio, row in enumerate(selected):
        source = incoming_selected[trio] if trio < len(incoming_selected) else []
        row[:] = [source[index] is True if index < len(source) else False for index in range(len(row))]
        channels = sum(2 if stereo[index] else 1 for index, value in enumerate(row) if value)
        for index, value in enumerate(row):
            if channels <= 3:
                break
            if value:
                channels -= 2 if stereo[index] else 1
                row[index] = False
        for index, value in enumerate(row):
            name = f'sourceSelected_{trio}_{index}'
            if name in states:
                states[name] = [int(value)]
    for index, value in enumerate(incoming.get('sourceMonitor') or []):
        name = f'sourceMonitor_{index}'
        if name in states:
            states[name] = [int(value is True)]


def apply_encoder_event(patch, event):
    try:
        node = patch['squiggleBoy'][event.m_name]
        for hop in event.m_path:
            gesture = hop >= 128
            key, size = ('gestures', 16) if gesture else ('modulators', 15)
            index = hop & 0x7f
            children = node.setdefault(key, [None] * size)
            if children[index] is None:
                # New depths and inactive gestures start neutral in patch units.
                #
                track_count = len(node['values']['values'][0])
                child = {'values': {'values': [[0.0] * track_count for _ in range(8)]}}
                if gesture:
                    child['active'] = [False] * 128
                children[index] = child
            node = children[index]
        if event.m_type == 4:
            node['values']['values'][event.m_scene][event.m_track] = event.m_value
        else:
            node['active'][event.m_scene * 16 + event.m_track] = event.m_value
    except (KeyError, IndexError, TypeError, AttributeError) as error:
        raise RecordingError(f'Cannot update patch encoder: {event.m_name}') from error


def apply_state_change(patch, name, scene, value):
    width = len(value)
    if width not in (1, 2, 4, 8) or scene >= 8:
        raise RecordingError('Invalid StateChange width or scene')
    matches = [(section, patch[section][name]) for section in ('nonagon', 'stateSaver')
               if isinstance(patch.get(section), dict) and name in patch[section]]
    if len(matches) != 1:
        raise RecordingError(f'Cannot locate unique patch state: {name}')
    section, values = matches[0]
    start = scene * width
    if not isinstance(values, list) or start + width > len(values):
        raise RecordingError(f'StateChange exceeds patch state: {name}')
    values[start:start + width] = [byte if byte < 128 else byte - 256 for byte in value]

    # These two StateSaver fields also have a configGrid representation, which
    # the existing patch loader applies after StateSaver.
    #
    config = patch.get('configGrid')
    if section != 'stateSaver' or not isinstance(config, dict):
        return
    stereo = re.fullmatch(r'sourceWidth_(\d+)', name)
    selected = re.fullmatch(r'sourceSelected_(\d+)_(\d+)', name)
    try:
        if stereo and 'sourceStereo' in config:
            config['sourceStereo'][int(stereo[1])] = int.from_bytes(value, 'little') != 0
        elif selected and 'sourceSelected' in config:
            config['sourceSelected'][int(selected[1])][int(selected[2])] = bool(value[0])
    except (IndexError, KeyError, TypeError) as error:
        raise RecordingError(f'Cannot update configGrid copy of {name}') from error
