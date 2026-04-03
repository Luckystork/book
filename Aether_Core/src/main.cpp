#include "CDPExtractor.h"
#include "Stealth.h"
#include <iostream>
#include <windows.h>

// Placeholder for your existing overlay/config/hotkey setup
// (add your real headers here if you have Overlay.h, Config.h, etc.)

int main() {
  std::cout << "[ZeroPoint] Launcher started - click Inject to begin...\n";

  // When user clicks "Inject" in launcher
  if (PerformHollowing()) {
    std::cout << "[ZeroPoint] Successfully hollowed into svchost.exe - stealth "
                 "active\n";
  } else {
    std::cout << "[ZeroPoint] Hollowing failed\n";
    return 1;
  }

  // Hotkey loop (replace with your real WH_KEYBOARD_LL handler)
  while (true) {
    // Ctrl+Shift+Z pressed
    if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState(VK_SHIFT) &
        GetAsyncKeyState(0x5A)) { // 0x5A = Z
      std::string question = ExtractBluebookDOM();
      std::cout << "[ZeroPoint] Bluebook question extracted: "
                << question.substr(0, 100) << "...\n";
      // TODO: send 'question' to your AI (WinHTTP to Claude), get answer +
      // scribble Display persistently in bottom-right overlay Sync to full menu
      // if open
    }

    // Ctrl+Alt+H = full menu
    if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState(VK_MENU) &
        GetAsyncKeyState(0x48)) { // 0x48 = H
      std::cout << "[ZeroPoint] Full Evadus-style menu opened\n";
      // ToggleFullMenu();
    }

    // Ctrl+Shift+X = panic
    if (GetAsyncKeyState(VK_CONTROL) & GetAsyncKeyState(VK_SHIFT) &
        GetAsyncKeyState(0x58)) { // 0x58 = X
      std::cout << "[ZeroPoint] PANIC - killing process and wiping tracks\n";
      // PanicKillAndWipe();
      break;
    }

    Sleep(10);
  }

  return 0;
}