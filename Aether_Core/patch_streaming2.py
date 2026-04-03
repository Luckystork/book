import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Fix Rapid Fire Streaming (actually this part is fine, I just need to check the dropdown placeholder)
# Actually, let's fix the placeholder in the Settings
settings_block = """            RECT rt4 = {px+8, py+25, px+290, py+40};
            SetTextColor(memDC, g_TextSecondary); DrawTextA(memDC, g_VEConfig.interception.customTarget.empty() ? "e.g. Examplify.exe" : g_VEConfig.interception.customTarget.c_str(), -1, &rt4, DT_LEFT);"""

new_settings_block = """            RECT rt4 = {px+8, py+25, px+290, py+40};
            SetTextColor(memDC, g_TextSecondary); DrawTextA(memDC, g_VEConfig.interception.customTarget.empty() ? "e.g. Examplify.exe" : g_VEConfig.interception.customTarget.c_str(), -1, &rt4, DT_LEFT);"""
            
# Let's check Local Resources Raw Hardware tooltip matching.
resources_block = """            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextSecondary);
            RECT rr2 = {px, py, w-20, py+40}; DrawTextA(memDC, "Leave on screen remote audio (raw physical hardware mode) to be entirely undetectable.", -1, &rr2, DT_LEFT|DT_WORDBREAK);"""

new_resources_block = """            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextSecondary);
            RECT rr2 = {px, py, w-20, py+40}; DrawTextA(memDC, "Leave on screen remote audio (raw physical hardware mode) to be entirely undetectable.", -1, &rr2, DT_LEFT|DT_WORDBREAK);"""

print("Settings block already matched.")
