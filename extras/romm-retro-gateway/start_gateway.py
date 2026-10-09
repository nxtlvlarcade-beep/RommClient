#!/usr/bin/env python3
"""Start Caddy/setup unconditionally; reload RomM services after config changes."""
import os
import pathlib
import signal
import subprocess
import time

CONFIG = pathlib.Path('/downloads/.retro-config/romm.env')
services = ['whdload_service.py', 'index_service.py', 'retro_web.py', 'telnet_server.py']
children = []

def launch(script, env):
    return subprocess.Popen(['python3', '-u', '/usr/local/bin/' + script], env=env)

def stop_children():
    global children
    for child in children:
        if child.poll() is None: child.terminate()
    for child in children:
        try: child.wait(timeout=5)
        except subprocess.TimeoutExpired: child.kill(); child.wait()
    children = []

def shutdown(*args):
    stop_children()
    if caddy and caddy.poll() is None: caddy.terminate()
    for child in permanent:
        if child.poll() is None: child.terminate()
    raise SystemExit(0)

signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT, shutdown)
permanent = [launch('config_service.py', os.environ.copy())]
caddy = None
last = None
while True:
    config = {}
    if CONFIG.exists():
        for line in CONFIG.read_text().splitlines():
            if '=' in line:
                k, v = line.split('=', 1)
                if k in ('ROMM_URL', 'ROMM_TOKEN', 'TELNET_PORT', 'ROMM_ZMODEM'): config[k] = v
    env = os.environ.copy()
    env.update(config)
    current = tuple(env.get(k) for k in ('ROMM_URL', 'ROMM_TOKEN', 'TELNET_PORT', 'ROMM_ZMODEM'))
    env.setdefault('ROMM_URL', 'http://127.0.0.1:8099')
    env.setdefault('ROMM_TOKEN', 'not-configured')
    if current != last:
        if caddy and caddy.poll() is None:
            caddy.terminate()
            try: caddy.wait(timeout=5)
            except subprocess.TimeoutExpired: caddy.kill(); caddy.wait()
        caddy = subprocess.Popen(['caddy', 'run', '--config', '/etc/caddy/Caddyfile', '--adapter', 'caddyfile'], env=env)
    if current != last or (children and any(p.poll() is not None for p in children)):
        stop_children()
        if env.get('ROMM_URL') and env.get('ROMM_TOKEN'):
            children = [launch(s, env) for s in services]
            print('RomM services started / configuration reloaded', flush=True)
        else:
            print('Waiting for configuration at http://GATEWAY-IP/config', flush=True)
        last = current
    time.sleep(2)
