#include "Stealth.h"
#include "CDPExtractor.h"
#include <windows.h>
#include <iostream>

// Clean dark launcher using native Windows dialogs (no ugly console)

int main() {
    // === API KEY CHECK (first run only) ===
    int keyResult = MessageBoxA(NULL, 
        "ZeroPoint Launcher\n\nNo API key detected.\n\nPaste your Anthropic (or other) API key now\n(or press Cancel to exit):", 
        "ZeroPoint - First Launch", MB_OKCANCEL | MB_ICONINFORMATION);

    if (keyResult == IDCANCEL) {
        return 0;
    }

    // (Your Config.cpp already saves it to C:\ProgramData\ZeroPoint\config.ini)

    // === MAIN LAUNCHER ===
    int injectResult = MessageBoxA(NULL, 
        "ZeroPoint is ready.\n\nClick OK to INJECT stealth mode.\n\nHotkeys after inject:\n"
        "Ctrl + Shift + Z   → Snapshot (discreet bottom-right answer)\n"
        "Ctrl + Alt + H     → Full frosted menu\n"
        "Ctrl + Shift + X   → Panic killswitch", 
        "ZeroPoint Launcher", MB_OK | MB_ICONINFORMATION);

    if (injectResult == IDOK) {
        if (PerformHollowing()) {
            MessageBoxA(NULL, "Stealth mode ACTIVATED.\nHollowed into svchost.exe.\nYou are now invisible.", 
                        "ZeroPoint", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL, "Hollowing failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    // === BACKGROUND HOTKEY LOOP ===
    while (true) {
        // Ctrl+Shift+Z = Snapshot
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) && 
            (GetAsyncKeyState(0x5A) & 0x8000)) {   // Z key
            std::string question = ExtractBluebookDOM();
            // TODO: send 'question' to your AI networking code
            // Show answer + scribble in bottom-right persistent overlay
            // Sync to full menu if open
        }

        // Ctrl+Alt+H = Full menu
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_MENU) & 0x8000) && 
            (GetAsyncKeyState(0x48) & 0x8000)) {   // H key
            // ToggleFullMenu();   // your existing menu function
        }

        // Ctrl+Shift+X = Panic
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) && 
            (GetAsyncKeyState(0x58) & 0x8000)) {   // X key
            // PanicKillAndWipe();
            break;
        }

        Sleep(10);
    }

    return 0;
}