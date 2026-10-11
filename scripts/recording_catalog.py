"""Bounded recording metadata and durable non-secret publishing state."""

import os
import re
import sqlite3
import struct
import uuid
from datetime import datetime
from pathlib import Path

from sgrec import Reader


x_timestamp = re.compile(r'recording-(\d{4}-\d{2}-\d{2})T(\d{2})(\d{2})(\d{2})')
x_header_limit = 1024 * 1024


def SafePath(root, relative):
    root = Path(root)
    pieces = Path(relative).parts
    if not pieces or Path(relative).is_absolute() or any(p in ('.', '..') for p in pieces):
        raise ValueError('unsafe catalog path')
    current = root
    if current.is_symlink():
        raise ValueError('symlink catalog root')
    for piece in pieces:
        current = current / piece
        if current.is_symlink():
            raise ValueError('symlink catalog path')
    return current


def Version(path):
    status = path.stat()
    return f'{status.st_size}:{status.st_mtime_ns}'


def WaveDuration(path):
    size = path.stat().st_size
    with path.open('rb') as source:
        prefix = source.read(12)
        if len(prefix) != 12 or prefix[:4] not in (b'RIFF', b'RF64') or prefix[8:] != b'WAVE':
            raise ValueError('not a RIFF/RF64 WAVE file')
        byte_rate = None
        large_data = None
        while source.tell() + 8 <= min(size, x_header_limit):
            tag, length = struct.unpack('<4sI', source.read(8))
            if tag == b'data':
                data_size = large_data if length == 0xffffffff else length
                if not byte_rate or data_size is None or source.tell() + data_size > size:
                    raise ValueError('incomplete WAVE data')
                return data_size / byte_rate
            if source.tell() + length > min(size, x_header_limit):
                raise ValueError('WAVE header exceeds bounded limit')
            if tag == b'ds64':
                if length < 28:
                    raise ValueError('invalid RF64 ds64')
                payload = source.read(28)
                large_data = struct.unpack_from('<Q', payload, 8)[0]
                source.seek(length - 28, os.SEEK_CUR)
            elif tag == b'fmt ':
                if length < 16:
                    raise ValueError('invalid WAVE format')
                encoding, channels, rate, byte_rate, alignment, bits = struct.unpack('<HHIIHH', source.read(16))
                if encoding not in (1, 3, 0xfffe) or channels != 2 or not rate or not alignment or not bits or byte_rate != rate * alignment:
                    raise ValueError('not a supported stereo WAVE')
                source.seek(length - 16, os.SEEK_CUR)
            else:
                source.seek(length, os.SEEK_CUR)
            source.seek(length % 2, os.SEEK_CUR)
    raise ValueError('missing WAVE data header')


def RecordedAt(path):
    match = x_timestamp.search(path.name)
    if match:
        return datetime.fromisoformat(f'{match[1]}T{match[2]}:{match[3]}:{match[4]}').isoformat()
    original = path.with_name(path.name[:-len('_stereo.wav')])
    for suffix in ('.sgrec', '.wav'):
        source_path = original.with_suffix(suffix)
        if source_path.is_file() and not source_path.is_symlink():
            if suffix == '.sgrec':
                try:
                    with source_path.open('rb') as source:
                        stamp = Reader(source).m_header['recorded_at_utc']
                    return datetime.fromisoformat(stamp.replace('Z', '+00:00')).astimezone().replace(tzinfo=None).isoformat()
                except (ValueError, OSError):
                    pass
            return datetime.fromtimestamp(source_path.stat().st_mtime).isoformat()
    return datetime.fromtimestamp(path.stat().st_mtime).isoformat()


class RecordingCatalog:
    def __init__(self, root):
        self.m_root = Path(root)
        if self.m_root.is_symlink():
            raise ValueError('symlink server root')
        self.m_root.mkdir(parents=True, exist_ok=True)
        database = self.m_root / '.soundcloud.sqlite3'
        if database.is_symlink():
            raise ValueError('symlink publishing database')
        self.m_db = sqlite3.connect(database, check_same_thread=False)
        self.m_db.row_factory = sqlite3.Row
        self.m_db.execute('PRAGMA journal_mode=WAL')
        self.m_db.execute('PRAGMA synchronous=FULL')
        self.m_db.executescript('''
            CREATE TABLE IF NOT EXISTS recordings (
                id TEXT PRIMARY KEY, path TEXT UNIQUE, version TEXT, recorded_at TEXT,
                date TEXT, duration REAL, size INTEGER, present INTEGER);
            CREATE TABLE IF NOT EXISTS accounts (
                id TEXT PRIMARY KEY, name TEXT, ready INTEGER, tracks TEXT);
            CREATE TABLE IF NOT EXISTS requests (
                request_id TEXT PRIMARY KEY, identity TEXT, account TEXT, job_id TEXT);
            CREATE TABLE IF NOT EXISTS jobs (
                sequence INTEGER PRIMARY KEY AUTOINCREMENT, job_id TEXT UNIQUE,
                request_id TEXT UNIQUE, identity TEXT, account TEXT, recording_id TEXT,
                version TEXT, date TEXT, title TEXT, slot INTEGER, artwork TEXT,
                state TEXT, progress REAL, error TEXT, track_id TEXT, permalink TEXT);
        ''')
        self.m_db.execute("INSERT OR IGNORE INTO requests SELECT request_id,identity,account,job_id FROM jobs WHERE request_id IS NOT NULL")
        self.m_db.execute("UPDATE jobs SET state='unknown', error='Receiver stopped during upload' WHERE state='uploading'")
        self.m_db.commit()

    def Scan(self):
        self.m_db.execute('UPDATE recordings SET present=0')
        directory = self.m_root / 'recordings'
        if directory.exists() and not directory.is_symlink():
            for path in directory.rglob('*_stereo.wav'):
                relative = path.relative_to(self.m_root).as_posix()
                if any(piece.startswith('.') for piece in path.relative_to(directory).parts):
                    continue
                try:
                    path = SafePath(self.m_root, relative)
                    if not path.is_file():
                        continue
                    version = Version(path)
                    previous = self.m_db.execute('SELECT * FROM recordings WHERE path=?', (relative,)).fetchone()
                    if previous and previous['version'] == version:
                        self.m_db.execute('UPDATE recordings SET present=1 WHERE id=?', (previous['id'],))
                        continue
                    duration = WaveDuration(path)
                    recorded_at = previous['recorded_at'] if previous else RecordedAt(path)
                    date = previous['date'] if previous else recorded_at[:10].replace('-', '/')
                    identifier = previous['id'] if previous else uuid.uuid4().hex
                    self.m_db.execute('INSERT OR REPLACE INTO recordings VALUES (?,?,?,?,?,?,?,1)',
                                      (identifier, relative, version, recorded_at, date, duration, path.stat().st_size))
                except (OSError, ValueError):
                    continue
        self.m_db.commit()

    def Rows(self):
        return self.m_db.execute('SELECT * FROM recordings WHERE present=1 ORDER BY recorded_at DESC, path DESC').fetchall()
