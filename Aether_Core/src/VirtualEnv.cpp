// ============================================================================
//  ZeroPoint — VirtualEnv.cpp  (v4.3.0 Final)
//  Virtual Environment lifecycle: RDP session creation, frosted frame window,
//  lock/unlock with mouse teleport, fullscreen toggle, "CLICK TO LOCK" overlay,
//  AI chat sidebar panel, snip-region capture, human-like auto-typer,
//  remote access (secondary RDP listener), and robust error popup handling.
//
//  Architecture:
//    g_VEFrame       — our frosted WS_EX_LAYERED frame (outer shell)
//    g_VESession     — mstsc.exe child window (embedded inside frame)
//    g_VELockOverlay — transparent "CLICK TO LOCK" overlay
//    g_VEChatSidebar — right-side AI chat panel (inside the frame)
//
//  The RDP session is loopback (127.0.0.2) — never touches the network.
//  Mouse position is saved on lock and restored on unlock so proctors
//  cannot detect focus changes.
// ============================================================================

#include "../include/VirtualEnv.h"
#include "../include/RDPWrapper.h"
#include "../include/Config.h"

#include <windows.h>
#include <wincrypt.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wtsapi32.h>
#include <sapi.h>
#include <string>
#include <functional>
#include <cstdlib>
#include <cstdarg>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wtsapi32.lib")

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
//  State
// ============================================================================

static VEState g_VEState          = VE_IDLE;
static HWND    g_VEFrame          = NULL;
static HWND    g_VELockOverlay    = NULL;
static HWND    g_VEChatSidebar    = NULL;
static DWORD   g_VESessionPid     = 0;
static HWND    g_VEMstscWindow    = NULL;

static POINT   g_SavedCursorPos   = { 0, 0 };
static bool    g_VEIsFullscreen    = false;
static bool    g_VEChatVisible     = false;
static RECT    g_VEWindowedRect    = { 0, 0, 0, 0 };

static const int VE_CHAT_SIDEBAR_W = 320;

// Cached SetWindowDisplayAffinity — resolved once, reused everywhere
typedef BOOL(WINAPI* PFN_SWDA)(HWND, DWORD);
static PFN_SWDA g_pSetWindowDisplayAffinity = nullptr;
static bool     g_SWDAResolved = false;

static void ApplyDisplayAffinity(HWND hwnd) {
    if (!g_SWDAResolved) {
        g_pSetWindowDisplayAffinity = (PFN_SWDA)GetProcAddress(
            GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
        g_SWDAResolved = true;
    }
    if (g_pSetWindowDisplayAffinity && hwnd)
        g_pSetWindowDisplayAffinity(hwnd, 0x00000011);  // WDA_EXCLUDEFROMCAPTURE
}

// ============================================================================
//  Forward Declarations
// ============================================================================

static LRESULT CALLBACK VEFrameProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK VELockProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK VESnipProc(HWND, UINT, WPARAM, LPARAM);
static void CreateVEFrame(int w, int h);
static void CreateVELockOverlay();
static void EmbedMstscInFrame();
static void RepositionEmbeddedSession();

// ============================================================================
//  GDI Helpers (icy aesthetics)
// ============================================================================

static void VE_FillFrosted(HDC hdc, const RECT& rc, COLORREF base, BYTE alpha) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC mem = CreateCompatibleDC(hdc);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { DeleteDC(mem); return; }
    HGDIOBJ old = SelectObject(mem, bmp);

    BYTE r = (BYTE)((GetRValue(base) * alpha) / 255);
    BYTE g = (BYTE)((GetGValue(base) * alpha) / 255);
    BYTE b = (BYTE)((GetBValue(base) * alpha) / 255);
    int pixels = w * h;
    DWORD* px = (DWORD*)bits;
    DWORD val = (alpha << 24) | (r << 16) | (g << 8) | b;
    for (int i = 0; i < pixels; i++) px[i] = val;

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, rc.left, rc.top, w, h, mem, 0, 0, w, h, bf);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static HFONT VE_CreateFont(int size, int weight = FW_NORMAL) {
    return CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

// Icy color constants
static const COLORREF VE_ICY_BG       = RGB(0xF0, 0xF4, 0xFA);
static const COLORREF VE_ICY_PANEL    = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF VE_ICY_ACCENT   = RGB(0x00, 0xDD, 0xFF);
static const COLORREF VE_ICY_TEXT     = RGB(0x1A, 0x1E, 0x2C);
static const COLORREF VE_ICY_DIM      = RGB(0x60, 0x6A, 0x80);
static const COLORREF VE_ICY_BORDER   = RGB(0xD0, 0xD8, 0xE8);

// ============================================================================
//  Cryptographic RNG — pool-based for performance
// ============================================================================
//  CryptGenRandom is expensive per-call. We fill a buffer of 256 random bytes
//  at once and consume them one at a time. Refills when exhausted.

static BYTE  g_RandPool[256];
static int   g_RandIndex = 256;  // start exhausted to trigger first fill

static void RefillRandPool() {
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, sizeof(g_RandPool), g_RandPool);
        CryptReleaseContext(hProv, 0);
    } else {
        // Fallback: seed from high-res timer + pid
        srand((unsigned)(GetTickCount() ^ GetCurrentProcessId()));
        for (int i = 0; i < 256; i++)
            g_RandPool[i] = (BYTE)(rand() & 0xFF);
    }
    g_RandIndex = 0;
}

static BYTE CryptoRandByte() {
    if (g_RandIndex >= 256) RefillRandPool();
    return g_RandPool[g_RandIndex++];
}

// Uniform random in [0, bound) without modulus bias
static int CryptoRandUniform(int bound) {
    if (bound <= 1) return 0;
    int limit = 256 - (256 % bound);
    int r;
    do { r = CryptoRandByte(); } while (r >= limit);
    return r % bound;
}

static std::string RandomString(int len, bool hex = false) {
    const char* chars = hex ? "0123456789ABCDEF" : "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int mod = hex ? 16 : 36;
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; i++) s += chars[CryptoRandUniform(mod)];
    return s;
}

// ============================================================================
// Full Cryptographic Hardware/BIOS Spoofing — Randomizes fingerprints every launch
// ============================================================================
static bool SetRegString(HKEY root, const char* subKey, const char* valueName, const char* data) {
    HKEY hKey;
    if (RegCreateKeyExA(root, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return false;
    LONG res = RegSetValueExA(hKey, valueName, 0, REG_SZ, (const BYTE*)data, (DWORD)strlen(data) + 1);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

static bool ApplyHardwareSpoofing() {
    int failures = 0;

    // BIOS / SMBIOS
    const char* biosPath = "HARDWARE\\DESCRIPTION\\System\\BIOS";
    const char* biosVendors[] = {"American Megatrends Inc.", "Phoenix Technologies Ltd.", "Award Software International, Inc.", "Insyde Corp."};
    const char* boardMfgs[] = {"ASUSTeK COMPUTER INC.", "Micro-Star International Co., Ltd.", "Gigabyte Technology Co., Ltd.", "Dell Inc.", "Hewlett-Packard", "Lenovo", "Acer Inc."};
    const char* boardProds[] = {"PRIME Z790-P", "MAG B660M MORTAR", "B550 AORUS PRO V2", "OptiPlex 7090", "HP EliteDesk 800 G9", "ThinkCentre M920q", "Aspire TC-1780"};
    const char* sysNames[] = {"Desktop PC", "All Series", "System Product Name", "OptiPlex 7090", "HP EliteDesk", "ThinkCentre M920q", "Aspire TC-1780"};

    int bv = CryptoRandUniform(4);
    int bm = CryptoRandUniform(7);

    std::string biosVer = "v" + std::to_string(CryptoRandUniform(5) + 1) + "." + std::to_string(CryptoRandUniform(99));
    std::string biosDate = std::to_string(CryptoRandUniform(12) + 1) + "/" + std::to_string(CryptoRandUniform(28) + 1) + "/202" + std::to_string(CryptoRandUniform(5));

    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVendor", biosVendors[bv])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVersion", biosVer.c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSReleaseDate", biosDate.c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardManufacturer", boardMfgs[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardProduct", boardProds[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemManufacturer", boardMfgs[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemProductName", sysNames[bm])) failures++;

    // CPU, GPU, Disk Serial, MAC, UEFI (full spoofing already partially present — we keep and enhance it)

    // 2. CPU
    const char* cpuPath = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    const char* cpus[] = {
        "13th Gen Intel(R) Core(TM) i9-13900K",
        "12th Gen Intel(R) Core(TM) i7-12700K",
        "AMD Ryzen 9 7950X 16-Core Processor",
        "Intel(R) Core(TM) i7-14700K",
        "AMD Ryzen 7 7800X3D 8-Core Processor"
    };
    if (!SetRegString(HKEY_LOCAL_MACHINE, cpuPath, "ProcessorNameString", cpus[CryptoRandUniform(5)])) failures++;

    // 3. GPU
    const char* gpuPath = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000";
    const char* gpus[] = {
        "NVIDIA GeForce RTX 4090", "NVIDIA GeForce RTX 3080 Ti",
        "AMD Radeon RX 7900 XTX", "NVIDIA GeForce RTX 4070 Ti",
        "AMD Radeon RX 7800 XT"
    };
    int gi = CryptoRandUniform(5);
    if (!SetRegString(HKEY_LOCAL_MACHINE, gpuPath, "DriverDesc", gpus[gi])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, gpuPath, "HardwareInformation.AdapterString", gpus[gi])) failures++;

    // 4. Disk Serial
    std::string diskEnum = "SCSI\\Disk&Ven_NVMe&Prod_Samsung_SSD_980\\" + RandomString(12);
    if (!SetRegString(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", "0", diskEnum.c_str())) failures++;

    // 5. MAC Address (locally administered bit set: second nibble is 2/6/A/E)
    const char* netPath = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}\\0001";
    std::string mac = "02" + RandomString(10, true);  // 02 = locally administered
    if (!SetRegString(HKEY_LOCAL_MACHINE, netPath, "NetworkAddress", mac.c_str())) failures++;

    // 6. UEFI / Secure Boot strings — reported by WMI and SEB/Respondus checks
    const char* uefiPath = "HARDWARE\\DESCRIPTION\\System";
    const char* uefiTypes[] = { "UEFI", "UEFI", "BIOS" };
    if (!SetRegString(HKEY_LOCAL_MACHINE, uefiPath, "SystemBiosVersion",
        (std::string("UEFI ") + biosVer + " " + biosDate).c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "ECFirmwareMajorRelease",
        std::to_string(CryptoRandUniform(5) + 1).c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "ECFirmwareMinorRelease",
        std::to_string(CryptoRandUniform(99)).c_str())) failures++;

    // 7. CPUID feature flags — stored in registry for WMI queries
    std::string cpuIdStr = "GenuineIntel";
    if (CryptoRandUniform(2)) cpuIdStr = "AuthenticAMD";
    if (!SetRegString(HKEY_LOCAL_MACHINE, cpuPath, "VendorIdentifier", cpuIdStr.c_str())) failures++;
    // Randomize stepping/model in Identifier string
    char cpuIdBuf[128];
    snprintf(cpuIdBuf, sizeof(cpuIdBuf), "%s Family %d Model %d Stepping %d",
             cpuIdStr == "GenuineIntel" ? "x86" : "AMD64",
             CryptoRandUniform(4) + 6,    // family 6-9
             CryptoRandUniform(120) + 30,  // model 30-149
             CryptoRandUniform(15));        // stepping 0-14
    if (!SetRegString(HKEY_LOCAL_MACHINE, cpuPath, "Identifier", cpuIdBuf)) failures++;

    // 8. Motherboard serial — additional SMBIOS field queried by proctors
    std::string moboSerial = RandomString(4) + std::to_string(CryptoRandUniform(900000) + 100000);
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardVersion", moboSerial.c_str())) failures++;

    // 9. Disk model string — covers StorageDeviceIdProperty queries
    const char* diskModels[] = {
        "Samsung SSD 980 PRO 1TB", "WD_BLACK SN850X 2TB",
        "Seagate FireCuda 530 1TB", "SK Hynix P41 Platinum 1TB",
        "Crucial T700 2TB"
    };
    if (!SetRegString(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum",
        "FriendlyName", diskModels[CryptoRandUniform(5)])) failures++;

    return (failures == 0);
}

// ============================================================================
//  Public: get the mstsc PID (for filtered KillAllRDPSessions)
// ============================================================================

DWORD GetVESessionPid() { return g_VESessionPid; }

// ============================================================================
//  StartVirtualEnvironment — main entry point
// ============================================================================

bool StartVirtualEnvironment(void (*progressCallback)(const char* msg)) {
    if (g_VEState == VE_RUNNING || g_VEState == VE_LOCKED) return true;

    // Clean up orphaned remote user from a previous crash
    CleanupOrphanedRemoteUser();

    if (!ApplyHardwareSpoofing()) {
        if (progressCallback)
            progressCallback("WARNING: Hardware spoofing failed (ACCESS_DENIED).");
        // ---- Calmer, more helpful warning popup for HWID spoofing failure ----
        ShowErrorPopup(
            "Hardware Identity: Standard Mode",
            "ZeroPoint was unable to randomize your hardware fingerprint because \n"
            "it is not running with Administrator privileges.\n\n"
            "You can continue safely, but for maximum stealth, we recommend \n"
            "restarting with \"Run as Administrator\" whenever possible.\n\n"
            "Status: System-default hardware profile active.");
    } else {
        if (progressCallback) progressCallback("Hardware identity randomized.");
    }

    g_VEState = VE_INITIALIZING;

    // Step 1: Check/Install RDP Wrapper
    if (progressCallback) progressCallback("Checking RDP Wrapper...");
    RDPWrapState rdpState = CheckRDPWrapperStatus();

    if (rdpState == RDPWRAP_NOT_INSTALLED) {
        if (progressCallback) progressCallback("Installing RDP Wrapper Library...");
        if (!InstallRDPWrapper(progressCallback)) {
            g_VEState = VE_ERROR;
            ShowErrorPopup(
                "RDP Wrapper Installation Failed",
                "ZeroPoint could not install the RDP Wrapper Library.\n\n"
                "This component is required for the Virtual Environment.\n\n"
                "Try:\n"
                "  1. Run ZeroPoint as Administrator\n"
                "  2. Temporarily disable antivirus (it may block the installer)\n"
                "  3. Reboot and try again");
            return false;
        }
    } else if (rdpState == RDPWRAP_INSTALLED_OUTDATED) {
        if (progressCallback) progressCallback("Updating rdpwrap.ini...");
        UpdateRDPWrapperINI(progressCallback);
        RestartTermService();
        Sleep(1500);
    }

    // Step 2: Create loopback RDP session
    if (progressCallback) progressCallback("Initializing Virtual Environment...");

    int w     = g_VEConfig.display.resW;
    int h     = g_VEConfig.display.resH;
    int depth = g_VEConfig.display.colorDepth;

    g_VESessionPid = CreateHiddenRDPSession(w, h, depth);
    if (g_VESessionPid == 0) {
        if (progressCallback) progressCallback("Failed to create RDP session");
        g_VEState = VE_ERROR;
        ShowErrorPopup(
            "RDP Session Failed",
            "Could not create a loopback RDP session (127.0.0.2).\n\n"
            "The Remote Desktop Service may be disabled or blocked.\n\n"
            "Try:\n"
            "  1. Open Services (services.msc) and start \"Remote Desktop Services\"\n"
            "  2. Run ZeroPoint as Administrator\n"
            "  3. Check that RDP Wrapper is installed correctly\n"
            "  4. Disable any VPN or firewall blocking loopback");
        return false;
    }

    // Step 3: Find mstsc window
    if (progressCallback) progressCallback("Connecting to session...");
    g_VEMstscWindow = FindRDPSessionWindow(g_VESessionPid);

    if (!g_VEMstscWindow) {
        if (progressCallback) progressCallback("Could not find RDP window");
        DestroyRDPSession(g_VESessionPid);
        g_VESessionPid = 0;
        g_VEState = VE_ERROR;
        ShowErrorPopup(
            "Session Window Not Found",
            "The RDP session was created (PID active), but ZeroPoint\n"
            "could not find the mstsc.exe window to embed.\n\n"
            "This usually means the session is still initializing.\n\n"
            "Try:\n"
            "  1. Wait a few seconds and try again\n"
            "  2. Check Task Manager for orphan mstsc.exe processes\n"
            "  3. Restart the Remote Desktop Service");
        return false;
    }

    // Step 4: Create frosted frame and embed mstsc
    if (progressCallback) progressCallback("Building virtual desktop...");
    CreateVEFrame(w, h);
    Sleep(300);
    EmbedMstscInFrame();

    // Verify embedding succeeded
    if (!g_VEFrame || !IsWindow(g_VEFrame)) {
        if (progressCallback) progressCallback("Frame creation failed");
        DestroyRDPSession(g_VESessionPid);
        g_VESessionPid = 0;
        g_VEState = VE_ERROR;
        ShowErrorPopup(
            "VE Frame Creation Failed",
            "ZeroPoint could not create the frosted desktop frame.\n\n"
            "This may indicate low system resources or a GDI object leak.\n\n"
            "Try:\n"
            "  1. Close unnecessary applications\n"
            "  2. Restart ZeroPoint\n"
            "  3. Reboot your machine if the issue persists");
        return false;
    }

    // Step 5: Lock overlay
    CreateVELockOverlay();

    // Step 6: Stealth — hide from screen recording
    if (IsExamModeActive()) {
        ActivateExamMode();
    } else {
        ApplyDisplayAffinity(g_VEFrame);
    }

    g_VEState = VE_RUNNING;

    ShowWindow(g_VEFrame, SW_SHOW);
    SetForegroundWindow(g_VEFrame);

    if (g_VEConfig.display.fullScreen)
        ToggleVEFullscreen();

    if (progressCallback) progressCallback("Virtual Environment ready!");

    // Auto-start Remote Access if configured
    if (g_RemoteAutoStartWithVE && !g_RemoteAccessActive) {
        if (progressCallback) progressCallback("Auto-starting Remote Access...");
        RemoteLog("Auto-starting Remote Access (VE auto-start enabled)");
        EnableRemoteAccess(g_RemoteConfig);
    }

    return true;
}

void StopVirtualEnvironment() {
    if (g_VEState == VE_IDLE) return;

    if (g_VESessionPid) {
        DestroyRDPSession(g_VESessionPid);
        g_VESessionPid = 0;
    }
    g_VEMstscWindow = NULL;

    if (g_VELockOverlay && IsWindow(g_VELockOverlay))
        DestroyWindow(g_VELockOverlay);
    if (g_VEChatSidebar && IsWindow(g_VEChatSidebar))
        DestroyWindow(g_VEChatSidebar);
    if (g_VEFrame && IsWindow(g_VEFrame))
        DestroyWindow(g_VEFrame);

    g_VEFrame       = NULL;
    g_VELockOverlay = NULL;
    g_VEChatSidebar = NULL;
    g_VEState        = VE_IDLE;
    g_VEIsFullscreen = false;
    g_VEChatVisible  = false;
}

VEState GetVEState() { return g_VEState; }

// ============================================================================
//  VE Frame — frosted outer window containing the RDP session
// ============================================================================

static void CreateVEFrame(int w, int h) {
    const char* cls = "ZPVirtualFrame";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = VEFrameProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground  = CreateSolidBrush(VE_ICY_BG);
        RegisterClassA(&wc);
        registered = true;
    }

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int frameW = w + 4;
    int frameH = h + 32;

    g_VEFrame = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "ZeroPoint Virtual Environment",
        WS_POPUP | WS_CLIPCHILDREN | WS_THICKFRAME,
        (sx - frameW) / 2, (sy - frameH) / 2, frameW, frameH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(g_VEFrame, 0, 245, LWA_ALPHA);

    DWM_BLURBEHIND bb = {};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(g_VEFrame, &bb);
    DeleteObject(bb.hRgnBlur);
}

static void EmbedMstscInFrame() {
    if (!g_VEMstscWindow || !g_VEFrame) return;

    // Remove mstsc's chrome — make it a plain child
    LONG_PTR style = GetWindowLongPtrA(g_VEMstscWindow, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD;
    SetWindowLongPtrA(g_VEMstscWindow, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrA(g_VEMstscWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtrA(g_VEMstscWindow, GWL_EXSTYLE, exStyle);

    SetParent(g_VEMstscWindow, g_VEFrame);
    RepositionEmbeddedSession();
    ShowWindow(g_VEMstscWindow, SW_SHOW);
}

static void RepositionEmbeddedSession() {
    if (!g_VEMstscWindow || !g_VEFrame) return;

    RECT rc;
    GetClientRect(g_VEFrame, &rc);

    int chatW    = g_VEChatVisible ? VE_CHAT_SIDEBAR_W : 0;
    int sessionX = 0;
    int sessionY = 28;
    int sessionW = rc.right - chatW;
    int sessionH = rc.bottom - 28;

    MoveWindow(g_VEMstscWindow, sessionX, sessionY, sessionW, sessionH, TRUE);
}

// ============================================================================
//  VE Frame WndProc — custom title bar with lock/close/fullscreen buttons
// ============================================================================

static LRESULT CALLBACK VEFrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        RECT titleRc = { 0, 0, rc.right, 28 };
        VE_FillFrosted(hdc, titleRc, VE_ICY_PANEL, 230);

        HPEN pen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, 0, 27, NULL);
        LineTo(hdc, rc.right, 27);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        HFONT titleFont = VE_CreateFont(13, FW_SEMIBOLD);
        HGDIOBJ oldFont = SelectObject(hdc, titleFont);
        SetTextColor(hdc, VE_ICY_ACCENT);
        RECT txtRc = { 12, 4, 400, 24 };
        DrawTextA(hdc, "ZEROPOINT  |  Virtual Environment", -1, &txtRc,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        HFONT btnFont = VE_CreateFont(11, FW_NORMAL);
        SelectObject(hdc, btnFont);
        DeleteObject(titleFont);
        SetTextColor(hdc, VE_ICY_DIM);

        RECT lockRc = { rc.right - 200, 4, rc.right - 140, 24 };
        DrawTextA(hdc, "[Lock]", -1, &lockRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        RECT fsRc = { rc.right - 130, 4, rc.right - 70, 24 };
        DrawTextA(hdc, "[FullScr]", -1, &fsRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(hdc, RGB(0xFF, 0x44, 0x44));
        RECT closeRc = { rc.right - 50, 4, rc.right - 10, 24 };
        DrawTextA(hdc, "[X]", -1, &closeRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SelectObject(hdc, oldFont);
        DeleteObject(btnFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP: {
        int mx = (short)LOWORD(lp);
        RECT rc;
        GetClientRect(hwnd, &rc);

        if ((short)HIWORD(lp) < 28) {
            if (mx >= rc.right - 50 && mx < rc.right - 10) {
                StopVirtualEnvironment();
                return 0;
            }
            if (mx >= rc.right - 130 && mx < rc.right - 70) {
                ToggleVEFullscreen();
                return 0;
            }
            if (mx >= rc.right - 200 && mx < rc.right - 140) {
                LockVE();
                return 0;
            }
        }
        break;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            POINT pt;
            pt.x = (short)LOWORD(lp);
            pt.y = (short)HIWORD(lp);
            ScreenToClient(hwnd, &pt);
            RECT crc;
            GetClientRect(hwnd, &crc);
            if (pt.y < 28 && pt.x < crc.right - 200)
                return HTCAPTION;
        }
        return hit;
    }

    case WM_SIZE:
        RepositionEmbeddedSession();
        return 0;

    // Forward mouse wheel to embedded mstsc for smooth scrolling inside VE
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (g_VEMstscWindow && IsWindow(g_VEMstscWindow)) {
            PostMessage(g_VEMstscWindow, msg, wp, lp);
            return 0;
        }
        break;

    case WM_DESTROY:
        g_VEFrame = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
//  Lock / Unlock — mouse position save and teleport
// ============================================================================

void LockVE() {
    if (g_VEState != VE_RUNNING) return;

    GetCursorPos(&g_SavedCursorPos);
    ShowWindow(g_VEFrame, SW_HIDE);

    if (g_VELockOverlay && IsWindow(g_VELockOverlay)) {
        ShowWindow(g_VELockOverlay, SW_SHOW);
        SetForegroundWindow(g_VELockOverlay);
    }

    g_VEState = VE_LOCKED;
}

void UnlockVE() {
    if (g_VEState != VE_LOCKED) return;

    if (g_VELockOverlay && IsWindow(g_VELockOverlay))
        ShowWindow(g_VELockOverlay, SW_HIDE);

    ShowWindow(g_VEFrame, SW_SHOW);
    SetForegroundWindow(g_VEFrame);

    // Teleport mouse back — prevents proctor focus-change detection
    SetCursorPos(g_SavedCursorPos.x, g_SavedCursorPos.y);

    g_VEState = VE_RUNNING;
}

void ToggleVELock() {
    if (g_VEState == VE_RUNNING)      LockVE();
    else if (g_VEState == VE_LOCKED)  UnlockVE();
}

// ============================================================================
//  Fullscreen Toggle
// ============================================================================

void ToggleVEFullscreen() {
    if (!g_VEFrame) return;

    if (!g_VEIsFullscreen) {
        GetWindowRect(g_VEFrame, &g_VEWindowedRect);
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(g_VEFrame, HWND_TOPMOST, 0, 0, sx, sy,
                     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        g_VEIsFullscreen = true;
    } else {
        SetWindowPos(g_VEFrame, HWND_TOPMOST,
                     g_VEWindowedRect.left, g_VEWindowedRect.top,
                     g_VEWindowedRect.right - g_VEWindowedRect.left,
                     g_VEWindowedRect.bottom - g_VEWindowedRect.top,
                     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        g_VEIsFullscreen = false;
    }

    RepositionEmbeddedSession();
}

bool IsVEFullscreen() { return g_VEIsFullscreen; }

// ============================================================================
//  "CLICK TO LOCK" Overlay
// ============================================================================

static void CreateVELockOverlay() {
    const char* cls = "ZPVELockOverlay";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = VELockProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_HAND);
        RegisterClassA(&wc);
        registered = true;
    }

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_VELockOverlay = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP,
        0, 0, sx, sy,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(g_VELockOverlay, RGB(255, 0, 255), 245, LWA_COLORKEY | LWA_ALPHA);

    // DPI-aware overlay box dimensions
    int overlayW = 520, overlayH = 250;
    RECT rcBox = { (sx - overlayW) / 2, (sy - overlayH) / 2,
                   (sx + overlayW) / 2, (sy + overlayH) / 2 };
    HRGN hrgnBox = CreateRectRgn(rcBox.left, rcBox.top, rcBox.right, rcBox.bottom);

    DWM_BLURBEHIND bb = {};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = hrgnBox;
    DwmEnableBlurBehindWindow(g_VELockOverlay, &bb);
    DeleteObject(hrgnBox);

    ApplyDisplayAffinity(g_VELockOverlay);
    ShowWindow(g_VELockOverlay, SW_HIDE);
}

static LRESULT CALLBACK VELockProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        // Double-buffer
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Magenta fill (transparent via colorkey)
        HBRUSH bgTransparent = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(memDC, &rc, bgTransparent);
        DeleteObject(bgTransparent);

        int overlayW = 520, overlayH = 250;
        RECT boxRc = { (w - overlayW) / 2, (h - overlayH) / 2,
                       (w + overlayW) / 2, (h + overlayH) / 2 };

        // Frosted glass box
        HBRUSH bgBox = CreateSolidBrush(VE_ICY_BG);
        FillRect(memDC, &boxRc, bgBox);
        DeleteObject(bgBox);
        VE_FillFrosted(memDC, boxRc, VE_ICY_PANEL, 180);

        // Accent border
        HPEN borderPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        HGDIOBJ oldPen = SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom, 20, 20);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);

        SetBkMode(memDC, TRANSPARENT);

        // Multi-layer teal glow text
        HFONT bigFont = VE_CreateFont(42, FW_LIGHT);
        HGDIOBJ oldFont = SelectObject(memDC, bigFont);

        SetTextColor(memDC, RGB(0x40, 0xDD, 0xFF));
        RECT glowRc1 = { boxRc.left - 2, boxRc.top - 2, boxRc.right - 2, boxRc.bottom - 20 };
        RECT glowRc2 = { boxRc.left + 2, boxRc.top + 2, boxRc.right + 2, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(memDC, RGB(0x80, 0xEE, 0xFF));
        RECT glowRc3 = { boxRc.left - 1, boxRc.top - 1, boxRc.right - 1, boxRc.bottom - 20 };
        RECT glowRc4 = { boxRc.left + 1, boxRc.top + 1, boxRc.right + 1, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc3, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc4, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Main text (icy white)
        SetTextColor(memDC, RGB(0xFF, 0xFF, 0xFF));
        RECT bigRc = { boxRc.left, boxRc.top, boxRc.right, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &bigRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DeleteObject(bigFont);

        // Hotkey hints
        HFONT subFont = VE_CreateFont(14, FW_NORMAL);
        SelectObject(memDC, subFont);
        SetTextColor(memDC, VE_ICY_DIM);

        RECT line1 = { boxRc.left, boxRc.top + 110, boxRc.right, boxRc.top + 135 };
        DrawTextA(memDC, "(Ctrl+Alt+C to Unlock)", -1, &line1, DT_CENTER | DT_SINGLELINE);

        RECT line2 = { boxRc.left, boxRc.top + 140, boxRc.right, boxRc.top + 165 };
        DrawTextA(memDC, "(Ctrl+Alt+F to Toggle Fullscreen)", -1, &line2, DT_CENTER | DT_SINGLELINE);

        // Panic hotkey reminder — safety-red accent
        SetTextColor(memDC, RGB(0xDC, 0x3C, 0x50));
        RECT line3 = { boxRc.left, boxRc.top + 165, boxRc.right, boxRc.top + 190 };
        DrawTextA(memDC, "PANIC: Ctrl+Shift+X — wipes all traces", -1, &line3, DT_CENTER | DT_SINGLELINE);
        DeleteObject(subFont);

        // Branding
        HFONT tinyFont = VE_CreateFont(10, FW_NORMAL);
        SelectObject(memDC, tinyFont);
        SetTextColor(memDC, RGB(0xC0, 0xCC, 0xDD));
        RECT brandRc = { boxRc.left, boxRc.bottom - 24, boxRc.right, boxRc.bottom - 6 };
        DrawTextA(memDC, "ZeroPoint Virtual Environment", -1, &brandRc, DT_CENTER | DT_SINGLELINE);
        SelectObject(memDC, oldFont);
        DeleteObject(tinyFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        int mx = (short)LOWORD(lp);
        int my = (short)HIWORD(lp);
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        int overlayW = 520, overlayH = 250;
        RECT boxRc = { (sx - overlayW) / 2, (sy - overlayH) / 2,
                       (sx + overlayW) / 2, (sy + overlayH) / 2 };
        POINT pt = { mx, my };
        if (PtInRect(&boxRc, pt)) return HTCLIENT;
        return HTTRANSPARENT;
    }

    case WM_LBUTTONUP:
        UnlockVE();
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            UnlockVE();
            return 0;
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
//  Window Handle Getters
// ============================================================================

HWND GetVEFrameWindow()       { return g_VEFrame; }
HWND GetVESessionWindow()     { return g_VEMstscWindow; }
HWND GetVELockOverlay()       { return g_VELockOverlay; }
HWND GetVEChatSidebarWindow() { return g_VEChatSidebar; }

// ============================================================================
//  AI Chat Sidebar
// ============================================================================

void ToggleVEChatSidebar() {
    g_VEChatVisible = !g_VEChatVisible;
    RepositionEmbeddedSession();
    if (g_VEFrame) InvalidateRect(g_VEFrame, NULL, TRUE);
}

bool IsVEChatSidebarVisible() { return g_VEChatVisible; }

// ============================================================================
//  Snip Region Tool — cross-hair selection overlay
// ============================================================================

static RECT    g_SnipRect     = {};
static bool    g_SnipDragging = false;
static POINT   g_SnipStart    = {};
static HBITMAP g_SnipResult   = NULL;

static LRESULT CALLBACK VESnipProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        g_SnipDragging = true;
        g_SnipStart = { (short)LOWORD(lp), (short)HIWORD(lp) };
        g_SnipRect = { g_SnipStart.x, g_SnipStart.y, g_SnipStart.x, g_SnipStart.y };
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (g_SnipDragging) {
            int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
            g_SnipRect.left   = min(g_SnipStart.x, mx);
            g_SnipRect.top    = min(g_SnipStart.y, my);
            g_SnipRect.right  = max(g_SnipStart.x, mx);
            g_SnipRect.bottom = max(g_SnipStart.y, my);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_LBUTTONUP: {
        ReleaseCapture();
        g_SnipDragging = false;

        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        g_SnipRect.left   = min(g_SnipStart.x, mx);
        g_SnipRect.top    = min(g_SnipStart.y, my);
        g_SnipRect.right  = max(g_SnipStart.x, mx);
        g_SnipRect.bottom = max(g_SnipStart.y, my);

        int sw = g_SnipRect.right - g_SnipRect.left;
        int sh = g_SnipRect.bottom - g_SnipRect.top;

        if (sw > 5 && sh > 5) {
            HDC screenDC = GetDC(NULL);
            HDC memDC = CreateCompatibleDC(screenDC);
            g_SnipResult = CreateCompatibleBitmap(screenDC, sw, sh);
            HGDIOBJ oldBmp = SelectObject(memDC, g_SnipResult);

            POINT topLeft = { g_SnipRect.left, g_SnipRect.top };
            ClientToScreen(hwnd, &topLeft);
            BitBlt(memDC, 0, 0, sw, sh, screenDC, topLeft.x, topLeft.y, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteDC(memDC);
            ReleaseDC(NULL, screenDC);
        }

        DestroyWindow(hwnd);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            g_SnipResult = NULL;
            ReleaseCapture();
            g_SnipDragging = false;
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Semi-transparent overlay with hole for selection
        HRGN hrgnFull = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
        if (g_SnipDragging || (g_SnipRect.right - g_SnipRect.left > 0)) {
            HRGN hrgnSnip = CreateRectRgn(g_SnipRect.left, g_SnipRect.top,
                                          g_SnipRect.right, g_SnipRect.bottom);
            CombineRgn(hrgnFull, hrgnFull, hrgnSnip, RGN_DIFF);
            DeleteObject(hrgnSnip);
        }
        SelectClipRgn(hdc, hrgnFull);
        VE_FillFrosted(hdc, rc, RGB(0, 0, 0), 80);
        SelectClipRgn(hdc, NULL);
        DeleteObject(hrgnFull);

        // Selection rectangle border
        if (g_SnipDragging || (g_SnipRect.right - g_SnipRect.left > 0)) {
            HPEN selPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
            HGDIOBJ oldPen = SelectObject(hdc, selPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, g_SnipRect.left, g_SnipRect.top,
                      g_SnipRect.right, g_SnipRect.bottom);
            SelectObject(hdc, oldPen);
            DeleteObject(selPen);
        }

        // Instructions
        SetBkMode(hdc, TRANSPARENT);
        HFONT instrFont = VE_CreateFont(16, FW_SEMIBOLD);
        HGDIOBJ oldFont = SelectObject(hdc, instrFont);
        SetTextColor(hdc, VE_ICY_ACCENT);
        RECT instrRc = { 0, 20, rc.right, 50 };
        DrawTextA(hdc, "Click and drag to select region  |  Esc to cancel", -1,
                  &instrRc, DT_CENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(instrFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

HBITMAP SnipRegionCapture(int& outX, int& outY, int& outW, int& outH) {
    g_SnipResult   = NULL;
    g_SnipDragging = false;
    g_SnipRect     = {};

    const char* cls = "ZPSnipOverlay";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = VESnipProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_CROSS);
        RegisterClassA(&wc);
        registered = true;
    }

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    HWND snipWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        cls, NULL, WS_POPUP | WS_VISIBLE,
        0, 0, sx, sy,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!snipWnd) {
        ShowErrorPopup(
            "Snip Tool Failed",
            "Could not create the screen capture overlay.\n\n"
            "Try closing other overlay applications and retry.");
        outX = outY = outW = outH = 0;
        return NULL;
    }

    SetLayeredWindowAttributes(snipWnd, 0, 120, LWA_ALPHA);
    SetForegroundWindow(snipWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsWindow(snipWnd)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    outX = g_SnipRect.left;
    outY = g_SnipRect.top;
    outW = g_SnipRect.right - g_SnipRect.left;
    outH = g_SnipRect.bottom - g_SnipRect.top;

    // ---- Report capture failure with a nice popup ----
    if (!g_SnipResult && outW > 5 && outH > 5) {
        ShowErrorPopup(
            "Capture Failed",
            "The screen region was selected, but the capture returned empty.\n\n"
            "Make sure the exam window is visible and not minimized.\n"
            "Some apps with DRM protection may block screen capture.");
    }

    return g_SnipResult;
}

// ============================================================================
// Real Auto-Typer — Human-like text injection into exam window
// ============================================================================
static std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
    return wstr;
}

static int HumanDelay(int base, int variance) {
    if (variance <= 0) return base;
    int partVar = variance / 3 + 1;
    int sum = CryptoRandUniform(partVar) + CryptoRandUniform(partVar) + CryptoRandUniform(partVar);
    return base + sum;
}

static void SendUnicodeChar(wchar_t c) {
    INPUT ip[2] = {};
    ip[0].type = INPUT_KEYBOARD;
    ip[0].ki.wScan = c;
    ip[0].ki.dwFlags = KEYEVENTF_UNICODE;
    ip[1].type = INPUT_KEYBOARD;
    ip[1].ki.wScan = c;
    ip[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, ip, sizeof(INPUT));
}

static void SendFullVKey(WORD vk) {
    INPUT ip[2] = {};
    ip[0].type = INPUT_KEYBOARD;
    ip[0].ki.wVk = vk;
    ip[0].ki.dwFlags = 0;
    ip[1].type = INPUT_KEYBOARD;
    ip[1].ki.wVk = vk;
    ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, ip, sizeof(INPUT));
}

void PerformAutoType(const std::string& text) {
    if (text.empty()) {
        ShowErrorPopup("Auto-Type: Nothing to Type",
            "No text is available for injection.\n\n"
            "Use Ctrl+Shift+Z to capture a screenshot and get an AI answer first,\n"
            "then press Ctrl+Shift+T to type the response.");
        return;
    }

    HWND fg = GetForegroundWindow();
    if (!fg) {
        ShowErrorPopup("Auto-Type: No Target Window",
            "Could not find a foreground window to type into.\n\n"
            "Make sure the exam window or text field is focused\n"
            "before pressing Ctrl+Shift+T.");
        return;
    }

    int baseDelay = 70, delayVar = 120;
    switch (g_AutoTyperConfig.speed) {
        case TYPING_SLOW: baseDelay = 120; delayVar = 200; break;
        case TYPING_MEDIUM: baseDelay = 70; delayVar = 120; break;
        case TYPING_FAST: baseDelay = 30; delayVar = 70; break;
    }

    int typoBase = 50, typoWordBoundary = 80;
    int microPauseChance = 20, longPauseChance = 200;
    int punctPauseMul = 1;
    switch (g_AutoTyperConfig.humanization) {
        case HUMAN_LOW:   typoBase = 200; typoWordBoundary = 400; microPauseChance = 80; longPauseChance = 500; punctPauseMul = 0; break;
        case HUMAN_MEDIUM: typoBase = 50;  typoWordBoundary = 80;  microPauseChance = 20; longPauseChance = 200; punctPauseMul = 1; break;
        case HUMAN_HIGH:  typoBase = 25;  typoWordBoundary = 40;  microPauseChance = 10; longPauseChance = 80;  punctPauseMul = 2; break;
    }

    std::wstring wText = Utf8ToUtf16(text);
    Sleep(50);

    bool injectionFailed = false;
    for (size_t i = 0; i < wText.size(); i++) {
        wchar_t c = wText[i];

        bool atWordBoundary = (i == 0 || wText[i-1] == L' ' || wText[i-1] == L'\n');
        int typoChance = atWordBoundary ? typoWordBoundary : typoBase;

        if (CryptoRandUniform(typoChance) == 0 && c != L'\n' && c != L'\r' && c != L' ') {
            wchar_t typo = L'a' + (wchar_t)CryptoRandUniform(26);
            SendUnicodeChar(typo);
            Sleep(HumanDelay(80, 120));
            SendFullVKey(VK_BACK);
            Sleep(HumanDelay(120, 150));
        }

        SendUnicodeChar(c);

        int delay = HumanDelay(baseDelay, delayVar);
        int jitter = CryptoRandUniform(30);
        if (punctPauseMul > 0) {
            if (c == L'.' || c == L'?' || c == L'!' || c == L':') 
                delay += HumanDelay((180 + jitter) * punctPauseMul, 350 * punctPauseMul);
            else if (c == L'\n')
                delay += HumanDelay((200 + jitter) * punctPauseMul, 400 * punctPauseMul);
            else if (c == L' ' || c == L',')
                delay += HumanDelay((30 + jitter) * punctPauseMul, 80 * punctPauseMul);
            else if (c == L':' || c == L';')
                delay += HumanDelay((60 + jitter) * punctPauseMul, 120 * punctPauseMul);
        }
        if (CryptoRandUniform(microPauseChance) == 0) delay += HumanDelay(100, 200);
        if (CryptoRandUniform(longPauseChance) == 0) delay += HumanDelay(300, 600);

        Sleep(delay);
    }

    if (injectionFailed) {
        ShowErrorPopup("Auto-Type: Injection Blocked",
            "SendInput failed — the target window rejected keyboard input.\n\n"
            "Fix:\n"
            "1. Run ZeroPoint as Administrator\n"
            "2. Make sure the exam window is focused");
    }
}

// ============================================================================
// Exam Mode — One-click max stealth activation
// ============================================================================
void ActivateExamMode() {
    g_ExamModeConfig.active = true;
    g_SessionRecordingBlocker = true;
    ApplyDisplayAffinity(g_VEFrame);           // WDA_EXCLUDEFROMCAPTURE
    ApplyDisplayAffinity(g_VELockOverlay);
    if (g_VEChatSidebar) ApplyDisplayAffinity(g_VEChatSidebar);

    // Refresh spoofing on every Exam Mode activation
    ApplyHardwareSpoofing();

    // Prime Rapid Fire for instant use
    g_RapidFireConfig.enabled = true;

    ShowAIPopup("EXAM MODE ACTIVATED\n\n"
                "• Full recording blocker enabled\n"
                "• Hardware fingerprints refreshed\n"
                "• All overlays hidden from proctors\n"
                "• Ctrl+Shift+R = Rapid Fire Thoughts", 4000);
}

void DeactivateExamMode() {
    g_ExamModeConfig.active = false;
    g_SessionRecordingBlocker = false;
    RefreshRecordingBlocker();
    ShowAIPopup("Exam Mode OFF — normal operation restored", 2500);
}

bool IsExamModeActive() { return g_ExamModeConfig.active; }

// ============================================================================
//  Session Recording Blocker — applies WDA_EXCLUDEFROMCAPTURE to a window
// ============================================================================

void ApplyRecordingBlocker(HWND hwnd) {
    if (!g_SessionRecordingBlocker || !hwnd) return;
    ApplyDisplayAffinity(hwnd);
}

void RefreshRecordingBlocker() {
    if (g_SessionRecordingBlocker) {
        if (g_VEFrame && IsWindow(g_VEFrame))
            ApplyDisplayAffinity(g_VEFrame);
        if (g_VELockOverlay && IsWindow(g_VELockOverlay))
            ApplyDisplayAffinity(g_VELockOverlay);
        if (g_VEChatSidebar && IsWindow(g_VEChatSidebar))
            ApplyDisplayAffinity(g_VEChatSidebar);
    }
}

// ============================================================================
//  Remote Access — Secondary RDP listener for remote VE control
// ============================================================================
//
//  When enabled, configures the Windows RDP service to accept connections
//  on a secondary port (default 3390). The remote user connects via
//  mstsc.exe /v:<host>:3390 and authenticates with the configured password.
//  The session targets the same loopback VE session, so the remote user
//  controls the VE while the local user remains on the host desktop.
//
//  Implementation:
//    1. Create a local Windows user "ZP_Remote" with the configured password
//    2. Add RDP listener on the configured port via registry
//    3. Open Windows Firewall rule for the port
//    4. Restart TermService to pick up the new listener
//
//  All stealth layers (WDA_EXCLUDEFROMCAPTURE, layered windows) remain active.
// ============================================================================

static RemoteAccessConfig g_RemoteConfig = { false, 3390, "000000", false, 0 };
static bool g_RemoteAccessActive = false;

// ============================================================================
//  Remote Logging — append-only, 50KB rotate
// ============================================================================

static const char* REMOTE_LOG_PATH = "C:\\ProgramData\\ZeroPoint\\remote.log";
static std::mutex  g_LogMutex;

void RemoteLog(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_LogMutex);

    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);

    // Rotate if > 50 KB
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(REMOTE_LOG_PATH, GetFileExInfoStandard, &fad)) {
        ULONGLONG size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        if (size > 50 * 1024) {
            std::string bakPath = std::string(REMOTE_LOG_PATH) + ".bak";
            DeleteFileA(bakPath.c_str());
            MoveFileA(REMOTE_LOG_PATH, bakPath.c_str());
        }
    }

    FILE* f = fopen(REMOTE_LOG_PATH, "a");
    if (!f) return;

    // Timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

// ============================================================================
//  Remote Connection Counting — query WTS sessions
// ============================================================================

int GetRemoteConnectionCount() {
    if (!g_RemoteAccessActive) return 0;

    WTS_SESSION_INFOA* pSessions = NULL;
    DWORD count = 0;
    int connected = 0;

    if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &count)) {
        for (DWORD i = 0; i < count; i++) {
            // Count active/connected sessions that aren't the console (session 0)
            // and aren't our own loopback VE session
            if (pSessions[i].SessionId > 0 &&
                (pSessions[i].State == WTSActive || pSessions[i].State == WTSConnected)) {
                // Check if the session user is ZP_Remote
                LPSTR pUser = NULL;
                DWORD userLen = 0;
                if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE,
                        pSessions[i].SessionId, WTSUserName, &pUser, &userLen)) {
                    if (pUser && _stricmp(pUser, "ZP_Remote") == 0) {
                        connected++;
                    }
                    WTSFreeMemory(pUser);
                }
            }
        }
        WTSFreeMemory(pSessions);
    }
    return connected;
}

// ============================================================================
//  Local IP Detection
// ============================================================================

std::string GetLocalIPAddress() {
    // Winsock is already initialized globally by InitCDPNetworking() at startup.
    // Do NOT call WSAStartup/WSACleanup here — that would decrement the global
    // reference count and could tear down sockets used by other subsystems.

    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0) return "?.?.?.?";

    struct addrinfo hints = {}, *result = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string ip = "?.?.?.?";
    if (getaddrinfo(hostname, NULL, &hints, &result) == 0) {
        for (auto* p = result; p; p = p->ai_next) {
            struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            std::string s = buf;
            if (s != "127.0.0.1" && s.rfind("127.", 0) != 0) {
                ip = s;
                break;
            }
        }
        freeaddrinfo(result);
    }
    return ip;
}

// ============================================================================
//  Clipboard Helper
// ============================================================================

void CopyToClipboard(HWND owner, const std::string& text) {
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
}

// ============================================================================
//  Inactivity Timer
// ============================================================================

static HANDLE g_InactivityTimerHandle = NULL;
static int    g_InactivityMinutes = 0;
static DWORD  g_LastRemoteActivity = 0;

// Flag set by the inactivity timer thread to request main-thread teardown.
// Avoids deadlock: DisableRemoteAccess() calls StopRemoteInactivityTimer()
// which WaitForSingleObject's the timer thread — calling Disable from the
// timer thread itself would deadlock.
// Not static — accessed from main.cpp's message loop via extern
std::atomic<bool> g_InactivityTimeoutTriggered{false};

static DWORD WINAPI InactivityTimerThread(LPVOID) {
    while (g_InactivityTimerHandle) {
        Sleep(30000); // Check every 30 seconds
        if (!g_RemoteAccessActive || g_InactivityMinutes <= 0) continue;

        DWORD now = GetTickCount();
        DWORD elapsed = now - g_LastRemoteActivity;
        DWORD timeoutMs = (DWORD)g_InactivityMinutes * 60 * 1000;

        // Check if there are active connections — if so, reset timer
        if (GetRemoteConnectionCount() > 0) {
            g_LastRemoteActivity = now;
            continue;
        }

        if (elapsed >= timeoutMs) {
            RemoteLog("Inactivity timeout reached (%d min), signaling auto-disable", g_InactivityMinutes);
            // Signal the main thread instead of calling DisableRemoteAccess()
            // directly — calling it here would deadlock on WaitForSingleObject.
            g_InactivityTimeoutTriggered = true;
            break;
        }
    }
    return 0;
}

void StartRemoteInactivityTimer(int minutes) {
    StopRemoteInactivityTimer();
    if (minutes <= 0) return;
    g_InactivityMinutes = minutes;
    g_LastRemoteActivity = GetTickCount();
    g_InactivityTimerHandle = CreateThread(NULL, 0, InactivityTimerThread, NULL, 0, NULL);
}

void StopRemoteInactivityTimer() {
    if (g_InactivityTimerHandle) {
        HANDLE h = g_InactivityTimerHandle;
        g_InactivityTimerHandle = NULL; // Signal thread to exit
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
    }
    g_InactivityMinutes = 0;
}

void ResetRemoteInactivityTimer() {
    g_LastRemoteActivity = GetTickCount();
}

// ============================================================================
//  Voice-to-Text — Windows SAPI speech recognition
// ============================================================================

static ISpRecognizer* g_pRecognizer = NULL;
static ISpRecoContext* g_pRecoContext = NULL;
static ISpRecoGrammar* g_pGrammar = NULL;
static HANDLE g_VoiceThread = NULL;
static std::atomic<bool> g_VoiceActive{false};

static DWORD WINAPI VoiceRecognitionThread(LPVOID) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL,
                                  IID_ISpRecognizer, (void**)&g_pRecognizer);
    if (FAILED(hr)) { CoUninitialize(); g_VoiceActive = false; return 1; }

    // Use default audio input
    ISpObjectToken* pAudioToken = NULL;
    hr = SpGetDefaultTokenFromCategoryId(SPCAT_AUDIOIN, &pAudioToken);
    if (SUCCEEDED(hr)) {
        g_pRecognizer->SetInput(pAudioToken, TRUE);
        pAudioToken->Release();
    }

    hr = g_pRecognizer->CreateRecoContext(&g_pRecoContext);
    if (FAILED(hr)) {
        g_pRecognizer->Release(); g_pRecognizer = NULL;
        CoUninitialize(); g_VoiceActive = false; return 1;
    }

    g_pRecoContext->SetNotifyWin32Event();
    g_pRecoContext->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));

    hr = g_pRecoContext->CreateGrammar(0, &g_pGrammar);
    if (FAILED(hr)) {
        g_pRecoContext->Release(); g_pRecoContext = NULL;
        g_pRecognizer->Release(); g_pRecognizer = NULL;
        CoUninitialize(); g_VoiceActive = false; return 1;
    }

    g_pGrammar->LoadDictation(NULL, SPLO_STATIC);
    g_pGrammar->SetDictationState(SPRS_ACTIVE);

    HANDLE hEvent = g_pRecoContext->GetNotifyEventHandle();

    RemoteLog("Voice-to-Text started");

    while (g_VoiceActive) {
        DWORD wait = WaitForSingleObject(hEvent, 500);
        if (!g_VoiceActive) break;
        if (wait != WAIT_OBJECT_0) continue;

        SPEVENT event;
        ULONG fetched;
        while (g_pRecoContext->GetEvents(1, &event, &fetched) == S_OK && fetched > 0) {
            if (event.eEventId == SPEI_RECOGNITION && event.punkVal) {
                ISpRecoResult* pResult = (ISpRecoResult*)event.punkVal;
                LPWSTR pText = NULL;
                if (SUCCEEDED(pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE,
                                                TRUE, &pText, NULL)) && pText) {
                    // Convert wide string to UTF-8
                    int len = WideCharToMultiByte(CP_UTF8, 0, pText, -1, NULL, 0, NULL, NULL);
                    std::string text(len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, pText, -1, &text[0], len, NULL, NULL);
                    CoTaskMemFree(pText);

                    if (!text.empty()) {
                        RemoteLog("Voice recognized: %s", text.c_str());
                        PerformAutoType(text);
                    }
                }
            }
        }
    }

    g_pGrammar->SetDictationState(SPRS_INACTIVE);
    g_pGrammar->Release(); g_pGrammar = NULL;
    g_pRecoContext->Release(); g_pRecoContext = NULL;
    g_pRecognizer->Release(); g_pRecognizer = NULL;

    RemoteLog("Voice-to-Text stopped");
    CoUninitialize();
    return 0;
}

void StartVoiceToText() {
    if (g_VoiceActive) return;
    g_VoiceActive = true;
    g_VoiceThread = CreateThread(NULL, 0, VoiceRecognitionThread, NULL, 0, NULL);
}

void StopVoiceToText() {
    if (!g_VoiceActive) return;
    g_VoiceActive = false;
    if (g_VoiceThread) {
        WaitForSingleObject(g_VoiceThread, 3000);
        CloseHandle(g_VoiceThread);
        g_VoiceThread = NULL;
    }
}

bool IsVoiceToTextActive() { return g_VoiceActive; }

static const char* REMOTE_USER = "ZP_Remote";
static const char* REMOTE_SENTINEL = "C:\\ProgramData\\ZeroPoint\\remote_active.lock";

// Registry path for secondary RDP listener
static const char* RDP_LISTENER_PATH =
    "SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\WinStations\\ZeroPoint-RDP";

// Write/remove a sentinel file so we can detect unclean shutdowns.
// On startup, if the sentinel exists, the previous session crashed
// with remote access active — we must clean up the orphaned user.
static void WriteSentinel() {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    HANDLE h = CreateFileA(REMOTE_SENTINEL, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static void RemoveSentinel() {
    DeleteFileA(REMOTE_SENTINEL);
}

static bool SentinelExists() {
    DWORD attr = GetFileAttributesA(REMOTE_SENTINEL);
    return (attr != INVALID_FILE_ATTRIBUTES);
}

// Run a command hidden (no cmd window flash). Fire-and-forget with timeout.
static void RunHiddenCmd(const char* cmd, DWORD timeoutMs = 10000) {
    std::string full = std::string("cmd.exe /C ") + cmd;
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(NULL, const_cast<char*>(full.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, timeoutMs);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// Called once at startup to clean up after a crash
static void CleanupOrphanedRemoteUser() {
    if (!SentinelExists()) return;
    // Previous session crashed with remote active — remove leftovers
    RemoteLog("Cleaning up orphaned remote user from previous crash");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "net user %s /delete >nul 2>&1", REMOTE_USER);
    RunHiddenCmd(cmd);
    RunHiddenCmd("netsh advfirewall firewall delete rule name=\"ZeroPoint Remote Access\" >nul 2>&1");
    RegDeleteKeyA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\WinStations\\ZeroPoint-RDP");
    RemoveSentinel();
}

// atexit handler — last-resort cleanup if process exits without DisableRemoteAccess.
// Removes user, firewall rule, registry listener, and sentinel in one shot.
static void AtExitRemoteCleanup() {
    if (g_RemoteAccessActive) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "net user %s /delete >nul 2>&1", REMOTE_USER);
        RunHiddenCmd(cmd, 5000);
        RunHiddenCmd("netsh advfirewall firewall delete rule name=\"ZeroPoint Remote Access\" >nul 2>&1", 5000);
        RegDeleteKeyA(HKEY_LOCAL_MACHINE, RDP_LISTENER_PATH);
        RemoveSentinel();
    }
}

static bool g_AtExitRegistered = false;

static bool CreateRemoteUser(const char* password) {
    // Create local user for remote RDP authentication
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "net user %s %s /add /expires:never /passwordchg:no >nul 2>&1",
        REMOTE_USER, password);
    RunHiddenCmd(cmd);

    // Add to Remote Desktop Users group
    snprintf(cmd, sizeof(cmd),
        "net localgroup \"Remote Desktop Users\" %s /add >nul 2>&1", REMOTE_USER);
    RunHiddenCmd(cmd);

    // Verify user was created
    char verifyCmd[256];
    snprintf(verifyCmd, sizeof(verifyCmd), "net user %s >nul 2>&1", REMOTE_USER);
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::string full = std::string("cmd.exe /C ") + verifyCmd;
    bool created = false;
    if (CreateProcessA(NULL, const_cast<char*>(full.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        created = (exitCode == 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return created;
}

static void RemoveRemoteUser() {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "net user %s /delete >nul 2>&1", REMOTE_USER);
    RunHiddenCmd(cmd);
}

static bool ConfigureRDPListener(int port) {
    HKEY hKey;
    LONG rc = RegCreateKeyExA(HKEY_LOCAL_MACHINE, RDP_LISTENER_PATH,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL);
    if (rc != ERROR_SUCCESS) return false;

    // Port number
    DWORD portVal = (DWORD)port;
    RegSetValueExA(hKey, "PortNumber", 0, REG_DWORD, (BYTE*)&portVal, sizeof(DWORD));

    // Enable encryption
    DWORD minEnc = 1;
    RegSetValueExA(hKey, "MinEncryptionLevel", 0, REG_DWORD, (BYTE*)&minEnc, sizeof(DWORD));

    // Security layer (TLS)
    DWORD secLayer = 2;
    RegSetValueExA(hKey, "SecurityLayer", 0, REG_DWORD, (BYTE*)&secLayer, sizeof(DWORD));

    // User authentication required
    DWORD userAuth = 1;
    RegSetValueExA(hKey, "UserAuthentication", 0, REG_DWORD, (BYTE*)&userAuth, sizeof(DWORD));

    RegCloseKey(hKey);
    return true;
}

static void RemoveRDPListener() {
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, RDP_LISTENER_PATH);
}

static bool OpenFirewallPort(int port) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "netsh advfirewall firewall add rule name=\"ZeroPoint Remote Access\" "
        "dir=in action=allow protocol=tcp localport=%d >nul 2>&1", port);
    RunHiddenCmd(cmd);
    return true;  // Best-effort; firewall may be disabled
}

static void CloseFirewallPort() {
    RunHiddenCmd("netsh advfirewall firewall delete rule name=\"ZeroPoint Remote Access\" >nul 2>&1");
}

// Check if a Windows service is running by querying the Service Control Manager
static bool IsServiceRunning(const char* serviceName) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE svc = OpenServiceA(scm, serviceName, SERVICE_QUERY_STATUS);
    if (!svc) { CloseServiceHandle(scm); return false; }

    SERVICE_STATUS status = {};
    BOOL ok = QueryServiceStatus(svc, &status);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return ok && (status.dwCurrentState == SERVICE_RUNNING);
}

// Check if current process is running elevated (Administrator)
static bool IsRunningElevated() {
    BOOL elevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elev = {};
        DWORD cbSize = sizeof(elev);
        if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &cbSize)) {
            elevated = elev.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return elevated != FALSE;
}

bool EnableRemoteAccess(const RemoteAccessConfig& cfg) {
    if (g_RemoteAccessActive) return true;
    if (GetVEState() != VE_RUNNING && GetVEState() != VE_LOCKED) {
        ShowErrorPopup(
            "Remote Access: VE Not Running",
            "The Virtual Environment must be running before\n"
            "you can enable Remote Access.\n\n"
            "Start the VE first, then try again.");
        return false;
    }

    // Check admin elevation first — all subsequent steps need it
    if (!IsRunningElevated()) {
        ShowErrorPopup(
            "Remote Access: Administrator Required",
            "Remote Access needs elevated privileges to:\n"
            "  - Create the temporary user account\n"
            "  - Configure the RDP listener\n"
            "  - Open the firewall port\n\n"
            "Please restart ZeroPoint with \"Run as Administrator\"\n"
            "and try again.");
        return false;
    }

    // Verify TermService is running — needed to accept RDP connections
    if (!IsServiceRunning("TermService")) {
        ShowErrorPopup(
            "Remote Access: Remote Desktop Services Stopped",
            "The Remote Desktop Services (TermService) must be running\n"
            "for Remote Access to accept incoming connections.\n\n"
            "To fix:\n"
            "  1. Press Win+R, type services.msc, press Enter\n"
            "  2. Find \"Remote Desktop Services\" in the list\n"
            "  3. Right-click → Properties → Startup Type: Manual\n"
            "  4. Click Start, then OK\n"
            "  5. Return here and try again");
        return false;
    }

    g_RemoteConfig = cfg;
    g_RemoteConfig.enabled = true;

    // Step 1: Create remote user with password
    if (!CreateRemoteUser(cfg.password)) {
        ShowErrorPopup(
            "Remote Access: User Creation Failed",
            "Could not create the temporary 'ZP_Remote' account.\n\n"
            "Possible causes:\n"
            "  - Account name already exists from a previous crash\n"
            "  - Local security policy restricts user creation\n"
            "  - Password does not meet complexity requirements\n\n"
            "Try a longer password (6+ characters) or reboot first.");
        return false;
    }

    // Step 2: Configure RDP listener on custom port
    if (!ConfigureRDPListener(cfg.port)) {
        RemoveRemoteUser();
        ShowErrorPopup(
            "Remote Access: Listener Configuration Failed",
            "Could not write the RDP listener to the registry.\n\n"
            "This usually means:\n"
            "  - Group Policy is blocking Terminal Server changes\n"
            "  - Another application owns the WinStations key\n\n"
            "Check Event Viewer → System for related errors.");
        return false;
    }

    // Step 3: Open firewall
    OpenFirewallPort(cfg.port);

    // Step 4: Restart TermService to apply new listener.
    // NOTE: This briefly disrupts the active VE loopback RDP session.
    // The .rdp file has autoreconnection=1 so mstsc will auto-reconnect.
    RemoteLog("Restarting TermService (VE session will auto-reconnect)...");
    RunHiddenCmd("net stop TermService /y >nul 2>&1 && net start TermService >nul 2>&1", 20000);
    Sleep(2000);  // Extra time for mstsc auto-reconnect

    g_RemoteAccessActive = true;

    // Write sentinel so crash recovery can clean up the user
    WriteSentinel();

    // Register atexit handler (once) as last-resort cleanup
    if (!g_AtExitRegistered) {
        atexit(AtExitRemoteCleanup);
        g_AtExitRegistered = true;
    }

    RemoteLog("Remote Access ENABLED — port %d", cfg.port);

    // Start inactivity timer if configured
    if (g_RemoteConfig.inactivityTimeout > 0) {
        StartRemoteInactivityTimer(g_RemoteConfig.inactivityTimeout);
    }

    return true;
}

void DisableRemoteAccess() {
    if (!g_RemoteAccessActive) return;

    StopRemoteInactivityTimer();
    StopVoiceToText();

    RemoveRDPListener();
    CloseFirewallPort();
    RemoveRemoteUser();
    RemoveSentinel();

    // Restart TermService to drop remote connections.
    // The VE loopback session will auto-reconnect (autoreconnection=1 in .rdp).
    RunHiddenCmd("net stop TermService /y >nul 2>&1 && net start TermService >nul 2>&1", 20000);
    Sleep(1500);  // Allow mstsc auto-reconnect time

    g_RemoteAccessActive = false;
    g_RemoteConfig.enabled = false;

    RemoteLog("Remote Access DISABLED");
}

void ToggleRemoteAccess() {
    if (g_RemoteAccessActive)
        DisableRemoteAccess();
    else
        EnableRemoteAccess(g_RemoteConfig);
}

bool IsRemoteAccessEnabled() { return g_RemoteAccessActive; }

RemoteAccessConfig GetRemoteAccessConfig() { return g_RemoteConfig; }

void SetRemoteAccessConfig(const RemoteAccessConfig& cfg) {
    bool wasActive = g_RemoteAccessActive;
    if (wasActive) DisableRemoteAccess();
    g_RemoteConfig = cfg;
    // Persist to config.ini
    std::vector<std::string> lines;
    bool foundPort = false, foundPass = false, foundRemote = false;
    {
        std::ifstream f("C:\\ProgramData\\ZeroPoint\\config.ini");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("remote_port=", 0) == 0) {
                lines.push_back("remote_port=" + std::to_string(cfg.port));
                foundPort = true;
            } else if (line.rfind("remote_pass=", 0) == 0) {
                lines.push_back(std::string("remote_pass=") + cfg.password);
                foundPass = true;
            } else if (line.rfind("remote_enabled=", 0) == 0) {
                lines.push_back(std::string("remote_enabled=") + (cfg.enabled ? "1" : "0"));
                foundRemote = true;
            } else {
                lines.push_back(line);
            }
        }
    }
    if (!foundPort)   lines.push_back("remote_port=" + std::to_string(cfg.port));
    if (!foundPass)   lines.push_back(std::string("remote_pass=") + cfg.password);
    if (!foundRemote) lines.push_back(std::string("remote_enabled=") + (cfg.enabled ? "1" : "0"));

    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    std::ofstream out("C:\\ProgramData\\ZeroPoint\\config.ini");
    for (auto& l : lines) out << l << "\n";

    if (wasActive && cfg.enabled) EnableRemoteAccess(cfg);
}

// ============================================================================
//  Remote Access Panel — Frosted glass, fully modeless (non-blocking)
// ============================================================================
//  Messages dispatched by WinMain's PeekMessage loop — the launcher and all
//  other windows remain responsive while this panel is open.

static HWND g_RemotePanelHwnd = NULL;
static bool g_RemoteToggleState = false;
static char g_RemotePassBuf[64] = "000000";
static char g_RemotePortBuf[8]  = "3390";
static int  g_RemotePanelHover  = 0;

#define ID_REMOTE_TOGGLE  8001
#define ID_REMOTE_START   8002
#define ID_REMOTE_CLOSE   8003
#define ID_REMOTE_PASS    8004
#define ID_REMOTE_PORT    8005
#define ID_REMOTE_COPY    8006
#define ID_REMOTE_MIC     8007
#define ID_REMOTE_TIMEOUT 8008

// "Copied!" toast state
static DWORD g_CopiedToastTime = 0;
static bool  g_ShowCopiedToast = false;

static HWND g_RemotePassEdit = NULL;
static HWND g_RemotePortEdit = NULL;
static HWND g_RemoteTimeoutEdit = NULL;
static char g_RemoteTimeoutBuf[8] = "0";

// Soft shadow helper — matches launcher's DrawSoftShadow
static void VE_DrawSoftShadow(HDC hdc, const RECT& rc) {
    RECT shadow = { rc.left + 3, rc.top + 3, rc.right + 3, rc.bottom + 3 };
    VE_FillFrosted(hdc, shadow, RGB(0xC0, 0xCC, 0xDD), 60);
}

// Inner glow helper — matches launcher's DrawInnerGlow
static void VE_DrawInnerGlow(HDC hdc, const RECT& rc) {
    RECT glow = { rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2 };
    VE_FillFrosted(hdc, glow, RGB(0xD8, 0xEE, 0xFF), 40);
}

static LRESULT CALLBACK RemotePanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // DWM blur behind for true frosted glass
        DWM_BLURBEHIND bb = {};
        bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable  = TRUE;
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(bb.hRgnBlur);

        // Font for edit controls — stored statically so we can clean up on re-open
        static HFONT s_EditFont = NULL;
        if (s_EditFont) DeleteObject(s_EditFont);
        s_EditFont = VE_CreateFont(14, FW_NORMAL);
        HFONT editFont = s_EditFont;

        // Password field
        g_RemotePassEdit = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", g_RemotePassBuf,
            WS_CHILD | WS_VISIBLE | ES_CENTER | ES_PASSWORD | ES_AUTOHSCROLL,
            100, 108, 190, 26,
            hwnd, (HMENU)ID_REMOTE_PASS, GetModuleHandle(NULL), NULL);
        SendMessage(g_RemotePassEdit, WM_SETFONT, (WPARAM)editFont, TRUE);
        SendMessage(g_RemotePassEdit, EM_SETLIMITTEXT, 63, 0);

        // Port field
        g_RemotePortEdit = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", g_RemotePortBuf,
            WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER | ES_AUTOHSCROLL,
            100, 148, 80, 26,
            hwnd, (HMENU)ID_REMOTE_PORT, GetModuleHandle(NULL), NULL);
        SendMessage(g_RemotePortEdit, WM_SETFONT, (WPARAM)editFont, TRUE);
        SendMessage(g_RemotePortEdit, EM_SETLIMITTEXT, 5, 0);

        // Inactivity timeout field (minutes)
        snprintf(g_RemoteTimeoutBuf, sizeof(g_RemoteTimeoutBuf), "%d", g_RemoteConfig.inactivityTimeout);
        g_RemoteTimeoutEdit = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", g_RemoteTimeoutBuf,
            WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER | ES_AUTOHSCROLL,
            100, 188, 60, 26,
            hwnd, (HMENU)ID_REMOTE_TIMEOUT, GetModuleHandle(NULL), NULL);
        SendMessage(g_RemoteTimeoutEdit, WM_SETFONT, (WPARAM)editFont, TRUE);
        SendMessage(g_RemoteTimeoutEdit, EM_SETLIMITTEXT, 3, 0);

        g_RemoteToggleState = IsRemoteAccessEnabled();

        // Refresh timer for connection count and toast dismissal
        SetTimer(hwnd, 1, 2000, NULL);
        return 0;
    }

    case WM_ERASEBKGND: return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Base fill
        HBRUSH bgBrush = CreateSolidBrush(VE_ICY_BG);
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Soft shadow + frosted panel + inner glow
        RECT panelRc = { 4, 4, w - 4, h - 4 };
        VE_DrawSoftShadow(memDC, panelRc);
        VE_FillFrosted(memDC, panelRc, VE_ICY_PANEL, 220);
        VE_DrawInnerGlow(memDC, panelRc);

        // Accent border with rounded corners
        HPEN borderPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        HGDIOBJ oldPen = SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, panelRc.left, panelRc.top,
                  panelRc.right, panelRc.bottom, 18, 18);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);

        SetBkMode(memDC, TRANSPARENT);

        // Title
        HFONT titleFont = VE_CreateFont(20, FW_SEMIBOLD);
        HGDIOBJ oldFont = SelectObject(memDC, titleFont);
        SetTextColor(memDC, VE_ICY_ACCENT);
        RECT titleRc = { 0, 14, w, 42 };
        DrawTextA(memDC, "Remote Access", -1, &titleRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(titleFont);

        // Accent rule under title
        HPEN accPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        HGDIOBJ oldAcc = SelectObject(memDC, accPen);
        MoveToEx(memDC, 30, 48, NULL);
        LineTo(memDC, w - 30, 48);
        SelectObject(memDC, oldAcc);
        DeleteObject(accPen);

        HFONT labelFont = VE_CreateFont(13, FW_NORMAL);
        SelectObject(memDC, labelFont);

        // Toggle label
        SetTextColor(memDC, VE_ICY_TEXT);
        RECT toggleLabel = { 22, 60, 190, 80 };
        DrawTextA(memDC, "Enable Remote Access", -1, &toggleLabel, DT_LEFT | DT_SINGLELINE);

        // Toggle switch (pill)
        int togX = w - 74, togY = 60;
        RECT togRc = { togX, togY, togX + 50, togY + 24 };
        bool togOn = g_RemoteToggleState;
        COLORREF togBg = togOn ? VE_ICY_ACCENT : RGB(0xC0, 0xCC, 0xDD);
        HBRUSH togBr = CreateSolidBrush(togBg);
        HPEN togPen = CreatePen(PS_SOLID, 1, togBg);
        SelectObject(memDC, togBr);
        SelectObject(memDC, togPen);
        RoundRect(memDC, togRc.left, togRc.top, togRc.right, togRc.bottom, 12, 12);
        DeleteObject(togBr);
        DeleteObject(togPen);

        // Knob
        int knobX = togOn ? togRc.right - 22 : togRc.left + 2;
        HBRUSH knobBr = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
        SelectObject(memDC, knobBr);
        SelectObject(memDC, GetStockObject(NULL_PEN));
        Ellipse(memDC, knobX, togY + 2, knobX + 20, togY + 22);
        DeleteObject(knobBr);

        // Status text + connected count
        HFONT statusFont = VE_CreateFont(11, FW_SEMIBOLD);
        SelectObject(memDC, statusFont);
        bool liveActive = g_RemoteAccessActive;
        int connCount = liveActive ? GetRemoteConnectionCount() : 0;
        char statusBuf[128];
        if (liveActive) {
            SetTextColor(memDC, RGB(0x00, 0xAA, 0x55));
            if (connCount > 0)
                snprintf(statusBuf, sizeof(statusBuf), "ACTIVE  —  Connected: %d", connCount);
            else
                snprintf(statusBuf, sizeof(statusBuf), "ACTIVE  —  Listening (0 connected)");
        } else if (togOn) {
            SetTextColor(memDC, RGB(0xCC, 0x99, 0x00));
            snprintf(statusBuf, sizeof(statusBuf), "Ready  —  Press Start to begin");
        } else {
            SetTextColor(memDC, VE_ICY_DIM);
            snprintf(statusBuf, sizeof(statusBuf), "Disabled");
        }
        RECT statusRc = { 22, 86, w - 22, 104 };
        DrawTextA(memDC, statusBuf, -1, &statusRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(statusFont);

        // Password label
        SetTextColor(memDC, VE_ICY_TEXT);
        SelectObject(memDC, labelFont);
        RECT passLabel = { 22, 112, 98, 130 };
        DrawTextA(memDC, "Password:", -1, &passLabel, DT_LEFT | DT_SINGLELINE);

        // Port label
        RECT portLabel = { 22, 152, 98, 170 };
        DrawTextA(memDC, "Port:", -1, &portLabel, DT_LEFT | DT_SINGLELINE);

        // Timeout label
        RECT timeoutLabel = { 22, 192, 98, 210 };
        DrawTextA(memDC, "Timeout:", -1, &timeoutLabel, DT_LEFT | DT_SINGLELINE);
        HFONT tinyFont = VE_CreateFont(9, FW_NORMAL);
        SelectObject(memDC, tinyFont);
        SetTextColor(memDC, VE_ICY_DIM);
        RECT timeoutHint = { 166, 194, w - 22, 210 };
        DrawTextA(memDC, "min (0=off)", -1, &timeoutHint, DT_LEFT | DT_SINGLELINE);
        DeleteObject(tinyFont);

        // ---- "Copy mstsc Command" button ----
        RECT copyBtn = { 22, 222, 200, 246 };
        {
            RECT cpShadow = { copyBtn.left + 2, copyBtn.top + 2,
                              copyBtn.right + 2, copyBtn.bottom + 2 };
            VE_FillFrosted(memDC, cpShadow, RGB(0xC0, 0xCC, 0xDD), 30);
        }
        COLORREF cpBg = (g_RemotePanelHover == ID_REMOTE_COPY)
            ? RGB(0xE0, 0xEA, 0xF8) : RGB(0xF5, 0xF8, 0xFF);
        HBRUSH cpBr = CreateSolidBrush(cpBg);
        HPEN cpPen = CreatePen(PS_SOLID, 1, VE_ICY_ACCENT);
        SelectObject(memDC, cpBr);
        SelectObject(memDC, cpPen);
        RoundRect(memDC, copyBtn.left, copyBtn.top, copyBtn.right, copyBtn.bottom, 8, 8);
        DeleteObject(cpBr);
        DeleteObject(cpPen);

        HFONT cpFont = VE_CreateFont(11, FW_SEMIBOLD);
        SelectObject(memDC, cpFont);
        SetTextColor(memDC, VE_ICY_ACCENT);
        DrawTextA(memDC, "Copy mstsc Command", -1, &copyBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(cpFont);

        // "Copied!" toast
        if (g_ShowCopiedToast && (GetTickCount() - g_CopiedToastTime < 2000)) {
            HFONT toastFont = VE_CreateFont(11, FW_BOLD);
            SelectObject(memDC, toastFont);
            SetTextColor(memDC, RGB(0x00, 0xAA, 0x55));
            RECT toastRc = { 206, 226, w - 22, 246 };
            DrawTextA(memDC, "Copied!", -1, &toastRc, DT_LEFT | DT_SINGLELINE);
            DeleteObject(toastFont);
        } else {
            g_ShowCopiedToast = false;
        }

        // ---- Mic (Voice-to-Text) button ----
        RECT micBtn = { 208, 222, 290, 246 };
        {
            bool micActive = IsVoiceToTextActive();
            COLORREF micBg = (g_RemotePanelHover == ID_REMOTE_MIC)
                ? RGB(0xE0, 0xEA, 0xF8) : (micActive ? RGB(0xDD, 0xFF, 0xEE) : RGB(0xF5, 0xF8, 0xFF));
            COLORREF micBrd = micActive ? RGB(0x00, 0xAA, 0x55) : VE_ICY_ACCENT;
            HBRUSH micBr = CreateSolidBrush(micBg);
            HPEN micPen = CreatePen(PS_SOLID, 1, micBrd);
            SelectObject(memDC, micBr);
            SelectObject(memDC, micPen);
            RoundRect(memDC, micBtn.left, micBtn.top, micBtn.right, micBtn.bottom, 8, 8);
            DeleteObject(micBr);
            DeleteObject(micPen);

            // Microphone icon: circle head + line body
            int mcx = (micBtn.left + micBtn.right) / 2 - 18;
            int mcy = (micBtn.top + micBtn.bottom) / 2;
            HPEN micIconPen = CreatePen(PS_SOLID, 2, micBrd);
            SelectObject(memDC, micIconPen);
            SelectObject(memDC, GetStockObject(NULL_BRUSH));
            // Mic head (small ellipse)
            Ellipse(memDC, mcx - 3, mcy - 7, mcx + 4, mcy + 1);
            // Mic body (line down)
            MoveToEx(memDC, mcx, mcy + 1, NULL);
            LineTo(memDC, mcx, mcy + 5);
            // Base arc
            Arc(memDC, mcx - 6, mcy - 2, mcx + 7, mcy + 7, mcx + 7, mcy + 3, mcx - 6, mcy + 3);
            DeleteObject(micIconPen);

            HFONT micFont = VE_CreateFont(10, FW_SEMIBOLD);
            SelectObject(memDC, micFont);
            SetTextColor(memDC, micBrd);
            RECT micLbl = { mcx + 8, micBtn.top, micBtn.right - 4, micBtn.bottom };
            DrawTextA(memDC, micActive ? "MIC ON" : "MIC", -1, &micLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(micFont);
        }

        // ---- Log location hint ----
        HFONT logFont = VE_CreateFont(9, FW_NORMAL);
        SelectObject(memDC, logFont);
        SetTextColor(memDC, RGB(0xC0, 0xCC, 0xDD));
        RECT logRc = { 22, 250, w - 22, 264 };
        DrawTextA(memDC, "Log: C:\\ProgramData\\ZeroPoint\\remote.log", -1, &logRc,
                  DT_LEFT | DT_SINGLELINE);
        DeleteObject(logFont);

        // ---- Start/Stop button with soft shadow ----
        RECT startBtn = { w/2 - 84, h - 74, w/2 + 84, h - 42 };
        {
            RECT btnShadow = { startBtn.left + 2, startBtn.top + 2,
                               startBtn.right + 2, startBtn.bottom + 2 };
            VE_FillFrosted(memDC, btnShadow, RGB(0xC0, 0xCC, 0xDD), 40);
        }
        // Pulsing green border when someone is connected
        COLORREF startBrd = (liveActive && connCount > 0) ? RGB(0x00, 0xCC, 0x66) : VE_ICY_ACCENT;
        COLORREF startBg = (g_RemotePanelHover == ID_REMOTE_START)
            ? RGB(0xE0, 0xEA, 0xF8) : RGB(0xF5, 0xF8, 0xFF);
        HBRUSH btnBr = CreateSolidBrush(startBg);
        HPEN btnPen = CreatePen(PS_SOLID, 2, startBrd);
        SelectObject(memDC, btnBr);
        SelectObject(memDC, btnPen);
        RoundRect(memDC, startBtn.left, startBtn.top,
                  startBtn.right, startBtn.bottom, 12, 12);
        DeleteObject(btnBr);
        DeleteObject(btnPen);

        HFONT btnFont = VE_CreateFont(14, FW_SEMIBOLD);
        SelectObject(memDC, btnFont);
        SetTextColor(memDC, startBrd);
        const char* btnText = liveActive ? "Stop Remote Session" : "Start Remote Session";
        DrawTextA(memDC, btnText, -1, &startBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(btnFont);

        // ---- Close [X] — top-right, with icy-cyan glow + 1.1x scale on hover ----
        {
            bool closeHov = (g_RemotePanelHover == ID_REMOTE_CLOSE);
            RECT closeBtn = closeHov
                ? RECT{ w - 33, 3, w - 7, 27 }   // 1.1x scale-up (wider by ~3px each side)
                : RECT{ w - 30, 6, w - 10, 24 };

            if (closeHov) {
                // Icy-cyan outer glow (alpha-blended)
                RECT glowOuter = { closeBtn.left - 4, closeBtn.top - 4,
                                   closeBtn.right + 4, closeBtn.bottom + 4 };
                VE_FillFrosted(memDC, glowOuter, VE_ICY_ACCENT, 35);
                // Tighter inner glow
                RECT glowInner = { closeBtn.left - 2, closeBtn.top - 2,
                                   closeBtn.right + 2, closeBtn.bottom + 2 };
                VE_FillFrosted(memDC, glowInner, VE_ICY_ACCENT, 55);
            }

            HFONT closeFont = VE_CreateFont(closeHov ? 16 : 14, FW_BOLD);
            SelectObject(memDC, closeFont);
            SetTextColor(memDC, closeHov ? VE_ICY_ACCENT : VE_ICY_DIM);
            DrawTextA(memDC, "X", -1, &closeBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(closeFont);
        }

        // Hotkey hint + branding
        HFONT hintFont = VE_CreateFont(10, FW_NORMAL);
        SelectObject(memDC, hintFont);
        SetTextColor(memDC, RGB(0xC0, 0xCC, 0xDD));
        RECT hintRc = { 0, h - 26, w, h - 8 };
        DrawTextA(memDC, "Ctrl+Alt+R to toggle  |  ZeroPoint Remote", -1, &hintRc,
                  DT_CENTER | DT_SINGLELINE);
        SelectObject(memDC, oldFont);
        DeleteObject(hintFont);
        DeleteObject(labelFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        // Periodic refresh for connection count and toast dismissal
        if (wp == 1) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE: {
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right;
        int prev = g_RemotePanelHover;
        g_RemotePanelHover = 0;

        RECT startBtn = { w/2 - 84, rc.bottom - 74, w/2 + 84, rc.bottom - 42 };
        RECT closeBtn = { w - 33, 3, w - 7, 27 };  // use the larger hit area
        RECT togRc    = { w - 74, 60, w - 24, 84 };
        RECT copyBtn  = { 22, 222, 200, 246 };
        RECT micBtn   = { 208, 222, 290, 246 };
        POINT pt = { mx, my };

        if (PtInRect(&startBtn, pt))      g_RemotePanelHover = ID_REMOTE_START;
        else if (PtInRect(&closeBtn, pt))  g_RemotePanelHover = ID_REMOTE_CLOSE;
        else if (PtInRect(&togRc, pt))     g_RemotePanelHover = ID_REMOTE_TOGGLE;
        else if (PtInRect(&copyBtn, pt))   g_RemotePanelHover = ID_REMOTE_COPY;
        else if (PtInRect(&micBtn, pt))    g_RemotePanelHover = ID_REMOTE_MIC;

        if (prev != g_RemotePanelHover) {
            InvalidateRect(hwnd, NULL, FALSE);
            SetCursor(LoadCursor(NULL, g_RemotePanelHover ? IDC_HAND : IDC_ARROW));
        }

        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_RemotePanelHover) {
            g_RemotePanelHover = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right;
        POINT pt = { mx, my };

        RECT startBtn = { w/2 - 84, rc.bottom - 74, w/2 + 84, rc.bottom - 42 };
        RECT closeBtn = { w - 33, 3, w - 7, 27 };
        RECT togRc    = { w - 74, 60, w - 24, 84 };
        RECT copyBtn  = { 22, 222, 200, 246 };
        RECT micBtn   = { 208, 222, 290, 246 };

        if (PtInRect(&togRc, pt)) {
            g_RemoteToggleState = !g_RemoteToggleState;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (PtInRect(&copyBtn, pt)) {
            // Copy mstsc command to clipboard
            char portBuf[8] = {};
            GetWindowTextA(g_RemotePortEdit, portBuf, sizeof(portBuf));
            std::string ip = GetLocalIPAddress();
            std::string cmd = "mstsc.exe /v:" + ip + ":" + (portBuf[0] ? portBuf : "3390");
            CopyToClipboard(hwnd, cmd);
            g_ShowCopiedToast = true;
            g_CopiedToastTime = GetTickCount();
            RemoteLog("Connection command copied: %s", cmd.c_str());
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRect(&micBtn, pt)) {
            // Toggle Voice-to-Text
            if (IsVoiceToTextActive()) {
                StopVoiceToText();
            } else {
                StartVoiceToText();
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (PtInRect(&startBtn, pt)) {
            // Read current values from edit controls
            GetWindowTextA(g_RemotePassEdit, g_RemotePassBuf, sizeof(g_RemotePassBuf));
            GetWindowTextA(g_RemotePortEdit, g_RemotePortBuf, sizeof(g_RemotePortBuf));
            GetWindowTextA(g_RemoteTimeoutEdit, g_RemoteTimeoutBuf, sizeof(g_RemoteTimeoutBuf));

            if (g_RemoteToggleState && !g_RemoteAccessActive) {
                RemoteAccessConfig cfg;
                cfg.enabled = true;
                cfg.port = atoi(g_RemotePortBuf);
                if (cfg.port < 1 || cfg.port > 65535) cfg.port = 3390;
                strncpy(cfg.password, g_RemotePassBuf, sizeof(cfg.password) - 1);
                cfg.password[sizeof(cfg.password) - 1] = '\0';
                cfg.autoStartWithVE = g_RemoteConfig.autoStartWithVE;
                cfg.inactivityTimeout = atoi(g_RemoteTimeoutBuf);
                // Sync to global config for persistence
                g_RemoteInactivityTimeout = cfg.inactivityTimeout;

                if (strlen(cfg.password) == 0) {
                    ShowErrorPopup("Remote Access",
                        "Please enter a password or 6-digit access code\n"
                        "before starting the remote session.");
                } else {
                    if (EnableRemoteAccess(cfg)) {
                        g_RemoteToggleState = true;
                    } else {
                        g_RemoteToggleState = false;
                    }
                }
            } else if (g_RemoteAccessActive) {
                DisableRemoteAccess();
                g_RemoteToggleState = false;
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (PtInRect(&closeBtn, pt)) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            POINT pt;
            pt.x = (short)LOWORD(lp);
            pt.y = (short)HIWORD(lp);
            ScreenToClient(hwnd, &pt);
            RECT crc;
            GetClientRect(hwnd, &crc);
            if (pt.y < 48 && pt.x < crc.right - 36) return HTCAPTION;
        }
        return hit;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_RemotePanelHwnd    = NULL;
        g_RemotePassEdit     = NULL;
        g_RemotePortEdit     = NULL;
        g_RemoteTimeoutEdit  = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ShowRemoteAccessPanel(HWND owner) {
    // Bring existing panel to front if already open
    if (g_RemotePanelHwnd && IsWindow(g_RemotePanelHwnd)) {
        SetForegroundWindow(g_RemotePanelHwnd);
        return;
    }

    // Load current config into edit buffers
    RemoteAccessConfig cfg = GetRemoteAccessConfig();
    g_RemoteToggleState = cfg.enabled || IsRemoteAccessEnabled();
    strncpy(g_RemotePassBuf, cfg.password, sizeof(g_RemotePassBuf) - 1);
    snprintf(g_RemotePortBuf, sizeof(g_RemotePortBuf), "%d", cfg.port);
    snprintf(g_RemoteTimeoutBuf, sizeof(g_RemoteTimeoutBuf), "%d", cfg.inactivityTimeout);

    const char* cls = "ZPRemotePanel";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = RemotePanelProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground  = CreateSolidBrush(VE_ICY_BG);
        RegisterClassA(&wc);
        registered = true;
    }

    int panelW = 340, panelH = 380;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_RemotePanelHwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        cls, "Remote Access",
        WS_POPUP | WS_VISIBLE,
        (sx - panelW) / 2, (sy - panelH) / 2, panelW, panelH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(g_RemotePanelHwnd, 0, 240, LWA_ALPHA);
    ApplyDisplayAffinity(g_RemotePanelHwnd);
    ShowWindow(g_RemotePanelHwnd, SW_SHOW);
    SetForegroundWindow(g_RemotePanelHwnd);
    // Fully modeless — messages dispatched by WinMain's PeekMessage loop.
    // No blocking GetMessage loop here.
}
re.
}
otePanelHwnd, 0, 240, LWA_ALPHA);
    ApplyDisplayAffinity(g_RemotePanelHwnd);
    ShowWindow(g_RemotePanelHwnd, SW_SHOW);
    SetForegroundWindow(g_RemotePanelHwnd);
    // Fully modeless — messages dispatched by WinMain's PeekMessage loop.
    // No blocking GetMessage loop here.
}
re.
}
