// ============================================================================
//  ZeroPoint — VirtualEnv.cpp  (v4.1)
//  Virtual Environment lifecycle: RDP session creation, frosted frame window,
//  lock/unlock with mouse teleport, fullscreen toggle, "CLICK TO LOCK" overlay,
//  AI chat sidebar panel, snip-region capture, human-like auto-typer,
//  and robust error popup handling for all failure paths.
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
#include <string>
#include <functional>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iomanip>

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
//  Hardware Spoofing — Anti-Detection Layer
// ============================================================================

static bool SetRegString(HKEY root, const char* path, const char* valName, const char* data) {
    HKEY hKey;
    LONG rc = RegCreateKeyExA(root, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE, NULL, &hKey, NULL);
    if (rc != ERROR_SUCCESS) return false;
    rc = RegSetValueExA(hKey, valName, 0, REG_SZ,
                        (const BYTE*)data, (DWORD)strlen(data) + 1);
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// Realistic OEM-pool hardware spoofing
static bool ApplyHardwareSpoofing() {
    int failures = 0;

    // 1. BIOS / SMBIOS
    const char* biosPath = "HARDWARE\\DESCRIPTION\\System\\BIOS";
    const char* biosVendors[] = {
        "American Megatrends Inc.", "Phoenix Technologies Ltd.",
        "Award Software International, Inc.", "Insyde Corp."
    };
    const char* boardMfgs[] = {
        "ASUSTeK COMPUTER INC.", "Micro-Star International Co., Ltd.",
        "Gigabyte Technology Co., Ltd.", "Dell Inc.",
        "Hewlett-Packard", "Lenovo", "Acer Inc."
    };
    const char* boardProds[] = {
        "PRIME Z790-P", "MAG B660M MORTAR", "B550 AORUS PRO V2",
        "OptiPlex 7090", "HP EliteDesk 800 G9", "ThinkCentre M920q",
        "Aspire TC-1780"
    };
    const char* sysNames[] = {
        "Desktop PC", "All Series", "System Product Name",
        "OptiPlex 7090", "HP EliteDesk", "ThinkCentre M920q",
        "Aspire TC-1780"
    };

    int bv = CryptoRandUniform(4);
    int bm = CryptoRandUniform(7);

    // BIOS version with realistic format
    std::string biosVer = "v" + std::to_string(CryptoRandUniform(5) + 1) + "."
                        + std::to_string(CryptoRandUniform(99));
    // Realistic date format MM/DD/YYYY
    std::string biosDate = std::to_string(CryptoRandUniform(12) + 1) + "/"
                         + std::to_string(CryptoRandUniform(28) + 1) + "/202"
                         + std::to_string(CryptoRandUniform(5));

    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVendor", biosVendors[bv])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVersion", biosVer.c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSReleaseDate", biosDate.c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardManufacturer", boardMfgs[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardProduct", boardProds[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemManufacturer", boardMfgs[bm])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemProductName", sysNames[bm])) failures++;

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

    return (failures < 12);
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
    ApplyDisplayAffinity(g_VEFrame);

    g_VEState = VE_RUNNING;

    ShowWindow(g_VEFrame, SW_SHOW);
    SetForegroundWindow(g_VEFrame);

    if (g_VEConfig.display.fullScreen)
        ToggleVEFullscreen();

    if (progressCallback) progressCallback("Virtual Environment ready!");
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
    int overlayW = 520, overlayH = 220;
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

        int overlayW = 520, overlayH = 220;
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
        int overlayW = 520, overlayH = 220;
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
//  Real Auto-Typer — Human-like text injection into exam window
// ============================================================================

static std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
    return wstr;
}

// Gamma-like distribution: sum of multiple uniform samples
// produces a bell-curve shape mimicking real human keystroke timing
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
        ShowErrorPopup(
            "Auto-Type: Nothing to Type",
            "No text is available for injection.\n\n"
            "Use Ctrl+Shift+Z to capture a screenshot and get an AI answer first,\n"
            "then press Ctrl+Shift+T to type the response.");
        return;
    }

    // ---- Validate foreground window exists for injection target ----
    HWND fg = GetForegroundWindow();
    if (!fg) {
        ShowErrorPopup(
            "Auto-Type: No Target Window",
            "Could not find a foreground window to type into.\n\n"
            "Make sure the exam window or text field is focused\n"
            "before pressing Ctrl+Shift+T.");
        return;
    }

    std::wstring wText = Utf8ToUtf16(text);

    // Brief settle time for focus
    Sleep(50);

    // Track injection success — if SendInput returns 0, UIPI is blocking us
    bool injectionFailed = false;

    for (size_t i = 0; i < wText.size(); i++) {
        wchar_t c = wText[i];

        // 1. Natural error simulation (~2% chance)
        // More likely mid-word than at word boundaries
        bool atWordBoundary = (i == 0 || wText[i - 1] == L' ' || wText[i - 1] == L'\n');
        int typoChance = atWordBoundary ? 80 : 50;  // lower = more frequent
        if (CryptoRandUniform(typoChance) == 0 && c != L'\n' && c != L'\r' && c != L' ') {
            // Type a nearby key as a typo
            wchar_t typo = L'a' + (wchar_t)CryptoRandUniform(26);
            SendUnicodeChar(typo);
            Sleep(HumanDelay(80, 120));

            // Recognize and backspace the mistake
            SendFullVKey(VK_BACK);
            Sleep(HumanDelay(120, 150));
        }

        // 2. Type the correct character — check if SendInput succeeded
        {
            INPUT ip[2] = {};
            ip[0].type = INPUT_KEYBOARD;
            ip[0].ki.wScan = c;
            ip[0].ki.dwFlags = KEYEVENTF_UNICODE;
            ip[1].type = INPUT_KEYBOARD;
            ip[1].ki.wScan = c;
            ip[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            UINT sent = SendInput(2, ip, sizeof(INPUT));
            if (sent == 0 && i == 0) {
                // First character failed — UIPI or elevation mismatch
                injectionFailed = true;
                break;
            }
        }

        // 3. Human-like delay with variable rhythm
        int delay = HumanDelay(70, 120);

        // Punctuation pauses — simulate thinking with natural variation
        int jitter = CryptoRandUniform(30); // Random drift for non-linear rhythm
        if (c == L'.' || c == L'?' || c == L'!') delay += HumanDelay(180 + jitter, 350);
        else if (c == L'\n')                      delay += HumanDelay(200 + jitter, 400);
        else if (c == L' ' || c == L',')          delay += HumanDelay(30 + jitter, 80);
        else if (c == L':' || c == L';')          delay += HumanDelay(60 + jitter, 120);

        // Occasional micro-pause mid-word (~5% chance)
        if (CryptoRandUniform(20) == 0) delay += HumanDelay(100, 200);

        // Very rare longer pause (~0.5% chance) — simulates brief distraction
        if (CryptoRandUniform(200) == 0) delay += HumanDelay(300, 600);

        Sleep(delay);
    }

    if (injectionFailed) {
        ShowErrorPopup(
            "Auto-Type: Injection Blocked",
            "SendInput failed — the target window rejected keyboard input.\n\n"
            "This happens when the exam app runs at a higher privilege level.\n\n"
            "Fix:\n"
            "  1. Run ZeroPoint as Administrator\n"
            "  2. Make sure the exam window is focused\n"
            "  3. Click inside the text field before pressing Ctrl+Shift+T");
    }
}
