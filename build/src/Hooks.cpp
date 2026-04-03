#include "Hooks.h"
#include <iostream>

HHOOK g_KeyboardHook = NULL;
bool g_Processing = false;

// Forward declarations of engine commands (resolved in engine modules)
extern void ExecuteSwiftCapture();    // Ctrl+Shift+Z
extern void ToggleEvadusMenu();       // Ctrl+Alt+H
extern void ExecutePanicKill();       // Ctrl+Shift+X

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool ctrlPressed = GetAsyncKeyState(VK_CONTROL) & 0x8000;
            bool shiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
            bool altPressed = GetAsyncKeyState(VK_MENU) & 0x8000;

            // Ctrl + Shift + Z: Silent Capture Auto-Send
            if (ctrlPressed && shiftPressed && pkbhs->vkCode == 'Z') {
                if (!g_Processing) {
                    ExecuteSwiftCapture();
                }
                return 1; // Block the key down event
            }
            
            // Ctrl + Alt + H: Summon UI
            if (ctrlPressed && altPressed && pkbhs->vkCode == 'H') {
                ToggleEvadusMenu();
                return 1;
            }

            // Ctrl + Shift + X: Panic Switch
            if (ctrlPressed && shiftPressed && pkbhs->vkCode == 'X') {
                ExecutePanicKill();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_KeyboardHook, nCode, wParam, lParam);
}

namespace Keybinder {
    bool InitializeHooks() {
        if (!g_KeyboardHook) {
            g_KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
            return g_KeyboardHook != NULL;
        }
        return true;
    }

    void RemoveHooks() {
        if (g_KeyboardHook) {
            UnhookWindowsHookEx(g_KeyboardHook);
            g_KeyboardHook = NULL;
        }
    }
}
