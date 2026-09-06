@echo off
cd /d "%~dp0"
echo NspireAI v0.5
echo Paste a NEW Gemini API key. It is only kept in this window.
set /p GEMINI_API_KEY=Gemini API key: 
echo.
nspire-ai-bridge.exe
pause
