#pragma once
#include <windows.h>
#include <string>

namespace Stealth {
    bool CloakWindow(HWND hwnd);
    bool InjectGhostProcess(const std::string& targetProcess, void* payloadBuffer, size_t payloadSize);
}
