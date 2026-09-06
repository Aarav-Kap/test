@echo off
echo.
echo This sets OPENAI_API_KEY for THIS Command Prompt session only.
echo Your key will not be written to config.json.
echo.
set /p OPENAI_API_KEY=Paste your OpenAI API key here: 
echo.
echo Key set for this window.
echo Now run:
echo   py -3 bridge.py test "What is 2+2?"
echo.
cmd /k
