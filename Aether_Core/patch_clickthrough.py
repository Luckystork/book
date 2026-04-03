import re

with open('src/VirtualEnv.cpp', 'r') as f:
    content = f.read()

# Make overlay click-through
clickthrough_fix = """    g_VELockOverlay = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP,
        (sx - overlayW) / 2, (sy - overlayH) / 2, overlayW, overlayH,
        NULL, NULL, GetModuleHandle(NULL), NULL);"""

new_clickthrough_fix = """    g_VELockOverlay = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        cls, NULL,
        WS_POPUP,
        (sx - overlayW) / 2, (sy - overlayH) / 2, overlayW, overlayH,
        NULL, NULL, GetModuleHandle(NULL), NULL);"""

content = content.replace(clickthrough_fix, new_clickthrough_fix)

with open('src/VirtualEnv.cpp', 'w') as f:
    f.write(content)

print("Clickthrough patched.")
