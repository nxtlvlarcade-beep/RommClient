#!/usr/bin/env python3
"""RetroWeb: stateless, JavaScript-free HTTP/1.0 frontend for the RomM REST API."""
import html
import io
from PIL import Image, ImageOps, UnidentifiedImageError
import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BASE = os.environ.get('ROMM_URL', '').rstrip('/')
TOKEN = os.environ.get('ROMM_TOKEN', '')
PAGE_SIZE = 30
MAX_IMAGE = 4 * 1024 * 1024

def esc(s):
    return html.escape(str(s if s is not None else ''), quote=True)

def query(**kw):
    return '?' + urllib.parse.urlencode(kw)

def upstream(path, accept='application/json'):
    if not path.startswith('/') or path.startswith('//'):
        raise ValueError('Invalid upstream path')
    req = urllib.request.Request(BASE + path, headers={
        'Authorization': 'Bearer ' + TOKEN, 'Accept': accept,
        'User-Agent': 'RomM-RetroWeb/1.1'})
    return urllib.request.urlopen(req, timeout=45)

def api(path):
    with upstream(path) as r:
        return json.load(r)

def rom_id(raw):
    n = int(raw)
    if not 0 < n <= 2147483647:
        raise ValueError('Invalid ROM ID')
    return n

def local_image_path(value):
    """Accept only paths on the configured RomM origin; never proxy arbitrary hosts."""
    if not isinstance(value, str) or not value.strip():
        return None
    value = value.strip()
    parsed = urllib.parse.urlsplit(value)
    origin = urllib.parse.urlsplit(BASE)
    if parsed.scheme or parsed.netloc:
        if (parsed.scheme, parsed.netloc) != (origin.scheme, origin.netloc):
            return None
        value = parsed.path + (('?' + parsed.query) if parsed.query else '')
    if not value.startswith('/') and not parsed.scheme and not parsed.netloc:
        value = '/' + value
    if not value.startswith('/') or value.startswith('//') or '\\' in value:
        return None
    # RomM installations may live under a reverse-proxy prefix.
    prefix = origin.path.rstrip('/')
    if prefix and value.startswith(prefix + '/'):
        value = value[len(prefix):]
    return value

def cover_paths(meta):
    """Return all candidate cover paths; a stale thumbnail must not hide a valid full cover."""
    found = []
    for key in ('path_cover_small', 'path_cover_large', 'url_cover'):
        path = local_image_path(meta.get(key))
        if path and path not in found:
            found.append(path)
    return found

def cover_path(meta):
    return next(iter(cover_paths(meta)), None)

def screenshots(meta):
    """Flatten documented merged/user screenshot fields without assuming one shape."""
    found = []
    seen = set()
    for field in ('merged_screenshots', 'user_screenshots', 'all_user_screenshots', 'screenshot_path'):
        entries = meta.get(field) or []
        if isinstance(entries, (str, dict, int)):
            entries = [entries]
        if not isinstance(entries, list):
            continue
        for item in entries:
            if isinstance(item, dict):
                candidate = None
                for key in ('path', 'url', 'screenshot_path', 'file_path', 'image_url', 'url_screenshot'):
                    candidate = local_image_path(item.get(key))
                    if candidate: break
                if not candidate:
                    sid = item.get('id')
                    if isinstance(sid, int) and sid > 0:
                        candidate = '/api/screenshots/%d/content' % sid
            else:
                candidate = local_image_path(item)
                if not candidate and isinstance(item, int) and item > 0:
                    candidate = '/api/screenshots/%d/content' % item
            if candidate and candidate not in seen:
                seen.add(candidate)
                found.append(candidate)
            if len(found) >= 12:
                return found
    return found

def image_data(path, fmt='gif'):
    with upstream(path, 'image/*') as response:
        data = response.read(MAX_IMAGE + 1)
        if len(data) > MAX_IMAGE:
            raise ValueError('Image exceeds size limit')
    try:
        with Image.open(io.BytesIO(data)) as img:
            img = ImageOps.exif_transpose(img)
            img.thumbnail((320, 320))
            if fmt == 'jpg':
                img = img.convert('RGB')
                out = io.BytesIO()
                img.save(out, 'JPEG', quality=75, optimize=True)
                return 'image/jpeg', out.getvalue()
            img = img.convert('RGB').quantize(colors=128)
            out = io.BytesIO()
            img.save(out, 'GIF', optimize=True)
            return 'image/gif', out.getvalue()
    except (UnidentifiedImageError, OSError, ValueError) as exc:
        raise ValueError('Invalid image data') from exc

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'
    server_version = 'RomM-RetroWeb/1.1'

    def log_message(self, fmt, *args):
        super().log_message(fmt, *args)

    def headers_out(self, code, kind, length=None, filename=None):
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Connection', 'close')
        self.send_header('Cache-Control', 'no-store')
        if length is not None:
            self.send_header('Content-Length', str(length))
        if filename:
            self.send_header('Content-Disposition', 'attachment; filename="' + filename + '"')
        self.end_headers()

    def page(self, title, body, status=200):
        markup = ('<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">\n'
                  '<HTML><HEAD><TITLE>' + esc(title) + '</TITLE></HEAD><BODY>\n'
                  '<H1>RomM RetroWeb</H1><P><A HREF="/">Platforms</A></P>\n'
                  + body + '\n</BODY></HTML>')
        data = markup.encode('iso-8859-1', 'xmlcharrefreplace')
        self.headers_out(status, 'text/html; charset=iso-8859-1', len(data))
        if self.command != 'HEAD':
            self.wfile.write(data)

    def listing(self, params):
        pid = rom_id(params.get('p', [''])[0])
        offset = max(0, min(1000000, int(params.get('o', ['0'])[0])))
        term = params.get('q', [''])[0].strip()[:80]
        letter = params.get('l', [''])[0].upper()
        if letter and (len(letter) != 1 or not 'A' <= letter <= 'Z'):
            raise ValueError('Invalid letter')
        args = {'platform_ids': pid, 'limit': PAGE_SIZE, 'offset': offset}
        if term or letter:
            if term: args['q'] = term
            if letter: args['letter'] = letter
            endpoint = '/api/roms/index-search'
            # Existing index service already performs server-side search and paging.
            # Access it locally rather than downloading entire indexes per request.
            req = urllib.request.Request('http://127.0.0.1:8091' + endpoint + query(**args))
            with urllib.request.urlopen(req, timeout=60) as r:
                payload = json.load(r)
        else:
            endpoint = '/api/roms'
            args.update({'with_char_index': 'false', 'with_filter_values': 'false', 'with_rom_id_index': 'false'})
            payload = api(endpoint + query(**args))
        games = payload if isinstance(payload, list) else payload.get('items', payload.get('roms', []))
        if not isinstance(games, list):
            raise ValueError('Unexpected games response')
        total = len(games) if isinstance(payload, list) else int(payload.get('total', len(games)))
        body = '<H2>Games</H2><FORM METHOD="GET" ACTION="/games">'
        body += '<INPUT TYPE="hidden" NAME="p" VALUE="' + str(pid) + '">'
        body += '<INPUT NAME="q" SIZE="25" MAXLENGTH="80" VALUE="' + esc(term) + '">'
        body += '<INPUT TYPE="submit" VALUE="Search"></FORM>'
        body += '<P>' + ' '.join('<A HREF="/games' + query(p=pid, l=c) + '">' + c + '</A>' for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ') + '</P>'
        body += '<UL>'
        for g in games:
            try: gid = rom_id(g['id'])
            except (KeyError, TypeError, ValueError): continue
            body += '<LI><A HREF="/game' + query(id=gid) + '">' + esc(g.get('name') or g.get('fs_name') or gid) + '</A></LI>'
        body += '</UL><P>Showing ' + str(offset + (1 if games else 0)) + '-' + str(offset + len(games)) + ' of ' + str(total) + '</P>'
        paging = dict(p=pid)
        if term: paging['q'] = term
        if letter: paging['l'] = letter
        if offset:
            body += '<A HREF="/games' + query(**paging, o=max(0, offset - PAGE_SIZE)) + '">Previous</A> '
        if offset + len(games) < total and games:
            body += '<A HREF="/games' + query(**paging, o=offset + PAGE_SIZE) + '">Next</A>'
        self.page('Games', body)

    def detail(self, params):
        gid = rom_id(params.get('id', [''])[0])
        g = api('/api/roms/' + str(gid))
        name = g.get('name') or g.get('fs_name') or str(gid)
        body = '<H2>' + esc(name) + '</H2>'
        if cover_path(g):
            body += '<P><IMG SRC="/cover' + query(id=gid) + '" ALT="Cover for ' + esc(name) + '"></P>'
        shots = screenshots(g)
        if shots:
            body += '<H3>Screenshots</H3>'
            for i in range(len(shots)):
                body += '<P><IMG SRC="/screenshot' + query(id=gid, n=i) + '" ALT="Screenshot ' + str(i + 1) + '"></P>'
        for label, key in [('Platform', 'platform_display_name'), ('Filename', 'fs_name'), ('Summary', 'summary')]:
            if g.get(key): body += '<P><B>' + label + ':</B> ' + esc(g[key]) + '</P>'
        body += '<P><A HREF="/download' + query(id=gid) + '">Download ROM</A></P>'
        if g.get('platform_id'):
            body += '<P><A HREF="/games' + query(p=g['platform_id']) + '">Back to games</A></P>'
        self.page(name, body)

    def image_reply(self, path, params):
        fmt = params.get('format', ['gif'])[0].lower()
        if fmt not in ('gif', 'jpg'):
            raise ValueError('Invalid image format')
        kind, data = image_data(path, fmt)
        self.headers_out(200, kind, len(data))
        if self.command != 'HEAD':
            self.wfile.write(data)

    def cover(self, params):
        gid = rom_id(params.get('id', [''])[0])
        g = api('/api/roms/' + str(gid))
        paths = cover_paths(g)
        if not paths:
            print('No usable cover paths for ROM %s: %r' % (gid, {k:g.get(k) for k in ('path_cover_small','path_cover_large','url_cover')}), flush=True)
            return self.page('No cover', '<P>No cover available.</P>', 404)
        for path in paths:
            try:
                return self.image_reply(path, params)
            except (urllib.error.HTTPError, urllib.error.URLError, ValueError, OSError) as exc:
                print('Cover fetch failed for ROM %s path %r: %s' % (gid, path, exc), flush=True)
                continue
        return self.page('No cover', '<P>RomM cover paths did not return an image.</P>', 404)

    def screenshot(self, params):
        gid = rom_id(params.get('id', [''])[0])
        n = int(params.get('n', ['0'])[0])
        shots = screenshots(api('/api/roms/' + str(gid)))
        if n < 0 or n >= len(shots):
            return self.page('No screenshot', '<P>No screenshot available.</P>', 404)
        self.image_reply(shots[n], params)

    def download(self, params):
        gid = rom_id(params.get('id', [''])[0])
        g = api('/api/roms/' + str(gid))
        name = g.get('fs_name')
        if not isinstance(name, str) or not name or '/' in name or '\\' in name:
            raise ValueError('Invalid ROM filename')
        encoded = urllib.parse.quote(name, safe='')
        path = '/api/roms/' + str(gid) + '/content/' + encoded
        with upstream(path, 'application/octet-stream') as r:
            length = r.headers.get('Content-Length')
            length = int(length) if length and length.isdecimal() else None
            safe = re.sub(r'[^A-Za-z0-9_.-]', '_', name)[:150] or 'rom.bin'
            self.headers_out(200, 'application/octet-stream', length, safe)
            if self.command != 'HEAD':
                while True:
                    chunk = r.read(65536)
                    if not chunk: break
                    self.wfile.write(chunk)

    def handle_request(self):
        parsed = urllib.parse.urlsplit(self.path)
        if len(self.path) > 2048:
            return self.page('Bad request', '<P>URL too long.</P>', 400)
        params = urllib.parse.parse_qs(parsed.query)
        try:
            if parsed.path == '/':
                platforms = api('/api/platforms')
                if not isinstance(platforms, list): raise ValueError('Unexpected platforms response')
                body = '<H2>Platforms</H2><UL>'
                for p in platforms:
                    try: pid = rom_id(p['id'])
                    except (KeyError, ValueError, TypeError): continue
                    name = p.get('display_name') or p.get('name') or str(pid)
                    body += '<LI><A HREF="/games' + query(p=pid) + '">' + esc(name) + '</A></LI>'
                return self.page('Platforms', body + '</UL>')
            if parsed.path == '/games': return self.listing(params)
            if parsed.path == '/game': return self.detail(params)
            if parsed.path == '/cover': return self.cover(params)
            if parsed.path == '/screenshot': return self.screenshot(params)
            if parsed.path == '/download': return self.download(params)
            return self.page('Not found', '<P>Page not found.</P>', 404)
        except (ValueError, KeyError, TypeError) as e:
            return self.page('Bad request', '<P>' + esc(e) + '</P>', 400)
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            self.log_error('Upstream unavailable: %s', type(e).__name__)
            return self.page('Gateway unavailable', '<P>RomM service unavailable.</P>', 502)

    def do_GET(self): self.handle_request()
    def do_HEAD(self): self.handle_request()

if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', 8092), Handler).serve_forever()
