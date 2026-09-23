import hashlib
import http.client
import io
import json
import socket
import ssl
import stat
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import wave
from pathlib import Path
from urllib.parse import quote


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sync_receiver import _identity, create_server, enable_tls


class IdentityTests(unittest.TestCase):
    def test_private_key_code_and_identity_have_private_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            identity = Path(directory) / 'identity'
            key, certificate, code, identifier, fingerprint = _identity(identity)
            self.assertTrue(certificate.exists())
            self.assertTrue(code)
            self.assertTrue(identifier)
            self.assertEqual(len(fingerprint), 64)
            for path in (identity, key, identity / 'access-code', identity / 'receiver-id'):
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o700 if path == identity else 0o600)

    def test_idle_tcp_client_does_not_block_tls_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            key, certificate, _, _, _ = _identity(root / 'identity')
            server = create_server('127.0.0.1', 0, root / 'files', 'secret')
            enable_tls(server, certificate, key)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            idle = socket.create_connection(('127.0.0.1', server.server_port))
            try:
                connection = http.client.HTTPSConnection('127.0.0.1', server.server_port,
                    context=ssl._create_unverified_context(), timeout=2)
                connection.request('GET', '/v1/manifest', headers={'Authorization': 'Bearer secret'})
                response = connection.getresponse()
                self.assertEqual(response.status, 200)
                self.assertEqual(json.loads(response.read()), {'version': 1, 'patches': []})
                connection.close()
            finally:
                idle.close()
                server.shutdown()
                server.server_close()
                thread.join(timeout=3)


class ReceiverTests(unittest.TestCase):
    def setUp(self):
        self.m_directory = tempfile.TemporaryDirectory()
        self.m_root = Path(self.m_directory.name)
        self.m_server = create_server('127.0.0.1', 0, self.m_root, 'secret')
        self.m_thread = threading.Thread(target=self.m_server.serve_forever, daemon=True)
        self.m_thread.start()

    def tearDown(self):
        if self.m_server is not None:
            self.m_server.shutdown()
            self.m_server.server_close()
            self.m_thread.join(timeout=3)
        self.m_directory.cleanup()

    def Request(self, method, route, body=None, headers=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.m_server.server_port, timeout=5)
        request_headers = {'Authorization': 'Bearer secret'}
        request_headers.update(headers or {})
        connection.request(method, route, body=body, headers=request_headers)
        response = connection.getresponse()
        payload = response.read()
        status = response.status
        connection.close()
        return status, payload

    def Put(self, kind, name, body, digest=None):
        return self.Request('PUT', f'/v1/{kind}?path={quote(name)}', body, {
            'Content-Length': str(len(body)),
            'X-SHA256': digest or hashlib.sha256(body).hexdigest(),
        })

    def test_manifest_requires_auth_and_reports_patch_hash(self):
        patch = self.m_root / 'patches' / 'sub' / 'tone.json'
        patch.parent.mkdir(parents=True)
        patch.write_bytes(b'abc')
        connection = http.client.HTTPConnection('127.0.0.1', self.m_server.server_port)
        connection.request('GET', '/v1/manifest')
        self.assertEqual(connection.getresponse().status, 401)
        connection.close()
        status, body = self.Request('GET', '/v1/manifest')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {'version': 1, 'patches': [{
            'path': 'sub/tone.json', 'size': 3,
            'sha256': hashlib.sha256(b'abc').hexdigest(),
        }]})

    def test_patch_upload_download_and_conflict(self):
        self.assertEqual(self.Request('GET', '/v1/patch?path=missing/tone.json')[0], 404)
        self.assertEqual(self.Put('patch', 'sub/tone.json', b'abc')[0], 200)
        self.assertEqual(self.Request('GET', '/v1/patch?path=sub/tone.json'), (200, b'abc'))
        self.assertEqual(self.Put('patch', 'sub/tone.json', b'changed')[0], 409)
        self.assertEqual((self.m_root / 'patches/sub/tone.json').read_bytes(), b'abc')

    def test_larger_log_replaces_smaller_log(self):
        self.assertEqual(self.Put('log', 'session.txt', b'abc')[0], 200)
        self.assertEqual(self.Put('log', 'session.txt', b'abcdef')[0], 200)
        status, payload = self.Put('log', 'session.txt', b'a')
        self.assertEqual((status, json.loads(payload)['state']), (200, 'skipped'))
        self.assertEqual((self.m_root / 'logs/session.txt').read_bytes(), b'abcdef')

    def test_wrong_hash_and_unsafe_path_leave_no_file(self):
        self.assertEqual(self.Put('patch', 'tone.json', b'abc', '0' * 64)[0], 400)
        self.assertFalse((self.m_root / 'patches/tone.json').exists())
        self.assertEqual(self.Put('patch', '../escape', b'abc')[0], 400)
        outside = self.m_root / 'outside'
        outside.mkdir()
        (self.m_root / 'patches').mkdir(exist_ok=True)
        (self.m_root / 'patches/link').symlink_to(outside, target_is_directory=True)
        self.assertEqual(self.Put('patch', 'link/escape', b'abc')[0], 400)
        self.assertFalse((outside / 'escape').exists())

    def test_interrupted_body_does_not_publish(self):
        raw = socket.create_connection(('127.0.0.1', self.m_server.server_port))
        raw.sendall(b'PUT /v1/patch?path=partial.json HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer secret\r\nContent-Length: 100\r\nX-SHA256: ' + b'0' * 64 + b'\r\n\r\nabc')
        raw.shutdown(socket.SHUT_WR)
        raw.close()
        time.sleep(0.1)
        self.assertFalse((self.m_root / 'patches/partial.json').exists())
        self.assertEqual(list((self.m_root / 'patches').glob('.*.partial-*')), [])

    def test_shutdown_drains_partial_upload(self):
        raw = socket.create_connection(('127.0.0.1', self.m_server.server_port))
        raw.sendall(b'PUT /v1/patch?path=shutdown.json HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer secret\r\nContent-Length: 1000000\r\nX-SHA256: ' + b'0' * 64 + b'\r\n\r\nabc')
        time.sleep(0.05)
        self.m_server.shutdown()
        self.m_server.server_close()
        self.m_thread.join(timeout=3)
        self.m_server = None
        raw.close()
        self.assertFalse((self.m_root / 'patches/shutdown.json').exists())
        self.assertEqual(list((self.m_root / 'patches').glob('.*.partial-*')), [])

    def test_recording_receipt_and_filename_conflict(self):
        fixture = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording/golden.sgrec'
        body = fixture.read_bytes()
        digest = hashlib.sha256(body).hexdigest()
        status, payload = self.Put('recording', 'take.sgrec', body)
        self.assertEqual(status, 200)
        self.assertIn(json.loads(payload)['state'], ('extracting', 'complete'))
        route = f'/v1/recording?path=take.sgrec&sha256={digest}'
        for _ in range(100):
            status, payload = self.Request('GET', route)
            receipt = json.loads(payload)
            if receipt['state'] != 'extracting':
                break
            time.sleep(0.03)
        self.assertEqual((status, receipt['state'], receipt['sha256']), (200, 'complete', digest))
        self.assertEqual((self.m_root / 'recordings/take.sgrec').read_bytes(), body)
        self.assertTrue((self.m_root / 'recordings/take_stereo.wav').exists())
        repeated_status, repeated_payload = self.Put('recording', 'take.sgrec', body)
        self.assertEqual(repeated_status, 200)
        self.assertEqual(json.loads(repeated_payload)['state'], 'complete')
        self.assertEqual(self.Put('recording', 'take.sgrec', b'other')[0], 409)

    def test_legacy_multichannel_wav_extracts_stereo_with_sox(self):
        audio = io.BytesIO()
        with wave.open(audio, 'wb') as output:
            output.setnchannels(4)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(struct.pack('<8h', 1, 2, 3, 4, 5, 6, 7, 8))
        body = audio.getvalue()
        digest = hashlib.sha256(body).hexdigest()
        self.assertEqual(self.Put('recording', 'legacy.wav', body)[0], 200)
        route = f'/v1/recording?path=legacy.wav&sha256={digest}'
        for _ in range(100):
            _, payload = self.Request('GET', route)
            if json.loads(payload)['state'] != 'extracting':
                break
            time.sleep(0.03)
        self.assertEqual(json.loads(payload)['state'], 'complete')
        stereo = self.m_root / 'recordings/legacy_stereo.wav'
        self.assertEqual(subprocess.check_output(['soxi', '-c', str(stereo)], text=True).strip(), '2')

    def test_incomplete_recording_is_retained_with_failed_receipt(self):
        fixture = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording/golden.sgrec'
        body = fixture.read_bytes()[:-16]
        digest = hashlib.sha256(body).hexdigest()
        self.assertEqual(self.Put('recording', 'bad.sgrec', body)[0], 200)
        for _ in range(100):
            status, payload = self.Request('GET', f'/v1/recording?path=bad.sgrec&sha256={digest}')
            receipt = json.loads(payload)
            if receipt['state'] != 'extracting':
                break
            time.sleep(0.03)
        self.assertEqual(receipt['state'], 'failed')
        self.assertEqual((self.m_root / 'recordings/bad.sgrec').read_bytes(), body)

    def test_existing_identical_recording_can_resume_after_server_restart(self):
        fixture = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording/golden.sgrec'
        body = fixture.read_bytes()
        digest = hashlib.sha256(body).hexdigest()
        destination = self.m_root / 'recordings/restart.sgrec'
        destination.parent.mkdir()
        destination.write_bytes(body)
        status, payload = self.Put('recording', 'restart.sgrec', body)
        self.assertEqual(status, 200)
        self.assertIn(json.loads(payload)['state'], ('extracting', 'complete'))
        for _ in range(100):
            status, payload = self.Request('GET', f'/v1/recording?path=restart.sgrec&sha256={digest}')
            if json.loads(payload)['state'] != 'extracting':
                break
            time.sleep(0.03)
        self.assertEqual(json.loads(payload)['state'], 'complete')

    def test_complete_receipt_fails_if_stereo_output_changes(self):
        fixture = Path(__file__).resolve().parents[2] / 'private/test/fixtures/streaming-recording/golden.sgrec'
        body = fixture.read_bytes()
        digest = hashlib.sha256(body).hexdigest()
        self.assertEqual(self.Put('recording', 'stale.sgrec', body)[0], 200)
        route = f'/v1/recording?path=stale.sgrec&sha256={digest}'
        for _ in range(100):
            _, payload = self.Request('GET', route)
            if json.loads(payload)['state'] != 'extracting':
                break
            time.sleep(0.03)
        self.assertEqual(json.loads(payload)['state'], 'complete')
        stereo = self.m_root / 'recordings/stale_stereo.wav'
        original_stereo = stereo.read_bytes()
        stereo.write_bytes(b'changed')
        _, payload = self.Request('GET', route)
        self.assertEqual(json.loads(payload)['state'], 'failed')
        _, payload = self.Put('recording', 'stale.sgrec', body)
        self.assertEqual(json.loads(payload)['state'], 'failed')
        stereo.write_bytes(original_stereo)
        (self.m_root / 'recordings/stale.sgrec').write_bytes(b'changed')
        _, payload = self.Request('GET', route)
        self.assertEqual(json.loads(payload)['state'], 'failed')


if __name__ == '__main__':
    unittest.main()
