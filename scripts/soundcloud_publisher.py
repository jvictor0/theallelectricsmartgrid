"""Account-scoped cached catalog and one durable Mac publishing worker."""

import hashlib
import json
import os
import re
import threading
import uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from recording_catalog import RecordingCatalog, SafePath, Version
from soundcloud_api import APIError


x_nightly = re.compile(r'^Smart Grid Nightly (\d{4}/\d{2}/\d{2})(?:\s+#([2-9]\d*|1\d+))?$', re.IGNORECASE)
x_max_artwork = 10 * 1024 * 1024


class PublishingError(ValueError):
    def __init__(self, message, status=400):
        super().__init__(message)
        self.status = status


def Track(track):
    return {'id': str(track.get('urn') or track.get('id') or ''),
            'title': track.get('title', ''), 'permalink': track.get('permalink_url', track.get('permalink', '')),
            'duration_ms': track.get('duration', track.get('duration_ms', 0)) or 0}


class PublishingService:
    def __init__(self, root, api):
        self.m_catalog = RecordingCatalog(root)
        self.m_db = self.m_catalog.m_db
        self.m_api = api
        self.m_lock = threading.RLock()
        self.m_worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix='sg-publish')
        self.m_refresh_worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix='sg-catalog')
        self.m_refreshing = False
        self.m_error = ''
        self.m_closed = False
        self.m_account = {'id': '', 'name': '', 'connected': False}
        self.m_tracks = []
        self.m_ready = False
        if api is not None:
            configured = getattr(api, 'm_tokens', {}).get('account', {})
            cached = self.m_db.execute('SELECT * FROM accounts ORDER BY rowid DESC LIMIT 1').fetchone()
            if configured:
                self.SetAccount(configured)
            elif cached:
                self.SetAccount({'id': cached['id'], 'name': cached['name']})
        self.m_catalog.Scan()
        if api is not None:
            for job in self.m_db.execute("SELECT job_id FROM jobs WHERE state='queued' ORDER BY sequence").fetchall():
                self.m_worker.submit(self.Run, job['job_id'])

    def SetAccount(self, account):
        identifier = str(account.get('urn') or account.get('id') or '')
        if not identifier:
            raise ValueError('Missing SoundCloud account identity')
        self.m_account = {'id': identifier, 'name': account.get('username', account.get('name', '')), 'connected': True}
        cached = self.m_db.execute('SELECT * FROM accounts WHERE id=?', (identifier,)).fetchone()
        self.m_ready = bool(cached and cached['ready'])
        self.m_tracks = json.loads(cached['tracks']) if cached else []

    def CurrentJob(self, row):
        return self.m_db.execute('SELECT * FROM jobs WHERE account=? AND recording_id=? AND version=? ORDER BY sequence DESC LIMIT 1',
                                 (self.m_account['id'], row['id'], row['version'])).fetchone()

    def Slot(self, date):
        known = {}
        highest = 0
        for track in self.m_tracks:
            match = x_nightly.fullmatch(track['title'])
            if match and match[1] == date:
                slot = int(match[2] or 1)
                known[track['id']] = slot
                highest = max(highest, slot)
        for job in self.m_db.execute("SELECT * FROM jobs WHERE account=? AND date=? AND state IN ('queued','uploading','uploaded','unknown')", (self.m_account['id'], date)):
            identity = job['track_id'] or job['job_id']
            known[identity] = job['slot'] or 1
            match = x_nightly.fullmatch(job['title'])
            explicit_slot = int(match[2] or 1) if match and match[1] == date else 0
            highest = max(highest, job['slot'] or 0, explicit_slot)
        return max(len(known), highest) + 1

    def Title(self, date, slot):
        return 'Smart Grid Nightly ' + date + (f' #{slot}' if slot > 1 else '')

    def Snapshot(self):
        with self.m_lock:
            rows = self.m_catalog.Rows()
            candidates = {}
            for row in rows:
                matches = [track['id'] for track in self.m_tracks
                           if (match := x_nightly.fullmatch(track['title'])) and match[1] == row['date']
                           and abs(track['duration_ms'] / 1000 - row['duration']) <= 1]
                candidates[row['id']] = matches
            result = []
            for row in rows:
                job = self.CurrentJob(row)
                matches = candidates[row['id']]
                candidate = matches[0] if len(matches) == 1 and sum(matches[0] in ids for ids in candidates.values()) == 1 else ''
                result.append({'id': row['id'], 'version': row['version'], 'filename': Path(row['path']).name,
                    'recorded_at': row['recorded_at'], 'date': row['date'], 'duration_seconds': row['duration'],
                    'size': row['size'], 'suggested_title': self.Title(row['date'], self.Slot(row['date'])),
                    'state': job['state'] if job else 'ready', 'title': job['title'] if job else '',
                    'permalink': job['permalink'] if job else '', 'job_id': job['job_id'] if job else '',
                    'error': job['error'] if job else '', 'progress': job['progress'] if job else 0.0,
                    'candidate_id': candidate})
            return {'available': True, 'account': dict(self.m_account), 'refreshing': self.m_refreshing,
                    'error': self.m_error, 'catalog_ready': self.m_ready, 'recordings': result,
                    'tracks': [dict(track) for track in self.m_tracks]}

    def Refresh(self):
        with self.m_lock:
            if not self.m_refreshing and not self.m_closed:
                self.m_refreshing = True
                self.m_refresh_worker.submit(self.RefreshWork)
        return self.Snapshot()

    def RefreshWork(self):
        try:
            with self.m_lock:
                self.m_catalog.Scan()
            if self.m_api is None:
                raise APIError('Connect SoundCloud on the Mac')
            account = self.m_api.Account()
            with self.m_lock:
                self.SetAccount(account)
            tracks = [Track(track) for track in self.m_api.Tracks()]
            if any(not track['id'] for track in tracks):
                raise ValueError('Invalid SoundCloud track catalog')
            tracks = list({track['id']: track for track in tracks}.values())
            with self.m_lock:
                self.SetAccount(account)
                self.m_tracks = tracks
                self.m_ready = True
                self.m_error = ''
                self.m_db.execute('INSERT OR REPLACE INTO accounts VALUES (?,?,1,?)',
                    (self.m_account['id'], self.m_account['name'], json.dumps(tracks)))
                self.m_db.commit()
        except Exception as failure:
            with self.m_lock:
                self.m_error = str(failure)
        finally:
            with self.m_lock:
                self.m_refreshing = False

    def RecordingAdded(self, path):
        with self.m_lock:
            relative = Path(path).relative_to(self.m_catalog.m_root).as_posix()
            SafePath(self.m_catalog.m_root, relative)
            self.m_catalog.Scan()

    def Validate(self, metadata):
        if not isinstance(metadata, dict):
            raise PublishingError('metadata must be an object')
        if any(not isinstance(metadata.get(field), str) or not 0 < len(metadata[field]) <= 256 for field in ('recording_id', 'version')):
            raise PublishingError('Invalid recording identity')
        row = self.m_db.execute('SELECT * FROM recordings WHERE id=? AND present=1', (metadata.get('recording_id'),)).fetchone()
        if row is None:
            raise PublishingError('Unknown recording', 404)
        try:
            path = SafePath(self.m_catalog.m_root, row['path'])
            current = Version(path)
        except (ValueError, OSError):
            raise PublishingError('Recording unavailable or changed', 409) from None
        if metadata.get('version') != row['version'] or current != row['version']:
            raise PublishingError('Recording version changed', 409)
        return row, path

    def Reply(self, job):
        return {field: job[field] for field in ('job_id', 'state', 'title', 'permalink', 'error')}

    def Submit(self, metadata, artwork, artwork_type):
        if not isinstance(metadata, dict):
            raise PublishingError('metadata must be an object')
        request = metadata.get('request_id')
        title = metadata.get('title')
        retry = metadata.get('retry_of')
        if not isinstance(request, str) or not 0 < len(request) <= 128 or (retry is not None and not isinstance(retry, str)):
            raise PublishingError('Invalid request identity')
        if title is not None and (not isinstance(title, str) or not title.strip() or len(title) > 256):
            raise PublishingError('Invalid title')
        if len(artwork) > x_max_artwork or (artwork and (artwork_type not in ('image/jpeg', 'image/png') or
            not (artwork.startswith(b'\x89PNG\r\n\x1a\n') if artwork_type == 'image/png' else artwork.startswith(b'\xff\xd8\xff')))):
            raise PublishingError('Artwork must be bounded JPEG or PNG')
        identity = json.dumps({'metadata': metadata, 'artwork': hashlib.sha256(artwork).hexdigest(), 'type': artwork_type}, sort_keys=True)
        with self.m_lock:
            replay = self.m_db.execute('SELECT * FROM requests WHERE request_id=?', (request,)).fetchone()
            if replay:
                if replay['identity'] != identity or replay['account'] != self.m_account['id']:
                    raise PublishingError('Request identity conflict', 409)
                return self.Reply(self.m_db.execute('SELECT * FROM jobs WHERE job_id=?', (replay['job_id'],)).fetchone())
            row, _ = self.Validate(metadata)
            if not self.m_account['connected'] or self.m_api is None:
                raise PublishingError('Connect SoundCloud on the Mac', 503)
            if title is None and not self.m_ready:
                raise PublishingError('Refresh SoundCloud catalog before choosing a default title', 503)
            previous = self.CurrentJob(row)
            if previous:
                if previous['state'] == 'uploaded':
                    self.m_db.execute('INSERT INTO requests VALUES (?,?,?,?)', (request, identity, self.m_account['id'], previous['job_id']))
                    self.m_db.commit()
                    return self.Reply(previous)
                if previous['state'] in ('queued', 'uploading'):
                    raise PublishingError('Recording already has an active job', 409)
                if retry != previous['job_id'] or previous['state'] not in ('failed', 'unknown'):
                    raise PublishingError('Explicit retry of the previous attempt is required', 409)
            elif retry is not None:
                raise PublishingError('Invalid retry attempt', 409)
            if row['duration'] > 24 * 3600 or row['size'] > 4 * 1024 ** 3:
                raise PublishingError('Recording exceeds SoundCloud upload limits')
            slot = self.Slot(row['date'])
            actual_title = title.strip() if title is not None else self.Title(row['date'], slot)
            job_id = uuid.uuid4().hex
            staged = ''
            if artwork:
                directory = SafePath(self.m_catalog.m_root, '.soundcloud-artwork')
                directory.mkdir(mode=0o700, exist_ok=True)
                destination = directory / (job_id + ('.png' if artwork_type == 'image/png' else '.jpg'))
                with destination.open('xb') as output:
                    output.write(artwork)
                    output.flush()
                    os.fsync(output.fileno())
                descriptor = os.open(directory, os.O_RDONLY)
                try:
                    os.fsync(descriptor)
                finally:
                    os.close(descriptor)
                staged = destination.relative_to(self.m_catalog.m_root).as_posix()
            self.m_db.execute('''INSERT INTO jobs (job_id,request_id,identity,account,recording_id,version,date,title,slot,artwork,state,progress,error,track_id,permalink)
                                  VALUES (?,?,?,?,?,?,?,?,?,?,'queued',0,'','','')''',
                (job_id, request, identity, self.m_account['id'], row['id'], row['version'], row['date'], actual_title, slot, staged))
            self.m_db.execute('INSERT INTO requests VALUES (?,?,?,?)', (request, identity, self.m_account['id'], job_id))
            self.m_db.commit()
            job = self.m_db.execute('SELECT * FROM jobs WHERE job_id=?', (job_id,)).fetchone()
            self.m_worker.submit(self.Run, job_id)
            return self.Reply(job)

    def Run(self, job_id):
        transmitted = False
        try:
            with self.m_lock:
                job = self.m_db.execute('SELECT * FROM jobs WHERE job_id=?', (job_id,)).fetchone()
                if job['account'] != self.m_account['id'] or self.m_api is None:
                    return
                row, path = self.Validate({'recording_id': job['recording_id'], 'version': job['version']})
                artwork = SafePath(self.m_catalog.m_root, job['artwork']) if job['artwork'] else None
                self.m_db.execute("UPDATE jobs SET state='uploading' WHERE job_id=?", (job_id,))
                self.m_db.commit()
            transmitted = True
            result = Track(self.m_api.Upload(path, job['title'], artwork, lambda value: self.Progress(job_id, value)))
            if not result['id'] or not result['permalink']:
                raise APIError('Incomplete SoundCloud creation response', uncertain=True)
            with self.m_lock:
                self.m_db.execute("UPDATE jobs SET state='uploaded',progress=1,track_id=?,permalink=? WHERE job_id=?",
                                  (result['id'], result['permalink'], job_id))
                self.m_db.commit()
        except Exception as failure:
            uncertain = failure.uncertain if isinstance(failure, APIError) else transmitted
            with self.m_lock:
                self.m_db.execute('UPDATE jobs SET state=?,error=? WHERE job_id=?',
                                  ('unknown' if uncertain else 'failed', str(failure), job_id))
                self.m_db.commit()
        finally:
            with self.m_lock:
                row = self.m_db.execute('SELECT artwork,state FROM jobs WHERE job_id=?', (job_id,)).fetchone()
            if row and row['artwork'] and row['state'] in ('uploaded', 'failed', 'unknown'):
                try:
                    SafePath(self.m_catalog.m_root, row['artwork']).unlink(missing_ok=True)
                except (OSError, ValueError):
                    pass

    def Progress(self, job_id, value):
        with self.m_lock:
            self.m_db.execute('UPDATE jobs SET progress=? WHERE job_id=?', (max(0, min(0.99, value)), job_id))
            self.m_db.commit()

    def Link(self, metadata):
        with self.m_lock:
            row, _ = self.Validate(metadata)
            track = next((t for t in self.m_tracks if t['id'] == metadata.get('track_id')), None)
            if not self.m_account['connected'] or not self.m_ready or track is None:
                raise PublishingError('Select a track from the configured account catalog')
            current = self.CurrentJob(row)
            if current and current['state'] in ('queued', 'uploading'):
                raise PublishingError('Recording already has an active job', 409)
            if current and current['state'] == 'uploaded':
                if current['track_id'] != track['id']:
                    raise PublishingError('Recording already linked', 409)
                return self.Reply(current)
            job_id = uuid.uuid4().hex
            self.m_db.execute('''INSERT INTO jobs (job_id,request_id,identity,account,recording_id,version,date,title,slot,artwork,state,progress,error,track_id,permalink)
                                  VALUES (?,NULL,'',?,?,?,?,?,NULL,'','uploaded',1,'',?,?)''',
                              (job_id, self.m_account['id'], row['id'], row['version'], row['date'], track['title'], track['id'], track['permalink']))
            if current and current['state'] == 'unknown':
                self.m_db.execute("UPDATE jobs SET state='failed',error='Resolved by explicit historical link' WHERE job_id=?", (current['job_id'],))
            self.m_db.commit()
            return self.Reply(self.m_db.execute('SELECT * FROM jobs WHERE job_id=?', (job_id,)).fetchone())

    def Close(self):
        with self.m_lock:
            self.m_closed = True
        self.m_refresh_worker.shutdown(wait=True)
        self.m_worker.shutdown(wait=True)
        self.m_db.close()
