@echo off
setlocal
cd /d "%~dp0"
echo.
echo === NspireAI Gemini setup ===
echo Use a NEW Gemini API key from Google AI Studio.
echo Do NOT paste the key into ChatGPT, screenshots, or config.json.
echo It is stored only for this Command Prompt session.
echo.
set /p GEMINI_API_KEY=Gemini API key: 
echo.
echo Testing Gemini 3.6 Flash...
py -3 bridge.py test "Explain the chain rule in 3 short steps."
echo.
echo If you got a real answer, Gemini is working.
echo Keep THIS window open if you want the key to remain available.
echo.
cmd /k
