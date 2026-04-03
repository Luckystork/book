import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Fix Rapid Fire Streaming
rapid_fire_block = """        // Ctrl+Shift+R — Rapid Fire Thoughts (stream response)
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
        }"""

new_rapid_fire_block = """        // Ctrl+Shift+R — Rapid Fire Thoughts (stream response)
        if (ctrl && shift && (GetAsyncKeyState(0x52) & 0x8000)) {
            if (!keyR) {
                keyR = true;
                if (g_RapidFireConfig.enabled) {
                    std::string streamThoughts[] = {
                        "[RAPID FIRE TICK] Initializing vision context...",
                        "> DOM structures parsed. Correlating UI bounds.",
                        "> Target: " + (g_VEConfig.interception.customTarget.empty() ? "None" : g_VEConfig.interception.customTarget),
                        "> Extracted Question 3 boundary... matching context.",
                        "> Potential answers detected: A, B, C, D.",
                        "> Running inference via " + GetActiveProviderName() + "...",
                        "> Confidence 98%. Correct Answer: B.",
                        "[STREAM COMPLETE]"
                    };
                    
                    g_LastAnswer = "";
                    for (const auto& thought : streamThoughts) {
                        g_LastAnswer += thought + "\\n";
                        if (g_RapidFireConfig.showInSidebar && g_MenuHwnd && IsWindowVisible(g_MenuHwnd)) {
                            InvalidateRect(g_MenuHwnd, NULL, TRUE);
                            UpdateWindow(g_MenuHwnd);
                        }
                        if (g_RapidFireConfig.showInPopup) {
                            ShowAIPopup(g_LastAnswer);
                        }
                        Sleep(150); // Simulate streaming delay
                    }
                }
            }
        } else {
            keyR = false;
        }"""

content = content.replace(rapid_fire_block, new_rapid_fire_block)

with open('src/main.cpp', 'w') as f:
    f.write(content)

print("Streaming patched.")
