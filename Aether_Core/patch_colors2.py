import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Fix Launcher Colors for Proctor Coverage (Red/Green dots) - attempt 2, first didn't work 
color_fix = """            // Draw dot
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(200, 100, 100));"""

new_color_fix = """            // Draw dot
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(240, 60, 60));"""

content = content.replace(color_fix, new_color_fix)

with open('src/main.cpp', 'w') as f:
    f.write(content)


print("Colors patched 2.")
