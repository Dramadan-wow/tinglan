"""Install a project-local Qt SDK and CMake without changing global settings."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
local = root / ".tools"
local.mkdir(exist_ok=True)
packages = local / "python"
temp = local / "tmp"
temp.mkdir(exist_ok=True)
env = {k.upper(): v for k, v in os.environ.items()}
for key in ["PIP_NO_INDEX", "PIP_INDEX_URL", "PIP_EXTRA_INDEX_URL", "PIP_FIND_LINKS", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY"]:
    env.pop(key, None)
env.update(PYTHONIOENCODING="utf-8", TEMP=str(temp), TMP=str(temp), PYTHONPATH=str(packages))
def call(args):
    subprocess.run([sys.executable, *args], env=env, cwd=root, check=True)

if len(sys.argv) < 2 or sys.argv[1] == "bootstrap":
    call(["-m", "pip", "install", "--disable-pip-version-check", "--index-url", "https://pypi.org/simple",
          "--target", str(packages), "--cache-dir", str(local/"cache"), "--no-warn-script-location", "aqtinstall", "cmake"])
elif sys.argv[1] == "list":
    call(["-m", "aqt", "list-qt", "windows", "desktop", "--spec", "6.8"])
    call(["-m", "aqt", "list-tool", "windows", "desktop", "tools_qtcreator", "-l"])
elif sys.argv[1] == "sdk":
    version = sys.argv[2] if len(sys.argv) > 2 else "6.8.3"
    try:
        call(["-m", "aqt", "install-qt", "windows", "desktop", version, "win64_msvc2022_64", "-O", str(local/"Qt"), "--archives", "qtbase", "qttools"])
    except subprocess.CalledProcessError:
        # aqt 3.3.0 assumes UTF-8 for qmake output, which uses the Windows code page.
        # Only accept the extracted SDK after repair_qt.py verifies the exact version.
        if version != "6.8.3" or not (local/"Qt"/version/"msvc2022_64/bin/Qt6Core.dll").exists():
            raise
    call([str(root/"scripts/repair_qt.py")])
elif sys.argv[1] == "tools":
    result = subprocess.run([sys.executable, '-m', 'aqt', 'list-tool', 'windows', 'desktop'], env=env, cwd=root, capture_output=True, text=True, check=True)
    print('\n'.join(line for line in result.stdout.splitlines() if 'creator' in line or 'sdktool' in line))
elif sys.argv[1] == "creator":
    repo = sys.argv[2] if len(sys.argv)>2 else 'tools_qtcreator_gui'
    variant = 'qt.tools.qtcreator_gui' if repo=='tools_qtcreator_gui' else 'qt.tools.qtcreator'
    call(["-m", "aqt", "install-tool", "windows", "desktop", repo, variant, "-O", str(local/"Qt")])
elif sys.argv[1] == "creator-list":
    call(["-m", "aqt", "list-tool", "windows", "desktop", sys.argv[2], "-l"])
elif sys.argv[1] == "sdktool":
    call(["-m", "aqt", "install-tool", "windows", "desktop", "sdktool", "qt.tools.qtcreator", "-O", str(local/"Qt")])
elif sys.argv[1] == "multimedia":
    try:
        call(["-m", "aqt", "install-qt", "windows", "desktop", "6.8.3", "win64_msvc2022_64", "-O", str(local/"Qt"), "--modules", "qtmultimedia", "--archives", "qtmultimedia"])
    except subprocess.CalledProcessError:
        if not (local/"Qt/6.8.3/msvc2022_64/bin/Qt6Multimedia.dll").exists():
            raise
    call([str(root/"scripts/repair_qt.py")])
