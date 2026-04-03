import re

with open('src/VirtualEnv.cpp', 'r') as f:
    content = f.read()

# I need to ensure WS_EX_TRANSPARENT was actually added.
clickthrough_fix = """        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP,
        (sx - overlayW) / 2, (sy - overlayH) / 2, overlayW, overlayH,"""

new_clickthrough_fix = """        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        cls, NULL,
        WS_POPUP,
        (sx - overlayW) / 2, (sy - overlayH) / 2, overlayW, overlayH,"""

content = content.replace(clickthrough_fix, new_clickthrough_fix)

# WAIT
# The user wants "The overlay must be click-through except for the lock area."
# But a window with WS_EX_TRANSPARENT passes ALL clicks through.
# To ONLY catch clicks inside the overlay bounds, we just NOT use WS_EX_TRANSPARENT, 
# because a WS_POPUP window already only catches clicks inside its own bounds!
# And the lock area IS the overlay (520x220).
# Let's remove WS_EX_TRANSPARENT if it's there.

content = content.replace("WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT", "WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW")

with open('src/VirtualEnv.cpp', 'w') as f:
    f.write(content)

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Let's double check Proctor dots colors in main.cpp:
# Ensure green is RGB(0, 200, 100) and red is RGB(240, 60, 60)
color_fix = "HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(200, 100, 100));"
new_color_fix = "HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(240, 60, 60));"
content = content.replace(color_fix, new_color_fix)

with open('src/main.cpp', 'w') as f:
    f.write(content)

print("Patch 3 complete.")
