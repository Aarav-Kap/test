WINDOWS HOST
============

The host bridge needs only Python 3.11+ for v0.1.

FIRST:
  Double-click FIRST_TEST.bat

You should see:
  PC bridge is working.

The test creates:
  exchange\request.tns
  exchange\response.tns

PROVIDER: MOCK
--------------
config.json begins with:
  "provider": "mock"

No internet and no API key are needed.

PROVIDER: GEMINI (RECOMMENDED / FREE-TIER)
-------------------------------------------
1. Create a Gemini API key in Google AI Studio.
2. config.json is already set to provider=gemini.
3. Double-click SET_GEMINI_KEY.bat.
4. Paste the key when asked.
5. It immediately runs a real Gemini test.

Default model: gemini-3.6-flash

PROVIDER: OPENAI
----------------
1. Get an API key from the OpenAI API platform.
2. API billing is separate from a ChatGPT Plus subscription.
3. Change config.json:
     "provider": "openai"
4. Set OPENAI_API_KEY in Windows.
   The included SET_OPENAI_KEY.bat sets it for one command window.
5. Test:
     py -3 bridge.py test "Explain the product rule."

Default model in this package:
  gpt-5.6-luna

You can change openai_model in config.json.

PROVIDER: OLLAMA
----------------
If you want the AI to run locally:
1. Install Ollama.
2. Pull a small model appropriate for your PC, e.g. qwen3:4b.
3. Change config.json:
     "provider": "ollama"
4. Ensure:
     "ollama_model": "qwen3:4b"
5. Test:
     py -3 bridge.py test "Explain kinetic energy."

RUN CONTINUOUSLY
----------------
Double-click RUN_BRIDGE.bat.

It watches:
  host\exchange\request.tns

and creates:
  host\exchange\response.tns

In v0.1 you transfer these two files manually between the calculator and this
folder with WebTILP. Automatic USB transport is the next milestone.
