import importlib.util
import http.client
import json
import sys
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
if importlib.util.find_spec('soundcloud_api'):
    from soundcloud_api import SoundCloudAPI, APIError, HTTPTransport, x_error_body_limit
    from soundcloud_connect import ConnectionFlow
else:
    SoundCloudAPI = None


class MemoryStorage:
    def __init__(self):
        self.value = {'client_id': 'client', 'client_secret': 'secret', 'access_token': 'old',
                      'refresh_token': 'refresh-one', 'expires_at': 9999999999,
                      'account': {'id': 'owner', 'name': 'Owner'}}
        self.saved = []

    def Load(self):
        return dict(self.value)

    def Save(self, value):
        self.value = dict(value)
        self.saved.append(dict(value))


class BoundaryTransport:
    def __init__(self, responses):
        self.responses = list(responses)
        self.requests = []

    def Request(self, method, url, headers, body):
        content = b''.join(body) if body is not None else b''
        self.requests.append((method, url, headers, content))
        result = self.responses.pop(0)
        if isinstance(result, Exception):
            raise result
        return result


class APITests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(SoundCloudAPI, 'Official API connection is not implemented')
        self.storage = MemoryStorage()

    def test_streamed_creation_has_official_fields_and_only_one_request(self):
        transport = BoundaryTransport([(201, {'urn': 'soundcloud:tracks:123', 'permalink_url': 'https://soundcloud.com/test/track'})])
        api = SoundCloudAPI(self.storage, transport)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'stereo.wav'
            path.write_bytes(b'audio bytes')
            result = api.Upload(path, 'Custom Title', None, lambda p: None)
        self.assertEqual(result['urn'], 'soundcloud:tracks:123')
        self.assertEqual(len(transport.requests), 1)
        method, url, headers, body = transport.requests[0]
        self.assertEqual((method, url), ('POST', 'https://api.soundcloud.com/tracks'))
        self.assertEqual(headers['Authorization'], 'OAuth old')
        for field, value in [(b'track[title]', b'Custom Title'), (b'track[genre]', b'Electronic'), (b'track[sharing]', b'public'), (b'track[asset_data]', b'audio bytes')]:
            self.assertIn(b'name="' + field + b'"', body)
            self.assertIn(value, body)
        self.assertNotIn(b'track[artwork_data]', body)
        self.assertEqual(int(headers['Content-Length']), len(body))

    def test_artwork_and_paginated_tracks_reject_foreign_origin(self):
        transport = BoundaryTransport([(200, {'collection': [{'id': '1'}], 'next_href': 'https://api.soundcloud.com/me/tracks?cursor=2'}),
                                       (200, {'collection': [{'id': '2'}]})])
        api = SoundCloudAPI(self.storage, transport)
        self.assertEqual([t['id'] for t in api.Tracks()], ['1', '2'])
        transport.responses = [(200, {'collection': [], 'next_href': 'https://attacker.invalid/tracks'})]
        with self.assertRaises(APIError):
            api.Tracks()
        self.assertEqual(len(transport.requests), 3)
        transport.responses = [(201, {'id': '3', 'permalink_url': 'https://soundcloud.com/test/three'})]
        with tempfile.TemporaryDirectory() as directory:
            audio, artwork = Path(directory) / 'audio.wav', Path(directory) / 'art.png'
            audio.write_bytes(b'audio')
            artwork.write_bytes(b'\x89PNG\r\n\x1a\nimage')
            api.Upload(audio, 'Title', artwork, lambda p: None)
        self.assertIn(b'name="track[artwork_data]"', transport.requests[-1][3])
        self.assertIn(b'Content-Type: image/png', transport.requests[-1][3])

    def test_refresh_rotates_and_saves_before_authenticated_use(self):
        self.storage.value['expires_at'] = 0
        transport = BoundaryTransport([(200, {'access_token': 'new', 'refresh_token': 'refresh-two', 'expires_in': 3600, 'scope': 'non-expiring'}),
                                       (200, {'id': 'owner', 'username': 'Owner'}), (200, {'id': 'owner', 'username': 'Owner'})])
        api = SoundCloudAPI(self.storage, transport)
        threads = [threading.Thread(target=api.Account) for _ in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(self.storage.value['refresh_token'], 'refresh-two')
        self.assertEqual(self.storage.saved[0]['scope'], 'non-expiring')
        self.assertEqual(len([r for r in transport.requests if 'oauth/token' in r[1]]), 1)
        self.assertEqual(parse_qs(transport.requests[0][3].decode())['refresh_token'], ['refresh-one'])
        self.assertTrue(all(r[2]['Authorization'] == 'OAuth new' for r in transport.requests[1:]))

    def test_upload_rejection_and_lost_response_are_classified_without_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'audio.wav'
            path.write_bytes(b'audio')
            for response, uncertain in [((422, {'error': 'bad'}), False), (OSError('connection lost'), True), ((504, {}), True), ((201, {}), True)]:
                transport = BoundaryTransport([response])
                api = SoundCloudAPI(self.storage, transport)
                with self.assertRaises(APIError) as failure:
                    api.Upload(path, 'Title', None, lambda p: None)
                self.assertEqual(failure.exception.uncertain, uncertain)
                self.assertEqual(len(transport.requests), 1)

    def test_http_rejection_status_is_classified_before_receipt_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'audio.wav'
            path.write_bytes(b'audio')
            for status, payload, uncertain in [(422, b'', False), (401, b'<html>Unauthorized</html>', False), (503, b'<html>Unavailable</html>', True)]:
                with self.subTest(status=status):
                    response = Mock(status=status)
                    response.read.return_value = payload
                    connection = Mock()
                    connection.getresponse.return_value = response
                    api = SoundCloudAPI(self.storage, HTTPTransport())
                    with patch('soundcloud_api.http.client.HTTPSConnection', return_value=connection):
                        with self.assertRaises(APIError) as failure:
                            api.Upload(path, 'Title', None, lambda p: None)
                    self.assertEqual(failure.exception.uncertain, uncertain)
                    self.assertIn('HTTP ' + str(status), str(failure.exception))

    def test_http_error_body_is_logged_and_retained_without_credentials(self):
        payloads = [json.dumps({'errors': [{'error_message': 'Track was created but response failed'}],
                               'message': 'access=old refresh=refresh-one secret=secret client=client'}).encode(),
                    b'Upstream error\naccess=old refresh=refresh-one secret=secret client=client\x1b']
        for payload in payloads:
            with self.subTest(payload=payload):
                response = Mock(status=400)
                response.read.return_value = payload
                connection = Mock()
                connection.getresponse.return_value = response
                api = SoundCloudAPI(self.storage, HTTPTransport())
                with patch('soundcloud_api.http.client.HTTPSConnection', return_value=connection):
                    with self.assertLogs('soundcloud_api', level='WARNING') as logs:
                        with self.assertRaises(APIError) as failure:
                            api.Request('POST', 'https://api.soundcloud.com/tracks')
                message = str(failure.exception)
                self.assertIn('HTTP 400', message)
                self.assertIn('Track was created' if payload.startswith(b'{') else 'Upstream error', message)
                self.assertIn('[redacted]', message)
                self.assertIn(message, logs.output[0])
                self.assertIn('POST /tracks', logs.output[0])
                self.assertEqual(len(logs.output), 1)
                self.assertNotIn('\n', message)
                self.assertNotIn('\x1b', message)
                for key in ('access_token', 'refresh_token', 'client_secret', 'client_id'):
                    self.assertNotIn('='+self.storage.value[key], message)
                connection.request.assert_called_once()
                connection.close.assert_called_once()

    def test_http_error_body_is_bounded_and_marks_truncation(self):
        response = Mock(status=400)
        response.read.return_value = b'x' * (x_error_body_limit + 1)
        connection = Mock()
        connection.getresponse.return_value = response
        with patch('soundcloud_api.http.client.HTTPSConnection', return_value=connection):
            status, result = HTTPTransport().Request('POST', 'https://api.soundcloud.com/tracks', {}, None)
        self.assertEqual(status, 400)
        self.assertEqual(result['message'], 'x' * x_error_body_limit)
        self.assertTrue(result['truncated'])
        response.read.assert_called_once_with(x_error_body_limit + 1)

    def test_unreadable_error_body_keeps_http_status_and_outcome(self):
        for status, uncertain in ((400, False), (503, True)):
            for error in (TimeoutError(), http.client.IncompleteRead(b'partial')):
                with self.subTest(status=status, error=type(error).__name__):
                    response = Mock(status=status)
                    response.read.side_effect = error
                    connection = Mock()
                    connection.getresponse.return_value = response
                    api = SoundCloudAPI(self.storage, HTTPTransport())
                    with patch('soundcloud_api.http.client.HTTPSConnection', return_value=connection):
                        with self.assertLogs('soundcloud_api', level='WARNING'):
                            with self.assertRaises(APIError) as failure:
                                api.Request('POST', 'https://api.soundcloud.com/tracks')
                    self.assertIn('HTTP ' + str(status), str(failure.exception))
                    self.assertIn('Could not read', str(failure.exception))
                    self.assertEqual(failure.exception.uncertain, uncertain)

    def test_receiver_loads_saved_authorization_only_once(self):
        from soundcloud_connect import LoadSavedAPI
        class CountingStorage(MemoryStorage):
            def __init__(self):
                super().__init__()
                self.loads = 0

            def Load(self):
                self.loads += 1
                return super().Load()

        storage = CountingStorage()
        with patch('soundcloud_connect.KeychainStorage', return_value=storage):
            api = LoadSavedAPI()
        self.assertIsInstance(api, SoundCloudAPI)
        self.assertEqual(storage.loads, 1)

    def test_refresh_storage_failure_blocks_consumed_token_reuse(self):
        class FailingStorage(MemoryStorage):
            def Save(self, value):
                raise OSError('Keychain unavailable')

        storage = FailingStorage()
        storage.value['expires_at'] = 0
        transport = BoundaryTransport([(200, {'access_token': 'new', 'refresh_token': 'rotated', 'expires_in': 3600})])
        api = SoundCloudAPI(storage, transport)
        with self.assertRaises(APIError):
            api.Account()
        with self.assertRaises(APIError):
            api.Account()
        self.assertEqual(len(transport.requests), 1)

    def test_setup_pkce_state_and_verified_account_are_stored(self):
        transport = BoundaryTransport([(200, {'access_token': 'connected', 'refresh_token': 'r', 'expires_in': 3600}),
                                       (200, {'urn': 'soundcloud:users:owner', 'username': 'Verified'})])
        flow = ConnectionFlow('app', 'secret', 'http://127.0.0.1:47659/callback', self.storage, transport)
        url = flow.AuthorizationURL()
        query = parse_qs(urlsplit(url).query)
        self.assertEqual(query['code_challenge_method'], ['S256'])
        with self.assertRaises(ValueError):
            flow.Complete('/callback?code=code&state=wrong')
        self.assertEqual(transport.requests, [])
        flow.Complete('/callback?code=code&state=' + query['state'][0])
        self.assertEqual(self.storage.value['account'], {'id': 'soundcloud:users:owner', 'name': 'Verified'})
        payload = parse_qs(transport.requests[0][3].decode())
        self.assertIn('code_verifier', payload)
        self.assertEqual(payload['redirect_uri'], ['http://127.0.0.1:47659/callback'])


if __name__ == '__main__':
    unittest.main()
