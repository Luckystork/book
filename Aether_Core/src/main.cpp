#include "CDPExtractor.h"
#include "Stealth.h"
#include <iostream>
#include <windows.h>


int main() {
  // API Key check (first run)
  int keyResult =
      MessageBoxA(NULL,
                  "ZeroPoint Launcher\n\nNo API key detected.\n\nPaste your "
                  "Anthropic API key now\n(or Cancel to exit):",
                  "ZeroPoint - First Launch", MB_OKCANCEL | MB_ICONINFORMATION);

  if (keyResult == IDCANCEL)
    return 0;

  // Inject stealth
  int injectResult =
      MessageBoxA(NULL,
                  "ZeroPoint is ready.\n\nClick OK to INJECT stealth "
                  "mode.\n\nHotkeys:\nCtrl+Shift+Z = Snapshot\nCtrl+Alt+H = "
                  "Full Menu\nCtrl+Shift+X = Panic",
                  "ZeroPoint Launcher", MB_OK | MB_ICONINFORMATION);

  if (injectResult == IDOK) {
    if (PerformHollowing()) {
      MessageBoxA(NULL,
                  "Stealth ACTIVATED.\nHollowed into svchost.exe.\nYou are now "
                  "invisible.",
                  "ZeroPoint", MB_OK | MB_ICONINFORMATION);
    } else {
      MessageBoxA(NULL, "Hollowing failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
      return 1;
    }
  }

  // Background hotkey loop
  while (true) {
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x5A) & 0x8000)) { // Ctrl+Shift+Z
      std::string question = ExtractBluebookDOM();
      // TODO: send 'question' to WinHTTP AI call → get answer + scribble
      // Display in bottom-right persistent overlay
    }

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000) &&
        (GetAsyncKeyState(0x48) & 0x8000)) { // Ctrl+Alt+H
                                             // ToggleFullMenu();
    }

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x58) & 0x8000)) { // Ctrl+Shift+X
      // PanicKillAndWipe();
      break;
    }

    Sleep(10);
  }
  return 0;
}