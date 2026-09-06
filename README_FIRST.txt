NspireAI v0.5 = AUTOMATIC USB + FULL-SCREEN UI

Once built:
- calculator artifact -> nspire_ai.tns
- windows-bridge artifact -> nspire-ai-bridge.exe + SET_KEY_AND_RUN.bat

Usage:
1. Replace your old nspire_ai.tns with the new v0.5 one in /NspireAI.
2. On Windows, unzip windows-bridge.
3. Double-click SET_KEY_AND_RUN.bat and paste your Gemini API key.
4. Leave that window open.
5. Plug calculator in by USB.
6. Open NspireAI.
7. Type directly in the full-screen UI and press Enter.
8. The PC automatically reads request.tns, calls Gemini, writes response.tns.
9. The calculator polls response.tns and displays it automatically.
No WebTILP transfers during normal use.

Note:
Native USB access on Windows depends on the calculator being accessible through
libusb/WinUSB. If the bridge says it cannot initialize USB even though WebTILP
works, Windows may need a WinUSB/libusb driver binding for the calculator.
Do not change drivers unless needed; test the bridge first.


v0.5.1 build fix:
The Windows GitHub runner now installs libusb through vcpkg and statically links it,
instead of using the old libusb1-sys vendored extraction path that failed on Windows.


v0.5.2 build fix:
Adds the vcpkg libusb include and library paths explicitly so libnspire-sys can find libusb.h on the Windows GitHub runner.
