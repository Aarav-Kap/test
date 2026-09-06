@echo off
setlocal
cd /d "%~dp0"
echo.
echo === NspireAI PC-only test ===
echo This uses the MOCK provider. It costs nothing and needs no API key.
echo.
py -3 bridge.py test "Explain why the derivative of x^2 is 2x."
if errorlevel 1 (
  echo.
  echo Python may not be installed or available as "py".
  echo Install Python 3.11 or newer from python.org, then run this again.
  pause
  exit /b 1
)
echo.
echo If you saw "PC bridge is working", this part is good.
pause
