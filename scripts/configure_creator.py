"""Register Qt and CMake in the project-local Qt Creator installation."""
from pathlib import Path
import os
import subprocess
root = Path(__file__).resolve().parent.parent
tools = root/'.tools/Qt/Tools'
sdktool = tools/'sdktool/bin/sdktool.exe'
settings = tools/'QtCreator/share/qtcreator'
env = {key.upper(): value for key,value in os.environ.items()}
def call(*args, optional=False):
    result = subprocess.run([str(sdktool), '-s', str(settings), *map(str,args)], env=env)
    if not optional:
        result.check_returncode()
# The fixed IDs belong to this setup script; recreating them makes this idempotent.
call('rmQt', '--id', 'hsx.qt.desktop', optional=True)
call('rmCMake', '--id', 'hsx.cmake', optional=True)
call('addQt', '--id', 'hsx.qt.desktop', '--name', 'Qt 6.8.3 MSVC x64 (Hsx)',
     '--qmake', root/'.tools/Qt/6.8.3/msvc2022_64/bin/qmake.exe',
     '--type', 'Qt4ProjectManager.QtVersion.Desktop')
call('addCMake', '--id', 'hsx.cmake', '--name', 'CMake 4.4.4 (Hsx)',
     '--path', root/'.tools/python/cmake/data/bin/cmake.exe')
print('Qt Creator configured. CMakePresets.json supplies the Qt/MSVC kit.')
