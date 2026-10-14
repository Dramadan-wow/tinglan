"""Provision a server account without putting its password in arguments, files or logs."""
import argparse
import getpass
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('username', help='3-32 lowercase ASCII letters/digits/underscore, starting with a letter')
parser.add_argument('--config', type=Path, default=root / 'server/config/local.json')
parser.add_argument('--server', type=Path, default=root / ('bin/im_server.exe' if os.name == 'nt' else 'build-server/im_server'))
args = parser.parse_args()
password = getpass.getpass('Remote password (12-128 UTF-8 bytes): ')
if password != getpass.getpass('Repeat password: '):
    raise SystemExit('Passwords do not match.')
if not 12 <= len(password.encode('utf-8')) <= 128:
    raise SystemExit('Password must contain 12-128 UTF-8 bytes.')
environment = {k.upper(): v for k, v in os.environ.items()} if os.name == 'nt' else dict(os.environ)
qt_bin = root / '.tools/Qt/6.8.3/msvc2022_64/bin'
if os.name == 'nt' and qt_bin.exists():
    environment['PATH'] = str(qt_bin) + os.pathsep + environment.get('PATH', '')
result = subprocess.run([str(args.server.resolve()), '--config', str(args.config.resolve()), '--add-user', args.username],
                        input=(password + '\n').encode('utf-8'), env=environment)
del password
raise SystemExit(result.returncode)
