@echo off
cd /d "%~dp0"
start "" "%~dp0dist\v5.4.0\Tinglan-5.4.0-Portable.exe" --admin --data "%LOCALAPPDATA%\Hsx Coursework\Tinglan-admin-cache.txt"
