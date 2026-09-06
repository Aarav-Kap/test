NspireAI v0.6
=============

What changed
------------
- Better chat-style UI on the calculator.
- Uses persistent mailbox files: uplink.tns and downlink.tns.
- No WebTILP file shuffling during normal use.
- The answer appears in the same screen.
- The Windows bridge writes "thinking" first, then progressively fills the answer.

How to use
----------
1. Replace your repo contents with this ZIP's contents.
2. Commit and push.
3. Run the GitHub Action "Build NspireAI v0.6".
4. Download BOTH artifacts:
   - calculator
   - windows-bridge
5. Replace the calculator's old nspire_ai.tns with the new one.
6. Extract windows-bridge on Windows.
7. Run SET_KEY_AND_RUN.bat and paste a NEW Gemini key.
8. Plug in the calculator and open NspireAI.
9. Type in the built-in UI and press Enter.

Notes
-----
- This version still uses TI's normal file-transport under the hood, but it is hidden.
- If the bridge prints "Calculator connected." but never "Prompt: ...", the calculator app is likely not in the /NspireAI folder. Keep nspire_ai.tns in a folder named exactly NspireAI.
