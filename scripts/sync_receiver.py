#!/usr/bin/env python3
"""Explicitly launched HTTPS receiver for page-scoped SmartGrid sync."""

import argparse
import hashlib
import hmac
import json
import os
import secrets
import socket
import ssl
import subprocess
import threading
import uuid
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from recording_export import extract_stereo_recording


x_chunk_size = 1024 * 1024
x_max_file_size = 64 * 1024 * 1024 * 1024


def _digest(path):
    hash_value = hashlib.sha256()
    with path.open('rb') as source:
        while chunk := source.read(x_chunk_size):
            hash_value.update(chunk)
    return hash_value.hexdigest()


def _flush_directory(path):
    descriptor = os.open(path, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _safe_path(root, relative, create=False):
    if not relative or '\\' in relative or '\x00' in relative or ':' in relative:
        raise ValueError('unsafe path')
    pieces = relative.split('/')
    if any(piece in ('', '.', '..') for piece in pieces):
        raise ValueError('unsafe path')
    root = Path(root)
    if root.is_symlink():
        raise ValueError('symlink directory')
    if create:
        root.mkdir(parents=True, exist_ok=True)
    current = root
    for piece in pieces[:-1]:
        current = current / piece
        if current.is_symlink():
            raise ValueError('symlink directory')
        if create:
            current.mkdir(exist_ok=True)
        if current.exists() and not current.is_dir():
            raise ValueError('non-directory path')
    destination = current / pieces[-1]
    if destination.is_symlink():
        raise ValueError('symlink file')
    return destination


class SyncServer(ThreadingHTTPServer):
    daemon_threads = False

    def __init__(self, address, root, token):
        self.m_root = Path(root)
        self.m_token = token
        self.m_receipts = {}
        self.m_output_hashes = {}
        self.m_lock = threading.Lock()
        self.m_connections = set()
        self.m_closing = False
        self.m_worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix='sg-extract')
        super().__init__(address, SyncHandler)

    def process_request(self, request, client_address):
        with self.m_lock:
            if self.m_closing:
                self.shutdown_request(request)
                return
            self.m_connections.add(request)
        request.settimeout(5)
        try:
            super().process_request(request, client_address)
        except BaseException:
            with self.m_lock:
                self.m_connections.discard(request)
            raise

    def process_request_thread(self, request, client_address):
        try:
            if isinstance(request, ssl.SSLSocket):
                request.settimeout(2)
                request.do_handshake()
                request.settimeout(5)
            super().process_request_thread(request, client_address)
        except OSError:
            self.shutdown_request(request)
        finally:
            with self.m_lock:
                self.m_connections.discard(request)

    def server_close(self):
        with self.m_lock:
            self.m_closing = True
            connections = list(self.m_connections)
        for connection in connections:
            try:
                connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            connection.close()
        super().server_close()
        self.m_worker.shutdown(wait=True, cancel_futures=True)

    def VerifiedReceipt(self, relative, digest):
        with self.m_lock:
            receipt = self.m_receipts.get((relative, digest))
            output_hash = self.m_output_hashes.get((relative, digest))
        if receipt is None:
            return None
        if receipt['state'] != 'complete':
            return receipt
        try:
            original = _safe_path(self.m_root / 'recordings', relative)
            output = original.with_stem(original.stem + '_stereo').with_suffix('.wav')
            if output.is_symlink() or _digest(original) != digest or _digest(output) != output_hash:
                raise ValueError('recording or stereo output changed')
        except (OSError, ValueError):
            return {'state': 'failed', 'sha256': digest, 'error': 'recording or stereo output changed'}
        return receipt

    def Extract(self, path, digest, relative):
        output = path.with_stem(path.stem + '_stereo').with_suffix('.wav')
        temporary = output.with_name(f'.{output.name}.partial-{uuid.uuid4().hex}')
        error = None
        output_hash = None
        try:
            if output.is_symlink():
                raise FileExistsError(f'stereo output already exists: {output.name}')
            if extract_stereo_recording(path, temporary) != 0:
                raise ValueError('recording is incomplete')
            with temporary.open('rb') as source:
                os.fsync(source.fileno())
            if output.exists():
                if _digest(output) != _digest(temporary):
                    raise FileExistsError(f'stereo output conflicts: {output.name}')
                with output.open('rb') as existing:
                    os.fsync(existing.fileno())
            else:
                os.link(temporary, output)
            _flush_directory(output.parent)
            output_hash = _digest(output)
        except Exception as failure:
            error = str(failure)
        finally:
            temporary.unlink(missing_ok=True)
        with self.m_lock:
            if not error:
                self.m_output_hashes[(relative, digest)] = output_hash
            self.m_receipts[(relative, digest)] = (
                {'state': 'failed', 'sha256': digest, 'error': error} if error else
                {'state': 'complete', 'sha256': digest}
            )


class SyncHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def handle(self):
        try:
            super().handle()
        except OSError:
            pass

    def finish(self):
        try:
            super().finish()
        except OSError:
            pass
        finally:
            with self.server.m_lock:
                self.server.m_connections.discard(self.connection)

    def log_message(self, format_string, *args):
        pass

    def _json(self, status, data):
        payload = json.dumps(data, separators=(',', ':')).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _route(self):
        parsed = urlsplit(self.path)
        query = parse_qs(parsed.query, keep_blank_values=True)
        return parsed.path, query

    def _authorized(self):
        provided = self.headers.get('Authorization', '')
        if not hmac.compare_digest(provided, f'Bearer {self.server.m_token}'):
            self._json(401, {'error': 'unauthorized'})
            return False
        return True

    def _relative(self, query):
        values = query.get('path', [])
        if len(values) != 1:
            raise ValueError('one path is required')
        return values[0]

    def do_GET(self):
        if not self._authorized():
            return
        route, query = self._route()
        try:
            if route == '/v1/manifest':
                patches = []
                root = self.server.m_root / 'patches'
                if root.exists():
                    if root.is_symlink():
                        raise ValueError('symlink directory')
                    for path in sorted(root.rglob('*')):
                        if path.is_symlink():
                            raise ValueError('symlink patch')
                        if path.is_file():
                            patches.append({'path': path.relative_to(root).as_posix(),
                                            'size': path.stat().st_size, 'sha256': _digest(path)})
                return self._json(200, {'version': 1, 'patches': patches})
            if route == '/v1/patch':
                path = _safe_path(self.server.m_root / 'patches', self._relative(query))
                if not path.is_file():
                    return self._json(404, {'error': 'missing patch'})
                size = path.stat().st_size
                self.send_response(200)
                self.send_header('Content-Type', 'application/octet-stream')
                self.send_header('Content-Length', str(size))
                self.end_headers()
                with path.open('rb') as source:
                    while chunk := source.read(x_chunk_size):
                        self.wfile.write(chunk)
                return
            if route == '/v1/recording':
                relative = self._relative(query)
                _safe_path(self.server.m_root / 'recordings', relative)
                digest = query.get('sha256', [''])[0]
                if len(digest) != 64 or any(char not in '0123456789abcdef' for char in digest):
                    raise ValueError('invalid SHA-256')
                receipt = self.server.VerifiedReceipt(relative, digest)
                return self._json(200, receipt or {'state': 'failed', 'sha256': digest,
                                                  'error': 'no matching upload'})
            return self._json(404, {'error': 'unknown endpoint'})
        except (ValueError, OSError) as failure:
            return self._json(400, {'error': str(failure)})

    def do_PUT(self):
        self.close_connection = True
        if not self._authorized():
            return
        route, query = self._route()
        if route not in ('/v1/patch', '/v1/log', '/v1/recording'):
            return self._json(404, {'error': 'unknown endpoint'})
        try:
            relative = self._relative(query)
            length_text = self.headers.get('Content-Length', '')
            if not length_text.isascii() or not length_text.isdecimal():
                raise ValueError('invalid Content-Length')
            length = int(length_text)
            if length > x_max_file_size:
                raise ValueError('file too large')
            digest = self.headers.get('X-SHA256', '')
            if len(digest) != 64 or any(char not in '0123456789abcdef' for char in digest):
                raise ValueError('invalid SHA-256')
            folder = {'/v1/patch': 'patches', '/v1/log': 'logs',
                      '/v1/recording': 'recordings'}[route]
            destination = _safe_path(self.server.m_root / folder, relative, create=True)
            if destination.exists():
                if route != '/v1/log':
                    if route == '/v1/recording' and _digest(destination) == digest:
                        with self.server.m_lock:
                            receipt = self.server.m_receipts.get((relative, digest))
                            if receipt is None:
                                receipt = {'state': 'extracting', 'sha256': digest}
                                self.server.m_receipts[(relative, digest)] = receipt
                                self.server.m_worker.submit(self.server.Extract, destination, digest, relative)
                        if receipt is not None:
                            self.close_connection = True
                            return self._json(200, self.server.VerifiedReceipt(relative, digest))
                    self.close_connection = True
                    return self._json(409, {'error': 'filename conflict'})
                if destination.stat().st_size >= length:
                    self.close_connection = True
                    return self._json(200, {'state': 'skipped'})
            temporary = destination.with_name(f'.{destination.name}.partial-{uuid.uuid4().hex}')
            self.connection.settimeout(5)
            try:
                actual = hashlib.sha256()
                remaining = length
                with temporary.open('xb') as output:
                    while remaining:
                        chunk = self.rfile.read(min(x_chunk_size, remaining))
                        if not chunk:
                            raise ValueError('incomplete request body')
                        output.write(chunk)
                        actual.update(chunk)
                        remaining -= len(chunk)
                    output.flush()
                    os.fsync(output.fileno())
                if actual.hexdigest() != digest:
                    raise ValueError('SHA-256 mismatch')
                _safe_path(self.server.m_root / folder, relative)
                if route == '/v1/log':
                    os.replace(temporary, destination)
                else:
                    os.link(temporary, destination)
                    temporary.unlink()
                _flush_directory(destination.parent)
            finally:
                temporary.unlink(missing_ok=True)
            if route == '/v1/recording':
                with self.server.m_lock:
                    self.server.m_receipts[(relative, digest)] = {'state': 'extracting', 'sha256': digest}
                self.server.m_worker.submit(self.server.Extract, destination, digest, relative)
                return self._json(200, {'state': 'extracting', 'sha256': digest})
            return self._json(200, {'sha256': digest})
        except FileExistsError:
            return self._json(409, {'error': 'filename conflict'})
        except (ValueError, OSError) as failure:
            return self._json(400, {'error': str(failure)})


def create_server(host, port, root, token):
    return SyncServer((host, port), root, token)


def enable_tls(server, certificate, key):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(certificate), str(key))
    server.socket = context.wrap_socket(server.socket, server_side=True,
                                        do_handshake_on_connect=False)


def _identity(directory):
    if directory.is_symlink():
        raise ValueError('identity directory is a symlink')
    directory.mkdir(parents=True, mode=0o700, exist_ok=True)
    directory.chmod(0o700)
    key = directory / 'receiver.key'
    certificate = directory / 'receiver.crt'
    code = directory / 'access-code'
    identifier = directory / 'receiver-id'
    if any(path.is_symlink() for path in (key, certificate, code, identifier)):
        raise ValueError('identity file is a symlink')
    if not key.exists() or not certificate.exists():
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:3072', '-nodes',
                        '-keyout', str(key), '-out', str(certificate), '-days', '3650',
                        '-subj', '/CN=SmartGridOne Sync'], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    key.chmod(0o600)
    if not code.exists():
        with os.fdopen(os.open(code, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'w') as output:
            output.write(secrets.token_urlsafe(24) + '\n')
    code.chmod(0o600)
    if not identifier.exists():
        with os.fdopen(os.open(identifier, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'w') as output:
            output.write(str(uuid.uuid4()) + '\n')
    identifier.chmod(0o600)
    der = ssl.PEM_cert_to_DER_cert(certificate.read_text())
    return key, certificate, code.read_text().strip(), identifier.read_text().strip(), hashlib.sha256(der).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', default=socket.gethostname().split('.')[0][:40] + ' SmartGridOne')
    parser.add_argument('--host', default='0.0.0.0')
    parser.add_argument('--port', type=int, default=47658)
    parser.add_argument('--root', type=Path, default=Path.home() / 'Documents/SmartGridOne')
    parser.add_argument('--identity-dir', type=Path, default=Path.home() / '.config/smartgridone/sync')
    args = parser.parse_args()
    key, certificate, token, identifier, fingerprint = _identity(args.identity_dir)
    server = create_server(args.host, args.port, args.root, token)
    enable_tls(server, certificate, key)
    advertisement = subprocess.Popen(['dns-sd', '-R', args.name, '_sgsync._tcp.',
                                      'local.', str(server.server_port), 'version=1',
                                      f'id={identifier}', f'fingerprint={fingerprint}'],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'Listening on port {server.server_port}', flush=True)
    print(f'Certificate SHA-256: {fingerprint}', flush=True)
    print(f'Access code: {token}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        advertisement.terminate()
        try:
            advertisement.wait(timeout=3)
        except subprocess.TimeoutExpired:
            advertisement.kill()
            advertisement.wait()


if __name__ == '__main__':
    main()
