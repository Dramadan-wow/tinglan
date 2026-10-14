@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist "bin\im_system.exe" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\build.ps1"
  if errorlevel 1 (
    pause
    exit /b 1
  )
)
"bin\im_system.exe"
if errorlevel 1 pause
