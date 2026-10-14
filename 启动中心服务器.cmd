@echo off
cd /d "%~dp0"
"%~dp0bin\im_server.exe" --config "%~dp0server\config\local.json"
pause
