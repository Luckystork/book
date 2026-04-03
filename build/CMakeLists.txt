cmake_minimum_required(VERSION 3.10)
project(ZeroPoint)

set(CMAKE_CXX_STANDARD 17)

# Adjust based on your cross-compilation environment for Windows (e.g., MinGW-w64)
# This assumes compilation on a Windows host or configured cross-compiler.

add_executable(ZeroPoint_Launcher 
    src/main.cpp 
    src/Stealth.cpp 
    src/Hooks.cpp 
    src/CDPExtractor.cpp 
    src/Overlay.cpp
)

target_link_libraries(ZeroPoint_Launcher 
    user32 
    d2d1 
    dwrite
)
