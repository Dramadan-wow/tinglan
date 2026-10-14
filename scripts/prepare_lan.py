"""Explicit LAN deployment profile. Never edits original configuration or databases.

Python 3 + cryptography are required only on A during configuration, not on B.
Generated server/backup directories are private; share the public connection code only.
"""
from pathlib import Path
import argparse
from contextlib import closing
import datetime as dt
import hashlib
import ipaddress
import json
import os
import re
import shutil
import sqlite3
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


def resolve_path(config, value):
    p = Path(value)
    return (p if p.is_absolute() else config.parent / p).resolve()


def private_directory(path):
    path.mkdir(parents=True, exist_ok=False)
    if os.name == 'nt':
        # Remove inherited access before any key or backup is written.
        sid = subprocess.check_output(['whoami', '/user', '/fo', 'csv', '/nh'])
        sid = re.search(rb'S-1-\d+(?:-\d+)+', sid).group().decode('ascii')
        subprocess.run(['icacls', str(path), '/inheritance:r', '/grant:r',
                        '*'+sid+':(OI)(CI)F', '*S-1-5-18:(OI)(CI)F'],
                       check=True, capture_output=True)
    else:
        path.chmod(0o700)


def backup(config, destination):
    settings = read_json(config)
    database = resolve_path(config, settings['workspaceFile'])
    users = resolve_path(config, settings['usersFile'])
    if not database.is_file():
        raise ValueError('Existing workspaceFile not found; refusing to create an empty database')
    private_directory(destination)
    # SQLite backup API includes committed WAL content, unlike a plain file copy.
    with closing(sqlite3.connect(database.as_uri()+'?mode=ro', uri=True)) as source:
        with closing(sqlite3.connect(destination/'workspace.sqlite')) as target:
            source.backup(target)
            if target.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                raise RuntimeError('Backup integrity check failed')
            tables = [r[0] for r in target.execute("SELECT name FROM sqlite_master WHERE type='table'")]
            counts = {t: target.execute('SELECT COUNT(*) FROM "'+t.replace('"','""')+'"').fetchone()[0] for t in tables}
            target.execute('PRAGMA journal_mode=DELETE')
    # Legacy raw-TCP usersFile may never have been created. Platform accounts live in SQLite.
    if users.is_file(): shutil.copy2(users, destination/'users.json')
    shutil.copy2(config, destination/'server.original.json')
    if (config.parent/'client.json').is_file():
        shutil.copy2(config.parent/'client.json', destination/'client.original.json')
    write_json(destination/'manifest.json', dict(sourceDatabase=str(database), sourceConfig=str(config),
        created=dt.datetime.now(dt.timezone.utc).isoformat(), tableRows=counts, legacyUsersFileExisted=users.is_file(),
        sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.iterdir() if p.is_file()}))
    return settings, database, users


def identity(host):
    from cryptography import x509
    try:
        ip = ipaddress.ip_address(host)
        if ip.version != 4 or ip.is_unspecified or ip.is_multicast or ip.is_link_local:
            raise ValueError('Use a unicast IPv4 address or DNS name, not a wildcard/link-local address')
        return x509.IPAddress(ip)
    except ValueError:
        if not re.fullmatch(r'(?=.{1,253}$)(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?', host) or re.fullmatch(r'[\d.]+', host):
            raise ValueError('Invalid server IP/DNS name')
        return x509.DNSName(host)


def certificates(host, directory):
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization as s
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
    san = identity(host)
    now = dt.datetime.now(dt.timezone.utc)
    ca_key = rsa.generate_private_key(public_exponent=65537, key_size=3072)
    key = rsa.generate_private_key(public_exponent=65537, key_size=3072)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Tinglan private LAN test CA')])
    ca = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(ca_key.public_key())
          .serial_number(x509.random_serial_number()).not_valid_before(now-dt.timedelta(minutes=10))
          .not_valid_after(now+dt.timedelta(days=365))
          .add_extension(x509.BasicConstraints(ca=True, path_length=0), True)
          .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), True)
          .add_extension(x509.SubjectKeyIdentifier.from_public_key(ca_key.public_key()), False)
          .sign(ca_key, hashes.SHA256()))
    cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, host)]))
            .issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now-dt.timedelta(minutes=10)).not_valid_after(now+dt.timedelta(days=90))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), True)
            .add_extension(x509.SubjectAlternativeName([san]), False)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False)
            .add_extension(x509.KeyUsage(True, False, True, False, False, False, False, False, False), True)
            .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), False)
            .sign(ca_key, hashes.SHA256()))
    for filename, value in [('ca.pem', ca), ('server.pem', cert)]:
        (directory/filename).write_bytes(value.public_bytes(s.Encoding.PEM))
    for filename, value in [('ca.key', ca_key), ('server.key', key)]:
        (directory/filename).write_bytes(value.private_bytes(s.Encoding.PEM, s.PrivateFormat.PKCS8, s.NoEncryption()))
    return dict(host=host, san=str(san.value), serverExpires=cert.not_valid_after_utc.isoformat(),
                caSHA256=ca.fingerprint(hashes.SHA256()).hex(),
                caFileSHA256=hashlib.sha256((directory/'ca.pem').read_bytes()).hexdigest())


def prepare(config, host, listen, destination, runtime):
    identity(host)
    address = ipaddress.ip_address(listen)
    if address.version != 4 or address.is_multicast or address.is_link_local:
        raise ValueError('Listen address must be IPv4; 0.0.0.0 is allowed explicitly')
    for name in ('im_desktop.exe', 'im_server.exe', 'packaged.json'):
        if not (runtime/name).is_file():
            raise ValueError('Missing deployed runtime: '+str(runtime/name))
    if any(p.suffix.lower() in ('.sqlite', '.db', '.key', '.pfx') or p.name in ('server', 'data', 'users.json') for p in runtime.rglob('*')):
        raise ValueError('Runtime contains private data; use the clean packaged runtime directory')
    destination.mkdir(parents=True, exist_ok=False)
    settings, database, users = backup(config, destination/'backup')
    private = destination/'server'
    private_directory(private)
    details = certificates(host, private)
    settings.pop('workspaceImport', None)
    settings.update(listenAddress=listen, port=56110, tls=True, allowInsecureLoopback=False,
                    idleExitMs=0, workspaceFile=str(database), usersFile=str(users),
                    certificateFile='server.pem', keyFile='server.key')
    write_json(private/'server.json', settings)
    client = destination/'client';client.mkdir()
    # Only public configuration is generated here. Friends install the generic client EXE.
    shutil.copy2(private/'ca.pem', client/'ca.pem')
    write_json(client/'client.json', dict(host=host, port=56110, tls=True,
        allowInsecureLoopback=False, autoStartLocalServer=False, caFile='ca.pem'))
    write_json(destination/'certificate-info.json', details)
    shutil.copy2(destination/'certificate-info.json', client/'certificate-info.json')
    (destination/'Start-Owner-Client.cmd').write_text('@echo off\nchcp 65001 >nul\nstart "" "'+str(runtime/'im_desktop.exe')+'" --client-config "%~dp0client/client.json"\r\n', encoding='utf-8')
    # Console is intentional: A can see startup failure and stop gracefully with Ctrl+C.
    (destination/'Start-Server.ps1').write_text(
        "$ErrorActionPreference = 'Stop'\n"
        "$config = Join-Path $PSScriptRoot 'server/server.json'\n"
        "$s = Get-Content -LiteralPath $config -Raw -Encoding UTF8 | ConvertFrom-Json\n"
        "if (!(Test-Path -LiteralPath $s.workspaceFile -PathType Leaf)) { throw 'Existing database missing. Refusing to start.' }\n"
        "$busy = Get-NetTCPConnection -State Listen -LocalPort 56110 -ErrorAction SilentlyContinue\n"
        "if ($busy) { throw 'Port 56110 is already in use. Close local clients and stop their local server first (see guide).' }\n"
        "& '"+str(runtime/'im_server.exe').replace("'", "''")+"' --config $config\n"
        "if ($LASTEXITCODE -ne 0) { throw \"Server exited: $LASTEXITCODE\" }\n", encoding='utf-8-sig')
    (destination/'Start-Server.cmd').write_text('@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Server.ps1"\r\npause\r\n',encoding='ascii')
    from export_connection_code import encode
    (destination/'connection-code.txt').write_text(encode(client/'client.json')+'\n',encoding='utf-8')
    print('Created LAN profile:', destination)
    print('Send the generic Client-Setup.exe and connection-code.txt to B; never send server/ or backup/.')
    print('CA file SHA256:', details['caFileSHA256'])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server-config', type=Path, default=Path(os.environ.get('LOCALAPPDATA', '.'))/'Hsx Coursework/听澜/server.json')
    p.add_argument('--backup-only', action='store_true')
    p.add_argument('--host', help='Address entered by the user at deployment time; never auto-detected')
    p.add_argument('--listen', help='Numeric A interface address, or explicitly 0.0.0.0')
    p.add_argument('--out', type=Path)
    p.add_argument('--runtime', type=Path, default=ROOT/('build-package/v'+(ROOT/'VERSION').read_text().strip()+'/runtime'))
    a = p.parse_args()
    config = a.server_config.resolve()
    if a.backup_only:
        out = (a.out or ROOT/'recovery'/('pre-lan-'+str(time.time_ns()))).resolve()
        backup(config, out);print('Consistent backup:', out);return
    if not a.host: p.error('--host must be supplied at deployment time')
    try: ipaddress.ip_address(a.host);listen = a.listen or a.host
    except ValueError:
        if not a.listen: p.error('DNS host requires --listen with a numeric local interface IP')
        listen = a.listen
    out = (a.out or ROOT/'lan-profiles'/str(time.time_ns())).resolve()
    prepare(config, a.host, listen, out, a.runtime.resolve())


if __name__ == '__main__':
    main()
