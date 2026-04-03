import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Fix Launcher Colors for Proctor Coverage (Red/Green dots)
color_fix = """            // Draw dot
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(200, 100, 100));"""

new_color_fix = """            // Draw dot
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(240, 60, 60));"""

content = content.replace(color_fix, new_color_fix)

with open('src/main.cpp', 'w') as f:
    f.write(content)

with open('ZeroPoint_Installer.iss', 'r') as f:
    content = f.read()

content = content.replace('WizardForm.Color := $F8FAFC;', 'WizardForm.Color := $00EBECE8;')
content = content.replace('WizardForm.MainPanel.Color := $F8FAFC;', 'WizardForm.MainPanel.Color := $00EBECE8;')
content = content.replace('WizardForm.PageNameLabel.Font.Color := $FFDD00;', 'WizardForm.PageNameLabel.Font.Color := $00DDFF;')

with open('ZeroPoint_Installer.iss', 'w') as f:
    f.write(content)

print("Colors patched.")
