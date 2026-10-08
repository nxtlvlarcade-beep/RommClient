#!/usr/bin/env python3
"""Small Telnet-to-PTY service: one restricted romm-tui per connection.
Telnet negotiation is intentionally minimal (NAWS, SGA, ECHO).
"""
import errno
import fcntl
import os
import pty
import select
import signal
import socket
import struct
import termios

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240
ECHO, SGA, NAWS, BINARY = 1, 3, 31, 0
PORT = int(os.getenv('TELNET_PORT', '2323'))
URL = os.environ['ROMM_URL']
TOKEN = os.environ['ROMM_TOKEN']

class Decoder:
    def __init__(self, master):
        self.state = 'data'
        self.command = 0
        self.option = 0
        self.sub = bytearray()
        self.master = master
        self.binary_in = False

    def feed(self, data):
        out = bytearray()
        for b in data:
            if self.state == 'data':
                if b == IAC: self.state = 'iac'
                else: out.append(b)
            elif self.state == 'iac':
                if b == IAC: out.append(IAC); self.state = 'data'
                elif b in (DO, DONT, WILL, WONT): self.command = b; self.state = 'opt'
                elif b == SB: self.sub.clear(); self.state = 'subopt'
                else: self.state = 'data'
            elif self.state == 'opt':
                if b == BINARY:
                    if self.command == WILL: self.binary_in = True
                    elif self.command == WONT: self.binary_in = False
                self.state = 'data'
            elif self.state == 'subopt':
                self.option = b; self.state = 'sub'
            elif self.state == 'sub':
                if b == IAC: self.state = 'subiac'
                elif len(self.sub) < 128: self.sub.append(b)
            elif self.state == 'subiac':
                if b == SE:
                    if self.option == NAWS and len(self.sub) >= 4:
                        cols, rows = struct.unpack('!HH', self.sub[:4])
                        if 20 <= cols <= 500 and 10 <= rows <= 200:
                            fcntl.ioctl(self.master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
                    self.state = 'data'
                elif b == IAC:
                    if len(self.sub) < 128: self.sub.append(IAC)
                    self.state = 'sub'
                else: self.state = 'data'
        return bytes(out)

def session(conn):
    conn.settimeout(10)
    # Server echoes, suppress go-ahead; ask client to send window size.
    conn.sendall(bytes([IAC, WILL, ECHO, IAC, WILL, SGA, IAC, DO, SGA, IAC, DO, NAWS, IAC, WILL, BINARY, IAC, DO, BINARY]))
    pid, master = pty.fork()
    if pid == 0:
        os.chdir('/downloads')
        os.environ['ROMM_ZMODEM'] = '1'
        os.execv('/usr/local/bin/romm-tui', ['romm-tui', URL, TOKEN])
    decoder = Decoder(master)
    conn.settimeout(None)
    try:
        while True:
            ready, _, _ = select.select([conn, master], [], [])
            if conn in ready:
                data = conn.recv(8192)
                if not data: break
                plain = decoder.feed(data)
                if plain: os.write(master, plain)
            if master in ready:
                try: data = os.read(master, 8192)
                except OSError as e:
                    if e.errno == errno.EIO: break
                    raise
                if not data: break
                conn.sendall(data.replace(b'\xff', b'\xff\xff'))
    except (OSError, BrokenPipeError):
        pass
    finally:
        try: os.kill(pid, signal.SIGTERM)
        except ProcessLookupError: pass
        os.close(master)
        try: os.waitpid(pid, 0)
        except ChildProcessError: pass
        conn.close()

def main():
    signal.signal(signal.SIGCHLD, signal.SIG_IGN)
    with socket.socket() as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(('0.0.0.0', PORT))
        srv.listen(16)
        print(f'Telnet ROMM TUI listening on {PORT}', flush=True)
        while True:
            conn, addr = srv.accept()
            pid = os.fork()
            if pid == 0:
                srv.close()
                try: session(conn)
                finally: os._exit(0)
            conn.close()

if __name__ == '__main__': main()
