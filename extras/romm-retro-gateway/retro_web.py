#!/usr/bin/env python3
"""HTTP/1.0 HTML remote control for the existing romm-tui Telnet service."""
import html
import http.server
import os
import re
import secrets
import socket
import subprocess
import threading
import time
import urllib.parse
import urllib.request
from http.cookies import SimpleCookie

TELNET_HOST = '127.0.0.1'
TELNET_PORT = int(os.getenv('TELNET_PORT', '2323'))
ROMM_URL = os.environ['ROMM_URL'].rstrip('/')
ROMM_TOKEN = os.environ['ROMM_TOKEN']
SESSIONS = {}
LOCK = threading.RLock()
TTL = 1200
COLS, ROWS = 100, 32

class Terminal:
    def __init__(self):
        self.lines = [[' '] * COLS for _ in range(ROWS)]
        self.x = self.y = 0
        self.state = 'text'
        self.csi = ''
        self.selected = False

    def feed(self, data):
        for c in data.decode('latin-1', 'replace'):
            if self.state == 'esc':
                if c == '[': self.state = 'csi'; self.csi = ''
                else: self.state = 'text'
                continue
            if self.state == 'csi':
                if c.isalpha() or c in '@`~':
                    nums = [int(x) if x else 0 for x in self.csi.split(';') if x == '' or x.isdigit()]
                    n = nums[0] if nums else 0
                    if c in 'Hf':
                        self.y = max(0, min(ROWS - 1, (n or 1) - 1))
                        self.x = max(0, min(COLS - 1, ((nums[1] if len(nums) > 1 else 1) or 1) - 1))
                    elif c == 'J' and n == 2:
                        self.lines = [[' '] * COLS for _ in range(ROWS)]
                    elif c == 'K':
                        self.lines[self.y][self.x:] = [' '] * (COLS - self.x)
                    elif c == 'm': self.selected = (n == 7)
                    self.state = 'text'
                elif len(self.csi) < 40: self.csi += c
                else: self.state = 'text'
                continue
            if c == '\x1b': self.state = 'esc'
            elif c == '\r': self.x = 0
            elif c == '\n': self.y = min(ROWS - 1, self.y + 1)
            elif c == '\b': self.x = max(0, self.x - 1)
            elif c >= ' ' and self.x < COLS:
                self.lines[self.y][self.x] = c
                self.x += 1

    def screen(self):
        return '\n'.join(''.join(row).rstrip() for row in self.lines).strip()

class Session:
    def __init__(self):
        self.sock = socket.create_connection((TELNET_HOST, TELNET_PORT), timeout=8)
        self.sock.settimeout(.15)
        self.term = Terminal()
        self.lock = threading.RLock()
        self.last = time.monotonic()
        self._negotiation()
        self.read()

    def _negotiation(self):
        # Telnet WILL ECHO/SGA/BINARY; reply with NAWS 100x32.
        self.sock.sendall(bytes([255, 251, 31, 255, 250, 31, 0, COLS, 0, ROWS, 255, 240]))

    def read(self):
        deadline = time.monotonic() + 3.5
        last_data = time.monotonic()
        buf = bytearray()
        while time.monotonic() < deadline:
            try: chunk = self.sock.recv(65536)
            except socket.timeout:
                if time.monotonic() - last_data > .35: break
                continue
            if not chunk: break
            last_data = time.monotonic()
            buf.extend(chunk)
            if len(buf) > 1000000: break
        # Remove Telnet IAC negotiations, including NAWS subnegotiation.
        plain = bytearray(); i = 0
        while i < len(buf):
            b = buf[i]
            if b != 255: plain.append(b); i += 1; continue
            if i + 1 >= len(buf): break
            cmd = buf[i+1]
            if cmd == 255: plain.append(255); i += 2
            elif cmd in (251, 252, 253, 254): i += 3
            elif cmd == 250:
                end = buf.find(b'\xff\xf0', i+2)
                i = len(buf) if end < 0 else end+2
            else: i += 2
        self.term.feed(plain)
        self.last = time.monotonic()

    def press(self, key):
        codes = {'up': b'\x1b[A', 'down': b'\x1b[B', 'left': b'\x1b[D',
                 'right': b'\x1b[C', 'enter': b'\r', 'back': b'\x1bx'}
        if key in codes: self.sock.sendall(codes[key])
        elif key == 'search': self.sock.sendall(b'/')
        self.read()

    def close(self):
        try: self.sock.close()
        except OSError: pass

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'
    server_version = 'RomM-RetroWeb/1.1'

    def reply(self, status, content, mime='text/html; charset=iso-8859-1', cookie=None, filename=None):
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(content)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        if cookie: self.send_header('Set-Cookie', 'rw=' + cookie + '; Path=/; HttpOnly; SameSite=Lax')
        if filename: self.send_header('Content-Disposition', 'attachment; filename="' + filename + '"')
        self.end_headers()
        try: self.wfile.write(content)
        except (BrokenPipeError, ConnectionResetError): pass

    def page(self, title, body, status=200, cookie=None):
        content = ('<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">\n'
                   '<HTML><HEAD><TITLE>' + html.escape(title) + '</TITLE></HEAD><BODY>\n'
                   '<H1>RomM RetroWeb 1.1</H1>\n' + body + '\n</BODY></HTML>').encode('iso-8859-1', 'xmlcharrefreplace')
        self.reply(status, content, cookie=cookie)

    def session(self):
        cookies = SimpleCookie()
        try: cookies.load(self.headers.get('Cookie', ''))
        except Exception: pass
        sid = cookies['rw'].value if 'rw' in cookies else ''
        with LOCK:
            for old, sess in list(SESSIONS.items()):
                if time.monotonic() - sess.last > TTL:
                    sess.close(); del SESSIONS[old]
            if sid not in SESSIONS:
                sid = secrets.token_hex(16)
                SESSIONS[sid] = Session()
            return sid, SESSIONS[sid]

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        qs = urllib.parse.parse_qs(url.query)
        if url.path == '/cover': return self.cover(qs)
        if url.path == '/download': return self.download(qs)
        try: sid, sess = self.session()
        except (OSError, TimeoutError) as exc:
            return self.page('Unavailable', '<P>Telnet backend unavailable: ' + html.escape(str(exc)) + '</P>', 503)
        action = qs.get('a', [''])[0]
        with sess.lock:
            if action in ('up', 'down', 'left', 'right', 'enter', 'back', 'search'):
                sess.press(action)
            elif action == 'query':
                value = qs.get('q', [''])[0]
                # TUI search is ASCII-only and accepts at most 127 characters.
                value = ''.join(c for c in value if ' ' <= c <= '~')[:120]
                sess.sock.sendall(b'/' + value.encode('ascii') + b'\r')
                sess.read()
            else: sess.read()
            screen = sess.term.screen()
        # IDs appear in the game-list column; use them only for optional cover/download links.
        games = re.findall(r'^\s*[> ]\s*(\d{1,10})\s+(.+?)\s*\|', screen, re.M)
        current = re.search(r'^\s*>\s*(\d{1,10})\s+', screen, re.M)
        nav = '<P><A HREF="/?a=up">Up</A> | <A HREF="/?a=down">Down</A> | <A HREF="/?a=enter">Select / WHDLoad</A> | <A HREF="/?a=back">Back</A> | <A HREF="/?a=left">A-Z back</A> | <A HREF="/?a=right">A-Z next</A></P>'
        nav += '<FORM ACTION="/" METHOD="GET"><INPUT TYPE="hidden" NAME="a" VALUE="query"><INPUT NAME="q" SIZE="28" MAXLENGTH="120"><INPUT TYPE="submit" VALUE="Search"></FORM>'

        extra = ''
        if 'GAMES' in screen and current:
            rid = current.group(1)
            extra = '<P><IMG SRC="/cover?id=' + rid + '" ALT="Cover" WIDTH="120"></P>'
            extra += '<P><A HREF="/download?id=' + rid + '">Download RAW</A></P>'
        self.page('RomM TUI', nav + extra + '<PRE>' + html.escape(screen) + '</PRE>' + nav, cookie=sid)

    def cli(self, *args):
        return subprocess.run(['/usr/local/bin/romm-cli', 'http://127.0.0.1:8080', ROMM_TOKEN, *args],
                              capture_output=True, timeout=25, check=True).stdout.decode('utf-8', 'replace').strip()

    def cover(self, qs):
        rid = qs.get('id', [''])[0]
        if not rid.isdigit(): return self.reply(400, b'Invalid id', 'text/plain')
        try:
            path = self.cli('cover-path', rid)
            if not path: return self.reply(404, b'No cover', 'text/plain')
            url = urllib.parse.urljoin(ROMM_URL + '/', path)
            parsed = urllib.parse.urlsplit(url)
            base = urllib.parse.urlsplit(ROMM_URL)
            if (parsed.scheme, parsed.netloc) != (base.scheme, base.netloc):
                return self.reply(400, b'Invalid cover path', 'text/plain')
            req = urllib.request.Request(url, headers={'Authorization': 'Bearer ' + ROMM_TOKEN})
            with urllib.request.urlopen(req, timeout=15) as res:
                mime = res.headers.get_content_type()
                if mime not in ('image/jpeg', 'image/png', 'image/gif', 'image/webp'):
                    return self.reply(415, b'Unsupported cover', 'text/plain')
                data = res.read(1024 * 1024 + 1)
            if len(data) > 1024 * 1024: return self.reply(413, b'Cover too large', 'text/plain')
            self.reply(200, data, mime)
        except Exception as exc:
            self.reply(502, ('Cover unavailable: ' + str(exc)).encode('ascii', 'replace'), 'text/plain')

    def download(self, qs):
        rid = qs.get('id', [''])[0]
        if not rid.isdigit(): return self.reply(400, b'Invalid id', 'text/plain')
        try:
            # ROMM CLI already handles authentication and download endpoint selection.
            import tempfile
            with tempfile.TemporaryDirectory(prefix='retro-download-') as d:
                target = os.path.join(d, 'rom-' + rid + '.bin')
                subprocess.run(['/usr/local/bin/romm-cli', 'http://127.0.0.1:8080', ROMM_TOKEN,
                                'download', rid, target], check=True, timeout=600, capture_output=True)
                with open(target, 'rb') as f: data = f.read()
            self.reply(200, data, 'application/octet-stream', filename='rom-' + rid + '.bin')
        except Exception as exc:
            self.reply(502, ('Download unavailable: ' + str(exc)).encode('ascii', 'replace'), 'text/plain')

if __name__ == '__main__':
    http.server.ThreadingHTTPServer(('127.0.0.1', 8092), Handler).serve_forever()
