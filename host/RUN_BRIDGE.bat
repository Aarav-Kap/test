@echo off
setlocal
cd /d "%~dp0"
echo Starting NspireAI bridge...
py -3 bridge.py watch
pause
