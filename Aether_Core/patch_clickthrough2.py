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

# WAIT
# The instructions say: "The overlay must be click-through except for the lock area."
# If I use WS_EX_TRANSPARENT, it's click-through EVERYTHING, which means clicking it WON'T unlock it.
# To make it click-through everywhere EXCEPT the lock area, I should either:
# 1) NOT use WS_EX_TRANSPARENT, but use hit testing (WM_NCHITTEST) to return HTTRANSPARENT where it shouldn't be clickable.

# Actually, the user says:
# "When the VE is locked (Ctrl+Alt+C), create a fully transparent frosted-glass layered overlay (WS_EX_LAYERED + high alpha) centered on the VE desktop."
# "The overlay must be click-through except for the lock area."
# Let's remove the WS_EX_TRANSPARENT flag if it was added. Instead, add WM_NCHITTEST.

clickthrough_proc_fix = """    case WM_LBUTTONUP:
        // Click on the overlay — unlock the VE
        UnlockVE();
        return 0;"""

new_clickthrough_proc_fix = """    case WM_NCHITTEST: {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        RECT rc;
        GetWindowRect(hwnd, &rc);
        // Let's say the whole window is the "lock area" since it's centered 520x220,
        // wait, the bounding box of the overlay IS the lock area. 
        // Oh, maybe they want a fullscreen transparent overlay that only catches clicks in the center?
        // Ah: "centered on the VE desktop. The overlay must be click-through except for the lock area."
        // If the window is only 520x220, the rest of the screen is naturally click-through because the window doesn't cover it.
        // But if they wanted it to be fullscreen, then.
        // Let's check how the overlay is created. It is 520x220 in the center.
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    
    case WM_LBUTTONUP:
        // Click on the overlay — unlock the VE
        UnlockVE();
        return 0;"""

# Wait, if the overlay is 520x220 and created with WS_POPUP, it only covers 520x220. The rest of the screen is ALREADY click-through.

print("Everything is fine.")
