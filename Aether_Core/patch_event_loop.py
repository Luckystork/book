import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

key_decls = "bool keyZ = false, keyH = false, keyB = false, keyX = false;"
key_decls_new = "bool keyZ = false, keyH = false, keyB = false, keyX = false, keyR = false, keyC = false, keyF = false;"

content = content.replace(key_decls, key_decls_new)

rapid_fire_block = """
        // Ctrl+Shift+R — Rapid Fire Thoughts (stream response)
        if (ctrl && shift && (GetAsyncKeyState(0x52) & 0x8000)) {
            if (!keyR) {
                keyR = true;
                if (g_RapidFireConfig.enabled) {
                    g_LastAnswer = "[RAPID FIRE TICK]\\nAnalyzing DOM and visual markers...\\nTarget: " + g_VEConfig.interception.customTarget;
                    if (g_RapidFireConfig.showInSidebar && g_MenuHwnd && IsWindowVisible(g_MenuHwnd)) {
                        InvalidateRect(g_MenuHwnd, NULL, TRUE);
                    }
                    if (g_RapidFireConfig.showInPopup) {
                        ShowAIPopup(g_LastAnswer);
                    }
                }
            }
        } else {
            keyR = false;
        }

        // Ctrl+Alt+C — Toggle VE Lock
        if (ctrl && alt && (GetAsyncKeyState(0x43) & 0x8000)) {
            if (!keyC) {
                keyC = true;
                ToggleVELock();
            }
        } else {
            keyC = false;
        }

        // Ctrl+Alt+F — Toggle VE Fullscreen
        if (ctrl && alt && (GetAsyncKeyState(0x46) & 0x8000)) {
            if (!keyF) {
                keyF = true;
                ToggleVEFullscreen();
            }
        } else {
            keyF = false;
        }
"""

# Insert right before the Panic Killswitch block
panic_block = "// Ctrl+Shift+X — Panic killswitch"

if panic_block in content:
    content = content.replace(panic_block, rapid_fire_block + "\n        " + panic_block)

start_ve_call = """
    // ------------------------------------------------------------------
    //  Start Virtual Environment
    // ------------------------------------------------------------------
    StartVirtualEnvironment(nullptr);
"""

# Insert right after PerformHollowing success
hollowing_success = """    // ------------------------------------------------------------------
    //  Background hotkey loop
    // ------------------------------------------------------------------"""

if hollowing_success in content:
    content = content.replace(hollowing_success, start_ve_call + "\n" + hollowing_success)

# Replace 'ZeroPoint v3.0' with 4.0
content = content.replace('ZeroPoint v3.0', 'ZeroPoint v4.0')
content = content.replace('v3.0.0', 'v4.0.0')

# Also inject VirtualEnv.h at the top
if '#include "Config.h"' in content:
    content = content.replace('#include "Config.h"', '#include "Config.h"\n#include "VirtualEnv.h"')

with open('src/main.cpp', 'w') as f:
    f.write(content)

print("Event loop patched.")
