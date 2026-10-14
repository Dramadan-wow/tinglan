@echo off
set "PATH=%~dp0.tools\Qt\6.8.3\msvc2022_64\bin;%~dp0.tools\python\cmake\data\bin;%PATH%"
start "" "%~dp0.tools\Qt\Tools\QtCreator\bin\qtcreator.exe" -settingspath "%~dp0.tools\QtCreatorSettings" "%~dp0CMakeLists.txt"
