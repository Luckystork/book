import re

with open('src/VirtualEnv.cpp', 'r') as f:
    content = f.read()

# Fix VirtualEnv.h not found
content = content.replace('#include "VirtualEnv.h"', '#include "../include/VirtualEnv.h"')

# Fix RDPWrapper.h not found  
content = content.replace('#include "RDPWrapper.h"', '#include "../include/RDPWrapper.h"')

# Fix Config.h not found
content = content.replace('#include "Config.h"', '#include "../include/Config.h"')

with open('src/VirtualEnv.cpp', 'w') as f:
    f.write(content)

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Fix Stealth.h
content = content.replace('#include "Stealth.h"', '#include "../include/Stealth.h"')
content = content.replace('#include "CDPExtractor.h"', '#include "../include/CDPExtractor.h"')
content = content.replace('#include "Config.h"', '#include "../include/Config.h"')
content = content.replace('#include "VirtualEnv.h"', '#include "../include/VirtualEnv.h"')

with open('src/main.cpp', 'w') as f:
    f.write(content)

print("Include paths patched.")
