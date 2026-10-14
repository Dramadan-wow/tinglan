@echo off
set "PATH=%~dp0.tools\Qt\6.8.3\msvc2022_64\bin;%~dp0.tools\python\cmake\data\bin;%PATH%"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "QT_VS_ROOT=%%i"
if defined QT_VS_ROOT call "%QT_VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64
cd /d "%~dp0"
cmd /k "qmake -v"
