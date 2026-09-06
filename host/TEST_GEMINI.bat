@echo off
setlocal
cd /d "%~dp0"
if "%GEMINI_API_KEY%"=="" (
  echo GEMINI_API_KEY is not set in this window.
  echo Run SET_GEMINI_KEY.bat instead.
  pause
  exit /b 1
)
py -3 bridge.py test "Explain why acceleration due to gravity is approximately constant near Earth's surface."
pause
