"""Official SoundCloud API boundary with streamed creation and rotating tokens."""

import http.client
import json
import logging
import mimetypes
import secrets
import threading
import time
from pathlib import Path
from urllib.parse import urlencode, urlsplit


x_api_origin = 'https://api.soundcloud.com'
x_token_url = 'https://secure.soundcloud.com/oauth/token'
x_chunk_size = 256 * 1024
x_error_body_limit = 16 * 1024


class APIError(Exception):
    def __init__(self, message, uncertain=False):
        super().__init__(message)
        self.uncertain = uncertain


class HTTPTransport:
    def Request(self, method, url, headers, body):
        parsed = urlsplit(url)
        if parsed.scheme != 'https' or parsed.hostname not in ('api.soundcloud.com', 'secure.soundcloud.com') or parsed.port not in (None, 443) or parsed.username or parsed.password:
            raise APIError('Unsupported SoundCloud origin')
        connection = http.client.HTTPSConnection(parsed.hostname, timeout=120)
        try:
            try:
                connection.connect()
            except OSError:
                raise APIError('Could not connect to SoundCloud before transmission') from None
            if isinstance(body, (list, tuple)) and 'Content-Length' not in headers:
                headers = dict(headers, **{'Content-Length': str(sum(len(part) for part in body))})
            connection.request(method, parsed.path + ('?' + parsed.query if parsed.query else ''), body=body, headers=headers)
            response = connection.getresponse()
            if not 200 <= response.status < 300:
                try:
                    payload = response.read(x_error_body_limit + 1)
                except (OSError, http.client.HTTPException):
                    return response.status, {'message': 'Could not read SoundCloud error response'}
                if len(payload) > x_error_body_limit:
                    return response.status, {'message': payload[:x_error_body_limit].decode('utf-8', errors='replace'),
                                             'truncated': True}
                try:
                    return response.status, json.loads(payload)
                except (ValueError, UnicodeDecodeError):
                    return response.status, {'message': payload.decode('utf-8', errors='replace')} if payload else {}
            payload = response.read(4 * 1024 * 1024 + 1)
            if len(payload) > 4 * 1024 * 1024:
                raise APIError('SoundCloud response too large', uncertain=method == 'POST')
            try:
                result = json.loads(payload)
            except (ValueError, UnicodeDecodeError):
                raise APIError('Invalid SoundCloud response', uncertain=method == 'POST') from None
            return response.status, result
        finally:
            connection.close()


class Multipart:
    def __init__(self, path, title, artwork_path, progress):
        self.m_boundary = 'smartgrid-' + secrets.token_hex(24)
        self.m_parts = []
        self.m_progress = progress
        for name, value in (('track[title]', title), ('track[genre]', 'Electronic'), ('track[sharing]', 'public')):
            self.m_parts.append((self.Header(name) + value.encode('utf-8') + b'\r\n', None))
        self.m_parts.append((self.Header('track[asset_data]', 'recording.wav', 'audio/wav'), Path(path)))
        if artwork_path:
            artwork = Path(artwork_path)
            image_type = mimetypes.guess_type(str(artwork))[0] or 'image/jpeg'
            self.m_parts.append((self.Header('track[artwork_data]', 'artwork' + artwork.suffix, image_type), artwork))
        self.m_end = f'--{self.m_boundary}--\r\n'.encode()
        self.m_length = len(self.m_end) + sum(len(header) + (file.stat().st_size + 2 if file else 0) for header, file in self.m_parts)

    def Header(self, name, filename=None, content_type=None):
        header = f'--{self.m_boundary}\r\nContent-Disposition: form-data; name="{name}"'
        if filename:
            header += f'; filename="{filename}"'
        header += '\r\n'
        if content_type:
            header += f'Content-Type: {content_type}\r\n'
        return (header + '\r\n').encode()

    def __iter__(self):
        sent = 0
        for header, path in self.m_parts:
            yield header
            sent += len(header)
            if path:
                with path.open('rb') as source:
                    while chunk := source.read(x_chunk_size):
                        yield chunk
                        sent += len(chunk)
                        self.m_progress(min(0.99, sent / self.m_length))
                yield b'\r\n'
        yield self.m_end


class SoundCloudAPI:
    def __init__(self, storage, transport=None, credentials=None):
        self.m_storage = storage
        self.m_transport = transport or HTTPTransport()
        self.m_tokens = dict(credentials) if credentials is not None else storage.Load()
        self.m_lock = threading.Lock()
        self.m_refresh_error = False

    def Token(self):
        with self.m_lock:
            if not self.m_tokens or self.m_refresh_error:
                raise APIError('Connect SoundCloud on the Mac')
            if self.m_tokens.get('expires_at', 0) <= time.time() + 60:
                values = {key: self.m_tokens[key] for key in ('client_id', 'client_secret', 'refresh_token')}
                values['grant_type'] = 'refresh_token'
                try:
                    status, result = self.m_transport.Request('POST', x_token_url,
                        {'Content-Type': 'application/x-www-form-urlencoded', 'Accept': 'application/json'}, [urlencode(values).encode()])
                    if status != 200 or not result.get('access_token') or not result.get('refresh_token'):
                        raise APIError('SoundCloud authorization needs reconnecting')
                    tokens = dict(self.m_tokens)
                    tokens.update(result)
                    tokens['expires_at'] = time.time() + float(result['expires_in'])
                    self.m_storage.Save(tokens)
                    self.m_tokens = tokens
                except Exception:
                    self.m_refresh_error = True
                    raise APIError('SoundCloud token refresh failed; reconnect on the Mac') from None
            return self.m_tokens['access_token']

    def Request(self, method, url, body=None, headers=None):
        if urlsplit(url).scheme != 'https' or urlsplit(url).netloc != 'api.soundcloud.com':
            raise APIError('SoundCloud pagination left the API origin')
        outgoing = {'Accept': 'application/json', 'Authorization': 'OAuth ' + self.Token()}
        outgoing.update(headers or {})
        try:
            status, result = self.m_transport.Request(method, url, outgoing, body)
        except APIError:
            raise
        except Exception:
            raise APIError('SoundCloud connection failed', uncertain=method == 'POST') from None
        if not 200 <= status < 300:
            detail = json.dumps(result, ensure_ascii=True) if result else ''
            for key in ('access_token', 'refresh_token', 'client_secret', 'client_id'):
                secret = self.m_tokens.get(key)
                if secret:
                    detail = detail.replace(json.dumps(secret, ensure_ascii=True)[1:-1], '[redacted]')
            message = f'SoundCloud rejected request (HTTP {status})' + (': ' + detail if detail else '')
            logging.getLogger(__name__).warning('%s %s: %s', method, urlsplit(url).path, message)
            raise APIError(message, uncertain=method == 'POST' and status >= 500)
        return result

    def Account(self):
        return self.Request('GET', x_api_origin + '/me')

    def Tracks(self):
        url = x_api_origin + '/me/tracks?linked_partitioning=true&limit=200'
        tracks = []
        visited = set()
        while url:
            if url in visited:
                raise APIError('SoundCloud pagination loop')
            visited.add(url)
            result = self.Request('GET', url)
            if isinstance(result, list):
                tracks.extend(result)
                break
            tracks.extend(result['collection'])
            url = result.get('next_href')
        return tracks

    def Upload(self, path, title, artwork_path, progress):
        try:
            if Path(path).stat().st_size > 4 * 1024 ** 3:
                raise APIError('SoundCloud upload exceeds 4 GB')
            body = Multipart(path, title, artwork_path, progress)
        except OSError:
            raise APIError('Local audio or artwork is unavailable before upload') from None
        result = self.Request('POST', x_api_origin + '/tracks', body,
                              {'Content-Type': 'multipart/form-data; boundary=' + body.m_boundary,
                               'Content-Length': str(body.m_length)})
        if not isinstance(result, dict) or not (result.get('urn') or result.get('id')) or not result.get('permalink_url'):
            raise APIError('SoundCloud creation response is incomplete', uncertain=True)
        return result
