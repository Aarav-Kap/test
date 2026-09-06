@echo off
setlocal
cd /d "%~dp0"
py -3 bridge.py once
pause
