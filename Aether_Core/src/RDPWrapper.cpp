// ============================================================================
//  ZeroPoint — RDPWrapper.cpp  (v4.1)
//  RDP Wrapper auto-download, install, configure, and session management.
//
//  Flow:
//    1. Check if RDP Wrapper is installed (look for rdpwrap.dll)
//    2. If not: download from GitHub, extract, install service
//    3. Download latest rdpwrap.ini for current Windows build
//    4. Add Defender exclusions via PowerShell
//    5. Restart TermService to load the wrapper
//    6. Create hidden loopback RDP sessions via mstsc /v:127.0.0.2
//
//  The wrapper patches termsrv.dll in-memory to allow concurrent sessions.
//  All communication stays on 127.0.0.2 (loopback) — never exposed to LAN.
// ============================================================================

#include "../include/RDPWrapper.h"
#include "../include/Config.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <tlhelp32.h>
#include <wtsapi32.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wtsapi32.lib")

static const char* RDPWRAP_DIR      = "C:\\Program Files\\RDP Wrapper";
static const char* RDPWRAP_DLL      = "C:\\Program Files\\RDP Wrapper\\rdpwrap.dll";
static const char* RDPWRAP_INI      = "C:\\Program Files\\RDP Wrapper\\rdpwrap.ini";
static const char* RDPWRAP_INSTALL  = "C:\\Program Files\\RDP Wrapper\\RDPWInst.exe";
static const char* RDP_SESSION_FILE = "C:\\ProgramData\\ZeroPoint\\ve_session.rdp";

// Track our mstsc PID so we only kill our own sessions
static DWORD g_OurMstscPid = 0;

// ============================================================================
//  Status Check
// ============================================================================

RDPWrapState CheckRDPWrapperStatus() {
    if (GetFileAttributesA(RDPWRAP_DLL) == INVALID_FILE_ATTRIBUTES)
        return RDPWRAP_NOT_INSTALLED;

    // Check if the INI exists and is reasonably sized (>5KB = populated)
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(RDPWRAP_INI, GetFileExInfoStandard, &fad))
        return RDPWRAP_INSTALLED_OUTDATED;

    ULONGLONG iniSize = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    if (iniSize < 5000)
        return RDPWRAP_INSTALLED_OUTDATED;

    // Check if TermService is running
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceA(scm, "TermService", SERVICE_QUERY_STATUS);
        if (svc) {
            SERVICE_STATUS ss;
            if (QueryServiceStatus(svc, &ss) && ss.dwCurrentState == SERVICE_RUNNING) {
                CloseServiceHandle(svc);
                CloseServiceHandle(scm);
                return RDPWRAP_INSTALLED_READY;
            }
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }

    return RDPWRAP_INSTALLED_OUTDATED;
}

// ============================================================================
//  HTTP Download Helper — download a file from HTTPS to disk
// ============================================================================

static bool DownloadFileImpl(const wchar_t* host, const wchar_t* path,
                             const std::string& saveTo, int depth) {
    // Guard against infinite redirects
    if (depth > 5) return false;

    HINTERNET hSession = WinHttpOpen(L"ZeroPoint/4.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Check for redirects (GitHub uses 302)
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &statusSize, NULL);

    if (statusCode == 301 || statusCode == 302) {
        WCHAR location[2048] = {};
        DWORD locSize = sizeof(location);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, NULL,
                                location, &locSize, NULL)) {
            URL_COMPONENTS uc = {};
            uc.dwStructSize = sizeof(uc);
            WCHAR hostBuf[256] = {}, pathBuf[1024] = {};
            uc.lpszHostName = hostBuf; uc.dwHostNameLength = 256;
            uc.lpszUrlPath = pathBuf;  uc.dwUrlPathLength = 1024;
            if (WinHttpCrackUrl(location, 0, 0, &uc)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return DownloadFileImpl(hostBuf, pathBuf, saveTo, depth + 1);
            }
        }
    }

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::ofstream out(saveTo, std::ios::binary);
    if (!out.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        out.write(buffer, bytesRead);
        bytesRead = 0;
    }

    out.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

static bool DownloadFile(const wchar_t* host, const wchar_t* path,
                         const std::string& saveTo) {
    return DownloadFileImpl(host, path, saveTo, 0);
}

// ============================================================================
//  Run a command and wait for completion
// ============================================================================

static bool RunCommand(const std::string& cmd, bool hide = true, DWORD timeoutMs = 30000) {
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    if (hide) {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(NULL, const_cast<char*>(cmd.c_str()),
                        NULL, NULL, FALSE,
                        hide ? CREATE_NO_WINDOW : 0,
                        NULL, NULL, &si, &pi)) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);

    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0)
        GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (waitResult == WAIT_OBJECT_0);
}

// ============================================================================
//  Windows Defender Exclusions
// ============================================================================

bool AddDefenderExclusions() {
    std::string cmd =
        "powershell.exe -NoProfile -Command \""
        "Add-MpPreference -ExclusionPath 'C:\\Program Files\\RDP Wrapper' -Force; "
        "Add-MpPreference -ExclusionPath 'C:\\ProgramData\\ZeroPoint' -Force; "
        "Add-MpPreference -ExclusionProcess 'RDPWInst.exe' -Force; "
        "Add-MpPreference -ExclusionProcess 'ZeroPoint.exe' -Force\"";
    return RunCommand(cmd, true, 15000);
}

// ============================================================================
//  Restart TermService
// ============================================================================

bool RestartTermService() {
    RunCommand("net stop TermService /y", true, 15000);
    Sleep(500);
    return RunCommand("net start TermService", true, 15000);
}

// ============================================================================
//  Update rdpwrap.ini from GitHub
// ============================================================================

bool UpdateRDPWrapperINI(void (*statusCallback)(const char* msg)) {
    if (statusCallback) statusCallback("Downloading latest rdpwrap.ini...");

    std::string tempINI = std::string(RDPWRAP_DIR) + "\\rdpwrap_new.ini";

    bool ok = DownloadFile(
        L"raw.githubusercontent.com",
        L"/sebaxakerHTB/rdpwrap.ini/master/rdpwrap.ini",
        tempINI);

    if (!ok) {
        if (statusCallback) statusCallback("Failed to download rdpwrap.ini");
        return false;
    }

    // Validate downloaded file is non-empty
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(tempINI.c_str(), GetFileExInfoStandard, &fad) ||
        fad.nFileSizeLow < 1000) {
        DeleteFileA(tempINI.c_str());
        if (statusCallback) statusCallback("Downloaded INI is invalid");
        return false;
    }

    // Atomic replace: delete old, rename new
    DeleteFileA(RDPWRAP_INI);
    if (!MoveFileA(tempINI.c_str(), RDPWRAP_INI)) {
        // MoveFile failed — try copy + delete
        CopyFileA(tempINI.c_str(), RDPWRAP_INI, FALSE);
        DeleteFileA(tempINI.c_str());
    }

    if (statusCallback) statusCallback("rdpwrap.ini updated successfully");
    return true;
}

// ============================================================================
//  Full Install
// ============================================================================

bool InstallRDPWrapper(void (*statusCallback)(const char* msg)) {
    if (statusCallback) statusCallback("Creating RDP Wrapper directory...");
    CreateDirectoryA(RDPWRAP_DIR, NULL);

    // Defender exclusions first
    if (statusCallback) statusCallback("Adding Windows Defender exclusions...");
    AddDefenderExclusions();
    Sleep(1000);

    // Download installer
    if (statusCallback) statusCallback("Downloading RDP Wrapper installer...");
    bool dlOk = DownloadFile(
        L"github.com",
        L"/stascorp/rdpwrap/releases/latest/download/RDPWInst.exe",
        RDPWRAP_INSTALL);

    if (!dlOk) {
        if (statusCallback) statusCallback("Failed to download RDPWInst.exe");
        return false;
    }

    // Run installer
    if (statusCallback) statusCallback("Installing RDP Wrapper Library...");
    std::string installCmd = std::string("\"") + RDPWRAP_INSTALL + "\" -i -o";
    if (!RunCommand(installCmd, true, 60000)) {
        if (statusCallback) statusCallback("RDPWInst.exe failed to run");
        return false;
    }
    Sleep(2000);

    // Update INI
    if (!UpdateRDPWrapperINI(statusCallback)) {
        if (statusCallback) statusCallback("Warning: INI update failed, using bundled version");
    }

    // Restart TermService
    if (statusCallback) statusCallback("Restarting Terminal Services...");
    RestartTermService();
    Sleep(1500);

    // Verify
    RDPWrapState state = CheckRDPWrapperStatus();
    if (state == RDPWRAP_INSTALLED_READY) {
        if (statusCallback) statusCallback("RDP Wrapper installed and ready!");
        return true;
    }

    if (statusCallback) statusCallback("RDP Wrapper installed but may need a reboot");
    return true;
}

// ============================================================================
//  Session Management
// ============================================================================

DWORD CreateHiddenRDPSession(int width, int height, int colorDepth) {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);

    // Get current username and computer name for loopback connection
    char username[256] = {};
    DWORD usernameLen = sizeof(username);
    GetUserNameA(username, &usernameLen);

    char compName[256] = {};
    DWORD compLen = sizeof(compName);
    GetComputerNameA(compName, &compLen);

    // Write .rdp file
    std::ofstream rdp(RDP_SESSION_FILE);
    if (!rdp.is_open()) return 0;

    rdp << "full address:s:127.0.0.2\n";
    rdp << "username:s:" << username << "\n";
    rdp << "domain:s:" << compName << "\n";
    rdp << "desktopwidth:i:" << width << "\n";
    rdp << "desktopheight:i:" << height << "\n";
    rdp << "session bpp:i:" << colorDepth << "\n";
    rdp << "winposstr:s:0,1,0,0," << width << "," << height << "\n";
    rdp << "screen mode id:i:1\n";
    rdp << "smart sizing:i:1\n";
    rdp << "dynamic resolution:i:1\n";
    rdp << "authentication level:i:0\n";
    rdp << "prompt for credentials:i:0\n";
    rdp << "negotiate security layer:i:0\n";
    rdp << "enablecredsspsupport:i:0\n";
    rdp << "disable wallpaper:i:0\n";
    rdp << "allow font smoothing:i:1\n";
    rdp << "allow desktop composition:i:1\n";
    rdp << "redirectclipboard:i:" << (g_VEConfig.resources.clipboard ? 1 : 0) << "\n";
    rdp << "redirectdrives:i:" << (g_VEConfig.resources.localDrives ? 1 : 0) << "\n";
    rdp << "redirectprinters:i:" << (g_VEConfig.resources.printers ? 1 : 0) << "\n";
    rdp << "connection type:i:" << (g_VEConfig.resources.connectionQual + 1) << "\n";
    rdp << "networkautodetect:i:0\n";
    rdp << "autoreconnection enabled:i:1\n";
    rdp.close();

    // Launch mstsc
    std::string cmd = "mstsc.exe \"" + std::string(RDP_SESSION_FILE) + "\"";
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(NULL, const_cast<char*>(cmd.c_str()),
                        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return 0;
    }

    WaitForInputIdle(pi.hProcess, 5000);

    DWORD pid = pi.dwProcessId;
    g_OurMstscPid = pid;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return pid;
}

bool DestroyRDPSession(DWORD sessionPid) {
    if (sessionPid == 0) return false;

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, sessionPid);
    if (hProc) {
        TerminateProcess(hProc, 0);
        CloseHandle(hProc);

        if (g_OurMstscPid == sessionPid)
            g_OurMstscPid = 0;

        // Clean up the temp .rdp file
        DeleteFileA(RDP_SESSION_FILE);
        return true;
    }
    return false;
}

// Find the mstsc window by enumerating top-level windows
struct FindMstscData {
    DWORD targetPid;
    HWND  result;
};

static BOOL CALLBACK EnumMstscProc(HWND hwnd, LPARAM lp) {
    FindMstscData* data = (FindMstscData*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == data->targetPid) {
        char cls[256] = {};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (strstr(cls, "TscShell") || strstr(cls, "RAIL") || IsWindowVisible(hwnd)) {
            data->result = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND FindRDPSessionWindow(DWORD sessionPid) {
    FindMstscData data = { sessionPid, NULL };
    for (int retry = 0; retry < 30 && !data.result; retry++) {
        EnumWindows(EnumMstscProc, (LPARAM)&data);
        if (!data.result) Sleep(300);
    }
    return data.result;
}

// ============================================================================
//  Cleanup
// ============================================================================

bool UninstallRDPWrapper() {
    if (GetFileAttributesA(RDPWRAP_INSTALL) != INVALID_FILE_ATTRIBUTES) {
        std::string cmd = std::string("\"") + RDPWRAP_INSTALL + "\" -u";
        RunCommand(cmd, true, 30000);
    }
    try {
        std::filesystem::remove_all(RDPWRAP_DIR);
    } catch (...) {}
    return true;
}

void KillAllRDPSessions() {
    // Only kill mstsc processes that we spawned (by tracking our PID)
    // If g_OurMstscPid is set, only kill that one. Otherwise fall back to
    // killing all mstsc.exe (emergency cleanup).
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    DWORD ourPid = GetCurrentProcessId();

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "mstsc.exe") == 0) {
                // Kill if it's our tracked PID, or if it's a child of our process
                bool shouldKill = false;
                if (g_OurMstscPid != 0 && pe.th32ProcessID == g_OurMstscPid)
                    shouldKill = true;
                else if (pe.th32ParentProcessID == ourPid)
                    shouldKill = true;
                else if (g_OurMstscPid == 0)
                    shouldKill = true;  // emergency fallback: kill all

                if (shouldKill) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        CloseHandle(hProc);
                    }
                }
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    g_OurMstscPid = 0;
}
