"""Use Qt's relative configuration to support Chinese paths without binary patching."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
qt = root / '.tools/Qt/6.8.3/msvc2022_64'
assert (qt / 'bin/Qt6Core.dll').exists(), 'Qt SDK not downloaded'
(qt / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\n', encoding='utf-8')
for name in ('qmake', 'qmake6', 'qtpaths', 'qtpaths6'):
    file = qt / 'bin' / (name + '.bat')
    if file.exists():
        file.write_text('@echo off\r\n"%~dp0' + name + '.exe" %*\r\n', encoding='ascii')
result = subprocess.run([str(qt/'bin/qmake.exe'), '-query', 'QT_VERSION'], capture_output=True, check=True)
assert result.stdout.strip() == b'6.8.3', result.stdout
print('Qt 6.8.3 configured; relative prefix supports Unicode paths.')
