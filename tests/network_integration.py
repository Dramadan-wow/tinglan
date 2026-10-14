"""Real server process + two real Windows Qt UI processes; all data/keys are isolated.
Run after scripts/build_gui.py. The cryptography package is needed only for TLS test certificates.
"""
from pathlib import Path
import datetime
import ipaddress
import json
import os
import queue
import secrets
import socket
import ssl
import struct
import subprocess
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests' / 'network-output' / str(time.time_ns())
QT = ROOT / '.tools/Qt/6.8.3/msvc2022_64'
ENV = {k.upper(): v for k, v in os.environ.items()}
ENV['PATH'] = str(QT / 'bin') + os.pathsep + ENV.get('PATH', '')
ENV['QT_PLUGIN_PATH'] = str(QT / 'plugins')
SERVER = Path(os.environ.get('TINGLAN_TEST_SERVER', str(ROOT / 'bin/im_server.exe')))
PROBE = ROOT / 'build-gui/Release/network_gui_probe.exe'
checks, skipped, children = [], [], []


def check(ok, name):
    if not ok:
        raise AssertionError(name)
    checks.append(name)
    print('PASS', name, flush=True)


def wait_for(predicate, timeout=30):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        result = predicate()
        if result:
            return result
        time.sleep(.03)
    raise TimeoutError('Timed out waiting for process event')


class Process:
    def __init__(self, args, name, initial=None):
        self.log = OUT / (name + '.log')
        self.lines, self.events = [], []
        self.process = subprocess.Popen([str(x) for x in args], cwd=ROOT, env=ENV,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        children.append(self)
        def read():
            with self.log.open('w', encoding='utf-8') as log:
                for data in self.process.stdout:
                    line = data.decode('utf-8', errors='replace').rstrip()
                    self.lines.append(line)
                    log.write(line + '\n'); log.flush()
                    if line.startswith('{'):
                        try: self.events.append(json.loads(line))
                        except json.JSONDecodeError: pass
        self.reader = threading.Thread(target=read, daemon=True); self.reader.start()
        if initial is not None:
            self.process.stdin.write((json.dumps(initial, ensure_ascii=False) + '\n').encode('utf-8'))
            self.process.stdin.flush()
        self.process.stdin.close()

    def event(self, kind, predicate=lambda e: True, timeout=30):
        def find():
            for e in list(self.events):
                if e.get('event') == kind and predicate(e):
                    self.events.remove(e)
                    return e
            if self.process.poll() is not None:
                raise RuntimeError(f'Process exited {self.process.returncode}: {self.lines[-5:]}')
        return wait_for(find, timeout)

    def kill(self):
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=15)
        self.reader.join(timeout=3)


class Gui(Process):
    def __init__(self, name, port, password, tls=False, ca=None):
        self.folder = OUT / name; self.folder.mkdir()
        super().__init__([PROBE], name, {'directory': str(self.folder), 'username': name.split('-')[0],
            'password': password, 'host': '127.0.0.1', 'port': port, 'tls': tls, 'caFile': str(ca or '')})

    def command(self, action, **kwargs):
        with (self.folder / 'commands.jsonl').open('a', encoding='utf-8') as f:
            f.write(json.dumps({'action': action, **kwargs}, ensure_ascii=False) + '\n')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0)); return s.getsockname()[1]


def message(kind, data=None, sender='', recipient='', mid=None):
    return {'v': 1, 'type': kind, 'id': mid or str(uuid.uuid4()),
            'senderId': sender, 'recipientId': recipient, 'data': data or {}}


def frame(m):
    body = json.dumps(m, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    return struct.pack('!I', len(body)) + body


class Raw:
    def __init__(self, port, context=None):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=10)
        if context:
            self.s = context.wrap_socket(self.s, server_hostname='localhost')
        self.s.settimeout(20)

    def send(self, m): self.s.sendall(frame(m))

    def recv(self):
        def exact(n):
            result = b''
            while len(result) < n:
                data = self.s.recv(n - len(result))
                if not data: raise EOFError('connection closed')
                result += data
            return result
        n = struct.unpack('!I', exact(4))[0]
        assert 0 < n <= 65536
        return json.loads(exact(n))

    def close(self): self.s.close()


def login(raw, username, password):
    raw.send(message('login', {'username': username, 'password': password}))
    return raw.recv()


def start_server(config, name):
    p = Process([SERVER, '--config', config], name)
    wait_for(lambda: any('listening address=' in line for line in p.lines))
    return p


def certificates():
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
    now = datetime.datetime.now(datetime.timezone.utc)
    ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Tinglan isolated test CA')])
    ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name).public_key(ca_key.public_key())
          .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(minutes=5))
          .not_valid_after(now + datetime.timedelta(days=1)).add_extension(x509.BasicConstraints(ca=True, path_length=0), True)
          .sign(ca_key, hashes.SHA256()))
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'localhost')]))
            .issuer_name(ca_name).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(minutes=5)).not_valid_after(now + datetime.timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'), x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]), False)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False).sign(ca_key, hashes.SHA256()))
    ca_path, cert_path, key_path = [OUT / name for name in ['test-ca.pem', 'test-server.pem', 'test-key.pem']]
    ca_path.write_bytes(ca.public_bytes(serialization.Encoding.PEM))
    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    return ca_path, cert_path, key_path


def main():
    OUT.mkdir(parents=True)
    passwords = {u: secrets.token_urlsafe(24) for u in ['alice', 'bob', 'carol']}
    port = free_port(); config = OUT / 'server.json'; users = OUT / 'users.json'
    settings = {'listenAddress': '127.0.0.1', 'port': port, 'tls': False, 'allowInsecureLoopback': True,
                'usersFile': str(users), 'maxConnections': 128, 'loginTimeoutMs': 30000, 'idleTimeoutMs': 60000}
    config.write_text(json.dumps(settings), encoding='utf-8')
    for user, password in passwords.items():
        p = subprocess.run([SERVER, '--config', config, '--add-user', user], input=(password + '\n').encode(), env=ENV, cwd=ROOT, capture_output=True, timeout=45)
        check(p.returncode == 0, 'Provision secure account ' + user)
    saved = users.read_text(encoding='utf-8'); records = json.loads(saved)['users']
    check(all(p not in saved for p in passwords.values()) and all(r['iterations'] == 600000 for r in records.values()), 'PBKDF2 store contains no plaintext passwords')
    check(len({r['salt'] for r in records.values()}) == 3, 'Each account has an independent random salt')
    bad = OUT / 'unsafe.json'; bad.write_text(json.dumps({**settings, 'listenAddress': '0.0.0.0'}), encoding='utf-8')
    reject = subprocess.run([SERVER, '--config', bad], capture_output=True, env=ENV, timeout=15)
    check(reject.returncode != 0, 'Reject plaintext public/LAN listener')
    server = start_server(config, 'server')
    lock = subprocess.run([SERVER, '--config', config, '--add-user', 'blocked'], input=b'isolated-test-password\n', env=ENV, capture_output=True, timeout=15)
    check(lock.returncode != 0, 'Account provisioning cannot race a running server')
    a, b = Gui('alice', port, passwords['alice']), Gui('bob', port, passwords['bob'])
    a.command('pulse'); a.event('pulse'); check(True, 'Qt UI event loop remains responsive during connection/authentication')
    a.event('online', lambda e: e['value']); b.event('online', lambda e: e['value'])
    check(True, 'Independent server and two full Windows GUI client processes authenticated')
    a.command('send', to='bob', text='你好，听澜！🙂 <>& "引号" \\ special')
    chat = b.event('chat'); check(chat['sender'] == 'alice' and '🙂' in chat['text'], 'GUI A to GUI B: Chinese, emoji and special characters')
    check(a.event('ack', lambda e: e['id'] == chat['id'])['ok'], 'GUI send success feedback comes from server acknowledgment')
    b.command('send', to='alice', text='收到，来自另一台客户端窗口的回复！')
    check(a.event('chat')['sender'] == 'bob', 'GUI B to GUI A reply')
    b.event('ack')
    a.command('send', to='nobody', text='offline test')
    check(not a.event('ack')['ok'], 'Offline recipient reports send failure')
    long_text = '长消息🙂' * 850
    a.command('send', to='bob', text=long_text)
    check(b.event('chat')['text'] == long_text, 'Long UTF-8 GUI message below 16 KiB delivered intact')
    a.event('ack')
    for gui in [a, b]: gui.command('shot'); check(gui.event('screenshot')['ok'], 'Actual full GUI screenshot: ' + gui.folder.name)

    unauth = Raw(port); unauth.send(message('chat', {'text': 'bad'}, 'alice', 'bob'))
    check(unauth.recv()['data']['code'] == 'not_authenticated', 'Unauthenticated chat rejected'); unauth.close()
    wrong = Raw(port)
    check(not login(wrong, 'carol', 'incorrect-test-password')['data']['ok'], 'Wrong remote password rejected'); wrong.close()
    duplicate = Raw(port)
    check(login(duplicate, 'alice', passwords['alice'])['data']['code'] == 'already_online', 'Duplicate active account does not replace existing session'); duplicate.close()
    raw = Raw(port); check(login(raw, 'carol', passwords['carol'])['data']['ok'], 'Third simultaneous authenticated client')
    spoof = message('chat', {'text': 'spoof'}, 'alice', 'bob'); raw.send(spoof)
    check(raw.recv()['data']['code'] == 'sender_mismatch', 'Authenticated client cannot forge senderId')
    m = message('chat', {'text': '半包 + 粘包\n中文🙂'}, 'carol', 'bob'); wire = frame(m)
    raw.s.sendall(wire[:2]); time.sleep(.1); raw.s.sendall(wire[2:9]); time.sleep(.1)
    raw.s.sendall(wire[9:] + frame(message('heartbeat', sender='carol')))
    replies = [raw.recv(), raw.recv()]
    check({r['type'] for r in replies} == {'message_ack', 'heartbeat'}, 'Split length/body and coalesced frames decoded correctly')
    check(b.event('chat')['text'] == m['data']['text'], 'Fragmented UTF-8 payload delivered unchanged')
    raw.send(m); ack = raw.recv()
    check(ack['data']['ok'] and ack['data']['duplicate'], 'Identical message ID is acknowledged without forwarding twice')
    time.sleep(.2); check(not any(e.get('event') == 'chat' and e.get('id') == m['id'] for e in b.events), 'Recipient sees no duplicate delivery')
    raw.send({**m, 'data': {'text': 'changed'}})
    check(raw.recv()['data']['code'] == 'id_conflict', 'Same ID with altered payload rejected')
    raw.send(message('chat', {'text': '大' * 6000}, 'carol', 'bob'))
    check(raw.recv()['data']['code'] == 'invalid_chat', 'Text exceeding 16 KiB rejected')
    raw.send(message('unsupported', sender='carol'))
    check(raw.recv()['data']['code'] == 'unknown_type', 'Unknown message type rejected')
    raw.close()
    for body, code in [(b'not-json', 'invalid_json'), (b'\xff', 'invalid_utf8'), (b'[]', 'invalid_json'), (b'{}', 'invalid_envelope')]:
        r = Raw(port); r.s.sendall(struct.pack('!I', len(body)) + body)
        check(r.recv()['data']['code'] == code, 'Malformed input rejected: ' + code); r.close()
    for size in [0, 65537, 0xffffffff]:
        r = Raw(port); r.s.sendall(struct.pack('!I', size))
        check(r.recv()['data']['code'] == 'invalid_frame_length', f'Illegal frame size rejected: {size}'); r.close()
    r = Raw(port); r.s.sendall(frame(message('heartbeat')) * 101)
    replies = [r.recv() for _ in range(101)]
    check(replies[-1]['data']['code'] == 'rate_limit', 'Per-connection message flood limit enforced'); r.close()

    server.kill(); a.event('online', lambda e: not e['value']); b.event('online', lambda e: not e['value'])
    a.command('pulse'); a.event('pulse'); check(True, 'UI responsive while server disconnected')
    server = start_server(config, 'server-restarted')
    a.event('online', lambda e: e['value'], 45); b.event('online', lambda e: e['value'], 45)
    check(True, 'Both clients automatically reconnect and reauthenticate after server restart')
    b.command('send', to='alice', text='重连后的消息'); check(a.event('chat')['text'] == '重连后的消息', 'Chat works after reconnect'); b.event('ack')
    b.command('stop'); b.event('online', lambda e: not e['value']); time.sleep(.2)
    a.command('send', to='bob', text='after logout'); check(not a.event('ack')['ok'], 'Disconnected user removed from server routing table')
    b.command('connect'); b.event('online', lambda e: e['value']); check(True, 'Manual reconnect after explicit disconnect')
    a.command('quit'); b.command('quit'); a.process.wait(15); b.process.wait(15); server.kill()

    timeout_config = OUT / 'timeouts.json'; timeout_config.write_text(json.dumps({**settings, 'idleTimeoutMs': 2000, 'loginTimeoutMs': 2000}), encoding='utf-8')
    server = start_server(timeout_config, 'server-timeouts'); r = Raw(port); r.s.settimeout(6)
    check(r.s.recv(1) == b'', 'Unauthenticated connection timeout closes socket'); r.close()
    r = Raw(port); check(login(r, 'carol', passwords['carol'])['data']['ok'], 'Heartbeat timeout test login')
    r.s.settimeout(6); check(r.s.recv(1) == b'', 'Authenticated idle/heartbeat timeout closes socket'); r.close(); server.kill()

    try:
        ca, cert, key = certificates()
    except ImportError:
        skipped.append('TLS integration: cryptography package unavailable'); return
    tls_config = OUT / 'tls.json'; tls_config.write_text(json.dumps({**settings, 'tls': True, 'allowInsecureLoopback': False,
        'certificateFile': str(cert), 'keyFile': str(key)}), encoding='utf-8')
    server = start_server(tls_config, 'server-tls')
    secure = Gui('alice-tls', port, passwords['alice'], True, ca); secure.event('online', lambda e: e['value'])
    context = ssl.create_default_context(cafile=str(ca)); r = Raw(port, context)
    check(login(r, 'bob', passwords['bob'])['data']['ok'], 'TLS authenticated protocol interoperates with Python SSL client')
    secure.command('send', to='bob', text='TLS 加密消息🙂'); received = r.recv()
    check(received['data']['text'] == 'TLS 加密消息🙂' and secure.event('ack')['ok'], 'Qt GUI to server to TLS recipient succeeds')
    r.send(message('chat', {'text': 'TLS reply'}, 'bob', 'alice')); r.recv()
    check(secure.event('chat')['text'] == 'TLS reply', 'TLS reply reaches Qt GUI')
    untrusted = Gui('carol-untrusted', port, passwords['carol'], True)
    untrusted.event('state', lambda e: '证书校验失败' in e['text'])
    check(not any(e.get('event') == 'online' and e.get('value') for e in untrusted.events), 'Qt client rejects untrusted server certificate without plaintext fallback')
    # CA trusted, hostname deliberately wrong: hostname verification must still reject the certificate.
    wrong_hostname = socket.create_connection(('127.0.0.1', port), timeout=5)
    try:
        context.wrap_socket(wrong_hostname, server_hostname='wrong.example.invalid')
    except ssl.SSLCertVerificationError:
        check(True, 'TLS hostname mismatch rejected')
    else:
        raise AssertionError('Wrong hostname certificate accepted')
    finally: wrong_hostname.close()
    secure.command('shot'); secure.event('screenshot'); secure.command('quit'); untrusted.command('quit'); r.close()
    secure.process.wait(15); untrusted.process.wait(15); server.kill()


if __name__ == '__main__':
    status, error = 'passed', None
    try:
        main()
    except Exception as e:
        status, error = 'failed', repr(e)
        print('FAIL', error, flush=True)
    finally:
        for child in children:
            child.kill()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / 'results.json').write_text(json.dumps({'status': status, 'count': len(checks), 'checks': checks,
            'skipped': skipped, 'error': error, 'environment': 'Windows local processes; no real LAN/cloud/Linux execution'}, ensure_ascii=False, indent=2), encoding='utf-8')
        print('RESULT', OUT / 'results.json', flush=True)
    raise SystemExit(status != 'passed')
