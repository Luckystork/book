#include "CDPExtractor.h"
#include "Stealth.h"
#include <iostream>
#include <string>
#include <windows.h>


// Placeholder for your Config and Overlay functions (add your real headers)
extern bool SaveAPIKey(const std::string &key);
extern void DisplayAnswerInCorner(const std::string &answer);
extern void ToggleFullMenu();
extern void PanicKillAndWipe();

int main() {
  // First run / API key
  int keyResult =
      MessageBoxA(NULL,
                  "ZeroPoint Launcher\n\nNo API key detected.\n\nPaste your "
                  "Anthropic API key:",
                  "ZeroPoint - First Launch", MB_OKCANCEL | MB_ICONINFORMATION);

  if (keyResult == IDCANCEL)
    return 0;

  // Save key (your Config.cpp handles encryption)
  // SaveAPIKey(userInputHere);   // replace with real input later

  // Main launcher
  int injectResult =
      MessageBoxA(NULL,
                  "ZeroPoint is ready.\n\nClick OK to INJECT stealth "
                  "mode.\n\nHotkeys:\nCtrl+Shift+Z = Snapshot\nCtrl+Alt+H = "
                  "Full Menu\nCtrl+Shift+X = Panic",
                  "ZeroPoint Launcher", MB_OK | MB_ICONINFORMATION);

  if (injectResult == IDOK) {
    if (PerformHollowing()) {
      MessageBoxA(NULL, "Stealth ACTIVATED.\nHollowed successfully.",
                  "ZeroPoint", MB_OK | MB_ICONINFORMATION);
    } else {
      MessageBoxA(NULL, "Hollowing failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
      return 1;
    }
  }

  // Background loop
  while (true) {
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x5A) & 0x8000)) { // Ctrl+Shift+Z
      std::string question = ExtractBluebookDOM();
      // TODO: replace with real WinHTTP call to Claude
      std::string answer = "13 D\n2x=8 → x=4"; // placeholder
      DisplayAnswerInCorner(answer);
    }

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000) &&
        (GetAsyncKeyState(0x48) & 0x8000)) { // Ctrl+Alt+H
      ToggleFullMenu();
    }

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x58) & 0x8000)) { // Ctrl+Shift+X
      PanicKillAndWipe();
      break;
    }

    Sleep(10);
  }
  return 0;
}