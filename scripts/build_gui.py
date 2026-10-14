"""Build and deploy the Qt GUI using the project's local SDK."""
from pathlib import Path
import os
import subprocess
import sys
import shutil
sys.stdout.reconfigure(encoding='utf-8')
root = Path(__file__).resolve().parent.parent
qt = root / '.tools/Qt/6.8.3/msvc2022_64'
cmake = root / '.tools/python/cmake/data/bin/cmake.exe'
if not cmake.exists() or not (qt/'bin/qmake.exe').exists():
    raise SystemExit('Qt SDK / CMake missing. Run setup_qt.py bootstrap and sdk first.')
env = {k.upper(): v for k, v in os.environ.items()}
env['PATH'] = str(qt/'bin') + os.pathsep + env.get('PATH', '')
env['PYTHONIOENCODING'] = 'utf-8'
def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=root, env=env, check=True)
run(cmake, '--preset', 'qt-desktop', '-DIM_DESKTOP_OUTPUT='+str(root/'bin'))
run(cmake, '--build', '--preset', 'qt-release', '--parallel', '4', '--', '/nodeReuse:false')
shutil.copy2(root/'build-gui/Release/im_system.exe', root/'bin/im_system.exe')
shutil.copy2(root/'build-gui/server/Release/im_server.exe', root/'bin/im_server.exe')
run(qt/'bin/windeployqt.exe', '--release', '--no-translations', '--no-opengl-sw', root/'bin/im_desktop.exe')
run(qt/'bin/windeployqt.exe', '--release', '--no-translations', root/'bin/im_server.exe')
run(cmake.parent/'ctest.exe', '--test-dir', root/'build-gui', '-C', 'Release', '--output-on-failure')
if '--test' in sys.argv:
    from datetime import datetime
    out = root/'tests/gui-output'/datetime.now().strftime('%Y%m%d-%H%M%S')
    out.mkdir(parents=True, exist_ok=False)
    run(root/'bin/im_desktop.exe', '--self-test', out)
    print((out/'results.json').read_text(encoding='utf-8'))
print('Ready: bin/im_desktop.exe')
