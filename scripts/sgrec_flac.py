"""Bounded mono PCM24 FLAC decoding through the libFLAC C API.

The library is loaded only when a selected FLAC stream is decoded. Legacy
recordings and metadata-only extraction have no native decoder dependency.
"""

from array import array
import ctypes as c
from ctypes.util import find_library
from functools import lru_cache


x_read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p, c.POINTER(c.c_size_t), c.c_void_p)
x_tell = c.CFUNCTYPE(c.c_int, c.c_void_p, c.POINTER(c.c_uint64), c.c_void_p)
x_write = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p,
                     c.POINTER(c.POINTER(c.c_int32)), c.c_void_p)
x_error = c.CFUNCTYPE(None, c.c_void_p, c.c_int, c.c_void_p)


def validate_flac(payload, frame_count, sample_rate):
    """Validate dimensions before native decoding or skipping an unselected stream."""
    if len(payload) < 42 or bytes(payload[:4]) != b'fLaC':
        raise ValueError('Missing or truncated FLAC stream header')
    if payload[4] & 127 or int.from_bytes(payload[5:8], 'big') != 34:
        raise ValueError('FLAC must begin with a 34-byte STREAMINFO')
    minimum = int.from_bytes(payload[8:10], 'big')
    maximum = int.from_bytes(payload[10:12], 'big')
    fields = int.from_bytes(payload[18:26], 'big')
    if not 16 <= minimum <= maximum <= 1024:
        raise ValueError('FLAC internal block size exceeds the 1024-sample bound')
    if (fields >> 44 != sample_rate or (fields >> 41) & 7
            or ((fields >> 36) & 31) != 23 or (fields & ((1 << 36) - 1)) != frame_count):
        raise ValueError('FLAC STREAMINFO does not match container rate/channels/depth/frame count')
    offset = 4
    while True:
        if offset + 4 > len(payload):
            raise ValueError('Truncated FLAC metadata')
        kind = payload[offset] & 127
        if kind == 127 or (offset != 4 and kind == 0):
            raise ValueError('Invalid FLAC metadata type')
        last = payload[offset] & 128
        offset += 4 + int.from_bytes(payload[offset + 1:offset + 4], 'big')
        if offset >= len(payload):
            raise ValueError('Truncated FLAC metadata or missing audio')
        if last:
            return


@lru_cache(maxsize=1)
def _load_library():
    name = find_library('FLAC')
    try:
        if not name:
            raise OSError('libFLAC was not found')
        library = c.CDLL(name)
    except OSError as error:
        raise ValueError('FLAC extraction requires libFLAC: install flac with your package manager '
                         '(macOS: brew install flac)') from error
    signatures = {
        'new': (c.c_void_p, []),
        'delete': (None, [c.c_void_p]),
        'set_md5_checking': (c.c_int, [c.c_void_p, c.c_int]),
        'init_stream': (c.c_int, [c.c_void_p, x_read, c.c_void_p, x_tell, c.c_void_p,
                                  c.c_void_p, x_write, c.c_void_p, x_error, c.c_void_p]),
        'process_until_end_of_metadata': (c.c_int, [c.c_void_p]),
        'process_single': (c.c_int, [c.c_void_p]),
        'get_decode_position': (c.c_int, [c.c_void_p, c.POINTER(c.c_uint64)]),
        'finish': (c.c_int, [c.c_void_p]),
    }
    for getter in ('blocksize', 'channels', 'bits_per_sample', 'sample_rate', 'state'):
        signatures['get_' + getter] = (c.c_uint, [c.c_void_p])
    try:
        for name, (result, arguments) in signatures.items():
            function = getattr(library, 'FLAC__stream_decoder_' + name)
            function.restype = result
            function.argtypes = arguments
    except AttributeError as error:
        raise ValueError('Installed libFLAC lacks the required decoder API') from error
    return library


def decode_flac(payload, frame_count, sample_rate, coordinate):
    validate_flac(payload, frame_count, sample_rate)
    library = _load_library()
    values = array('i', [0]) * frame_count
    address, _ = values.buffer_info()
    input_offset = 0
    written = 0
    errors = []

    @x_read
    def read(decoder, target, requested, context):
        nonlocal input_offset
        try:
            count = min(requested[0], len(payload) - input_offset)
            requested[0] = count
            if not count:
                return 1
            c.memmove(target, bytes(payload[input_offset:input_offset + count]), count)
            input_offset += count
            return 0
        except Exception as error:
            errors.append(str(error))
            return 2

    @x_tell
    def tell(decoder, position, context):
        position[0] = input_offset
        return 0

    @x_write
    def write(decoder, frame, buffers, context):
        nonlocal written
        try:
            count = library.FLAC__stream_decoder_get_blocksize(decoder)
            if (not 1 <= count <= 1024 or written + count > frame_count
                    or library.FLAC__stream_decoder_get_channels(decoder) != 1
                    or library.FLAC__stream_decoder_get_bits_per_sample(decoder) != 24
                    or library.FLAC__stream_decoder_get_sample_rate(decoder) != sample_rate):
                errors.append('FLAC frame dimensions disagree with container')
                return 1
            c.memmove(address + written * 4, buffers[0], count * 4)
            written += count
            return 0
        except Exception as error:
            errors.append(str(error))
            return 1

    @x_error
    def error(decoder, status, context):
        errors.append(f'FLAC decoder error {status}')

    decoder = library.FLAC__stream_decoder_new()
    if not decoder:
        raise ValueError('Cannot allocate FLAC decoder')
    initialized = False
    try:
        if not library.FLAC__stream_decoder_set_md5_checking(decoder, 1):
            raise ValueError('Cannot enable FLAC integrity checking')
        if library.FLAC__stream_decoder_init_stream(decoder, read, None, tell, None, None,
                                                   write, None, error, None) != 0:
            raise ValueError('Cannot initialize FLAC decoder')
        initialized = True
        if not library.FLAC__stream_decoder_process_until_end_of_metadata(decoder) or errors:
            raise ValueError('Invalid FLAC metadata')
        while written < frame_count:
            previous = written
            if not library.FLAC__stream_decoder_process_single(decoder) or errors or written == previous:
                raise ValueError('Corrupt or truncated FLAC audio')
        consumed = c.c_uint64()
        if (not library.FLAC__stream_decoder_get_decode_position(decoder, c.byref(consumed))
                or consumed.value != len(payload)):
            raise ValueError('Trailing data after FLAC audio')
        valid = library.FLAC__stream_decoder_finish(decoder)
        initialized = False
        if not valid:
            raise ValueError('FLAC MD5 mismatch')
        if min(values) < (0 if coordinate else -8388608) or max(values) > 8388607:
            raise ValueError('FLAC value outside PCM24/position range')
        return values
    finally:
        if initialized:
            library.FLAC__stream_decoder_finish(decoder)
        library.FLAC__stream_decoder_delete(decoder)
