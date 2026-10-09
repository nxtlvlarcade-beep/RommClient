#!/usr/bin/env python3
"""LAN-only initial setup UI. Protect behind an HTTPS reverse proxy outside trusted LANs."""
import base64
import hmac
import html
import os
import pathlib
import secrets
import tempfile
import urllib.parse
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path('/downloads/.retro-config')
CONFIG = ROOT / 'romm.env'
PASSWORD = ROOT / 'setup-password'
ROOT.mkdir(mode=0o700, parents=True, exist_ok=True)
os.chmod(ROOT, 0o700)
if not PASSWORD.exists():
    fd = os.open(PASSWORD, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(fd, 'w') as f: f.write(secrets.token_urlsafe(18) + '\n')
print('RetroWeb configuration password: ' + PASSWORD.read_text().strip(), flush=True)

def read_config():
    data = {}
    if CONFIG.exists():
        for line in CONFIG.read_text().splitlines():
            if '=' in line and not line.startswith('#'):
                k, v = line.split('=', 1)
                if k in ('ROMM_URL', 'ROMM_TOKEN', 'TELNET_PORT', 'ROMM_ZMODEM'): data[k] = v
    return data

def page(message=''):
    c = read_config()
    url = html.escape(c.get('ROMM_URL', ''), quote=True)
    port = html.escape(c.get('TELNET_PORT', '2323'), quote=True)
    zmodem = c.get('ROMM_ZMODEM', '1') == '1'
    return ('<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">'
            '<HTML><HEAD><TITLE>RetroWeb configuration</TITLE></HEAD><BODY>'
            '<H1>RetroWeb configuration</H1><P>' + html.escape(message) + '</P>'
            '<FORM METHOD="POST" ACTION="/config">'
            '<P>RomM URL: <INPUT NAME="url" SIZE="48" VALUE="' + url + '"></P>'
            '<P>API token: <INPUT TYPE="password" NAME="token" SIZE="48" VALUE=""></P>'
            '<P>Leave token blank to retain the saved token.</P>'
            '<P>Telnet port: <INPUT NAME="telnet_port" SIZE="6" VALUE="' + port + '"></P>'
            '<P>ZMODEM: <SELECT NAME="zmodem"><OPTION VALUE="1"' + (' SELECTED' if zmodem else '') + '>Enabled</OPTION><OPTION VALUE="0"' + ('' if zmodem else ' SELECTED') + '>Disabled</OPTION></SELECT></P>'
            '<INPUT TYPE="submit" VALUE="Save and test"></FORM>'
            '<P><A HREF="/">Return to RetroWeb</A></P></BODY></HTML>')

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.0'
    def authorized(self):
        value = self.headers.get('Authorization', '')
        if not value.startswith('Basic '): return False
        try:
            raw = base64.b64decode(value[6:], validate=True).decode('utf-8')
        except (ValueError, UnicodeError): return False
        return hmac.compare_digest(raw, 'admin:' + PASSWORD.read_text().strip())
    def reply(self, code, body):
        data = body.encode('iso-8859-1', 'xmlcharrefreplace')
        self.send_response(code)
        self.send_header('Content-Type', 'text/html; charset=iso-8859-1')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        if code == 401: self.send_header('WWW-Authenticate', 'Basic realm="RetroWeb setup"')
        self.end_headers()
        if self.command != 'HEAD': self.wfile.write(data)
    def route(self):
        if self.path != '/config': return self.reply(404, page('Not found'))
        if not self.authorized(): return self.reply(401, page('Login required'))
        if self.command == 'POST':
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= 8192: raise ValueError('Invalid form size')
                form = urllib.parse.parse_qs(self.rfile.read(size).decode('utf-8'), keep_blank_values=True)
                url = form.get('url', [''])[0].strip().rstrip('/')
                token = form.get('token', [''])[0].strip() or read_config().get('ROMM_TOKEN', '')
                port = int(form.get('telnet_port', ['2323'])[0])
                if not 1024 <= port <= 65535 or port in (80, 8080, 8090, 8091, 8092, 8093):
                    raise ValueError('Telnet port must be 1024-65535 and not reserved by gateway')
                zmodem = form.get('zmodem', ['1'])[0]
                if zmodem not in ('0', '1'): raise ValueError('Invalid ZMODEM setting')
                p = urllib.parse.urlsplit(url)
                if p.scheme not in ('http', 'https') or not p.hostname or p.username or p.password or p.query or p.fragment:
                    raise ValueError('Invalid RomM URL')
                if not token or '\n' in token or '\r' in token or '\n' in url or '\r' in url:
                    raise ValueError('Invalid API token')
                req = urllib.request.Request(url + '/api/platforms', headers={'Authorization':'Bearer ' + token})
                with urllib.request.urlopen(req, timeout=12) as resp:
                    if resp.status != 200: raise ValueError('RomM API returned error')
                fd, tmp = tempfile.mkstemp(dir=ROOT, prefix='.romm-', text=True)
                try:
                    with os.fdopen(fd, 'w') as f:
                        f.write('ROMM_URL=' + url + '\nROMM_TOKEN=' + token + '\nTELNET_PORT=' + str(port) + '\nROMM_ZMODEM=' + zmodem + '\n')
                        f.flush(); os.fsync(f.fileno())
                    os.chmod(tmp, 0o600)
                    os.replace(tmp, CONFIG)
                finally:
                    if os.path.exists(tmp): os.unlink(tmp)
                return self.reply(200, page('Connection successful. Settings saved. Services will reload shortly.'))
            except (ValueError, urllib.error.URLError, OSError) as exc:
                return self.reply(400, page('Configuration not saved: ' + str(exc)))
        return self.reply(200, page(''))
    def do_GET(self): self.route()
    def do_HEAD(self): self.route()
    def do_POST(self): self.route()

if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', 8093), Handler).serve_forever()
