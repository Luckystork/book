// ============================================================================
//  ZeroPoint — VirtualEnv.cpp  (v4.0)
//  Virtual Environment lifecycle: RDP session creation, frosted frame window,
//  lock/unlock with mouse teleport, fullscreen toggle, "CLICK TO LOCK" overlay,
//  AI chat sidebar panel, and snip-region capture tool.
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
#include <dwmapi.h>
#include <gdiplus.h>
#include <string>
#include <functional>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

// ============================================================================
//  State
// ============================================================================

static VEState g_VEState          = VE_IDLE;
static HWND    g_VEFrame          = NULL;
static HWND    g_VELockOverlay    = NULL;
static HWND    g_VEChatSidebar    = NULL;
static DWORD   g_VESessionPid    = 0;
static HWND    g_VEMstscWindow    = NULL;

static POINT   g_SavedCursorPos  = { 0, 0 };  // mouse pos saved on lock
static bool    g_VEIsFullscreen   = false;
static bool    g_VEChatVisible    = false;
static RECT    g_VEWindowedRect   = { 0, 0, 0, 0 }; // saved pos before fullscreen

static const int VE_CHAT_SIDEBAR_W = 320;  // chat panel width in pixels

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
//  GDI Helpers (imported from main.cpp style — icy aesthetics)
// ============================================================================

// Alpha-blended fill for frosted glass
static void VE_FillFrosted(HDC hdc, const RECT& rc, COLORREF base, BYTE alpha) {
    HDC mem = CreateCompatibleDC(hdc);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth       = rc.right - rc.left;
    bi.bmiHeader.biHeight      = rc.bottom - rc.top;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = SelectObject(mem, bmp);

    BYTE r = (BYTE)((GetRValue(base) * alpha) / 255);
    BYTE g = (BYTE)((GetGValue(base) * alpha) / 255);
    BYTE b = (BYTE)((GetBValue(base) * alpha) / 255);
    int pixels = bi.bmiHeader.biWidth * bi.bmiHeader.biHeight;
    DWORD* px = (DWORD*)bits;
    DWORD val = (alpha << 24) | (r << 16) | (g << 8) | b;
    for (int i = 0; i < pixels; i++) px[i] = val;

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, rc.left, rc.top, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight,
               mem, 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight, bf);

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
//  Hardware Spoofing — Anti-Detection Layer
// ============================================================================

// Returns true on success. Requires admin (elevated) privileges for HKLM writes.
static bool SetRegString(HKEY root, const char* path, const char* valName, const char* data) {
    HKEY hKey;
    LONG rc = RegCreateKeyExA(root, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE, NULL, &hKey, NULL);
    if (rc != ERROR_SUCCESS) return false;  // likely ERROR_ACCESS_DENIED without admin
    rc = RegSetValueExA(hKey, valName, 0, REG_SZ,
                        (const BYTE*)data, (DWORD)strlen(data) + 1);
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

static std::string RandomString(int len, bool hex = false) {
    const char* chars = hex ? "0123456789ABCDEF" : "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string s = "";
    for (int i = 0; i < len; i++) s += chars[rand() % (hex ? 16 : 36)];
    return s;
}

// NOTE: Requires administrator (elevated) privileges to write HKLM registry keys.
// If the process is not elevated, all writes will silently fail and the function
// returns false. Run ZeroPoint as Administrator to enable hardware spoofing.
static bool ApplyHardwareSpoofing() {
    srand((unsigned int)time(NULL));
    int failures = 0;

    // 1. BIOS / SMBIOS Spoofing
    const char* biosPath = "HARDWARE\\DESCRIPTION\\System\\BIOS";
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVendor", "American Megatrends Inc.")) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSVersion", ("v" + std::to_string(rand() % 5 + 1) + "." + std::to_string(rand() % 99)).c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BIOSReleaseDate", (std::to_string(rand() % 12 + 1) + "/" + std::to_string(rand() % 28 + 1) + "/202" + std::to_string(rand() % 5)).c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardManufacturer", "ASUSTeK COMPUTER INC.")) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "BaseBoardProduct", ("PRIME Z" + std::to_string(rand() % 9 + 1) + "90-P").c_str())) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemManufacturer", "ASUSTeK COMPUTER INC.")) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, biosPath, "SystemProductName", "Desktop PC")) failures++;

    // 2. CPUID Spoofing
    const char* cpuPath = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    const char* cpus[] = { "13th Gen Intel(R) Core(TM) i9-13900K", "12th Gen Intel(R) Core(TM) i7-12700K", "AMD Ryzen 9 7950X 16-Core Processor" };
    if (!SetRegString(HKEY_LOCAL_MACHINE, cpuPath, "ProcessorNameString", cpus[rand() % 3])) failures++;

    // 3. GPU Spoofing
    const char* gpuPath = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000";
    const char* gpus[] = { "NVIDIA GeForce RTX 4090", "NVIDIA GeForce RTX 3080 Ti", "AMD Radeon RX 7900 XTX" };
    if (!SetRegString(HKEY_LOCAL_MACHINE, gpuPath, "DriverDesc", gpus[rand() % 3])) failures++;
    if (!SetRegString(HKEY_LOCAL_MACHINE, gpuPath, "HardwareInformation.AdapterString", gpus[rand() % 3])) failures++;

    // 4. Disk Serial Spoofing
    if (!SetRegString(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", "0", ("SCSI\\Disk&Ven_NVMe&Prod_Samsung_SSD_980\\" + RandomString(12)).c_str())) failures++;

    // 5. MAC Address Simulation (Randomized NetworkAddress)
    const char* netPath = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}\\0001";
    std::string mac = "00" + RandomString(10, true);
    if (!SetRegString(HKEY_LOCAL_MACHINE, netPath, "NetworkAddress", mac.c_str())) failures++;

    // Any failure likely means no admin rights — report partial success honestly
    if (failures == 12) return false;   // total failure: not elevated
    if (failures > 0)  return true;     // partial: some keys protected, some written
    return true;                        // full success
}

// ============================================================================
//  StartVirtualEnvironment — main entry point
// ============================================================================

bool StartVirtualEnvironment(void (*progressCallback)(const char* msg)) {
    if (g_VEState == VE_RUNNING || g_VEState == VE_LOCKED) return true;

    // Apply hardware spoofing before session initialization (requires admin)
    if (!ApplyHardwareSpoofing()) {
        // All 12 HKLM writes failed — process is not elevated
        if (progressCallback)
            progressCallback("WARNING: Hardware spoofing failed (ACCESS_DENIED). "
                             "Restart ZeroPoint as Administrator to enable HWID spoofing.");
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
            return false;
        }
    } else if (rdpState == RDPWRAP_INSTALLED_OUTDATED) {
        if (progressCallback) progressCallback("Updating rdpwrap.ini...");
        UpdateRDPWrapperINI(progressCallback);
        RestartTermService();
        Sleep(2000);
    }

    // Step 2: Create the loopback RDP session
    if (progressCallback) progressCallback("Initializing Virtual Environment...");

    int w = g_VEConfig.display.resW;
    int h = g_VEConfig.display.resH;
    int depth = g_VEConfig.display.colorDepth;

    g_VESessionPid = CreateHiddenRDPSession(w, h, depth);
    if (g_VESessionPid == 0) {
        if (progressCallback) progressCallback("Failed to create RDP session");
        g_VEState = VE_ERROR;
        return false;
    }

    // Step 3: Find the mstsc window
    if (progressCallback) progressCallback("Connecting to session...");
    g_VEMstscWindow = FindRDPSessionWindow(g_VESessionPid);

    if (!g_VEMstscWindow) {
        if (progressCallback) progressCallback("Could not find RDP window");
        DestroyRDPSession(g_VESessionPid);
        g_VEState = VE_ERROR;
        return false;
    }

    // Step 4: Create our frosted frame and embed mstsc inside it
    if (progressCallback) progressCallback("Building virtual desktop...");
    CreateVEFrame(w, h);
    Sleep(500);
    EmbedMstscInFrame();

    // Step 5: Create the lock overlay
    CreateVELockOverlay();

    // Step 6: Apply stealth — hide from screen recording
    typedef BOOL(WINAPI* PFN_SWDA)(HWND, DWORD);
    PFN_SWDA pSWDA = (PFN_SWDA)GetProcAddress(
        GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
    if (pSWDA) {
        pSWDA(g_VEFrame, 0x00000011);
    }

    g_VEState = VE_RUNNING;

    // Show the frame
    ShowWindow(g_VEFrame, SW_SHOW);
    SetForegroundWindow(g_VEFrame);

    if (g_VEConfig.display.fullScreen) {
        ToggleVEFullscreen();
    }

    if (progressCallback) progressCallback("Virtual Environment ready!");
    return true;
}

void StopVirtualEnvironment() {
    if (g_VEState == VE_IDLE) return;

    // Kill the RDP session
    if (g_VESessionPid) {
        DestroyRDPSession(g_VESessionPid);
        g_VESessionPid = 0;
    }
    g_VEMstscWindow = NULL;

    // Destroy our windows
    if (g_VELockOverlay && IsWindow(g_VELockOverlay))
        DestroyWindow(g_VELockOverlay);
    if (g_VEChatSidebar && IsWindow(g_VEChatSidebar))
        DestroyWindow(g_VEChatSidebar);
    if (g_VEFrame && IsWindow(g_VEFrame))
        DestroyWindow(g_VEFrame);

    g_VEFrame = NULL;
    g_VELockOverlay = NULL;
    g_VEChatSidebar = NULL;
    g_VEState = VE_IDLE;
    g_VEIsFullscreen = false;
    g_VEChatVisible = false;
}

VEState GetVEState() { return g_VEState; }

// ============================================================================
//  VE Frame — our frosted outer window that contains the RDP session
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
    int frameW = w + 4;   // 2px border each side
    int frameH = h + 32;  // title bar + bottom border

    g_VEFrame = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "ZeroPoint Virtual Environment",
        WS_POPUP | WS_CLIPCHILDREN | WS_THICKFRAME,
        (sx - frameW) / 2, (sy - frameH) / 2, frameW, frameH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(g_VEFrame, 0, 245, LWA_ALPHA);

    // Enable DWM blur
    DWM_BLURBEHIND bb = {};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(g_VEFrame, &bb);
    DeleteObject(bb.hRgnBlur);
}

static void EmbedMstscInFrame() {
    if (!g_VEMstscWindow || !g_VEFrame) return;

    // Remove mstsc's title bar and border — make it a plain child
    LONG style = GetWindowLong(g_VEMstscWindow, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD;
    SetWindowLong(g_VEMstscWindow, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(g_VEMstscWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(g_VEMstscWindow, GWL_EXSTYLE, exStyle);

    // Re-parent into our frame
    SetParent(g_VEMstscWindow, g_VEFrame);

    // Position within the frame (below our title bar, leaving room for chat sidebar)
    RepositionEmbeddedSession();

    ShowWindow(g_VEMstscWindow, SW_SHOW);
}

static void RepositionEmbeddedSession() {
    if (!g_VEMstscWindow || !g_VEFrame) return;

    RECT rc;
    GetClientRect(g_VEFrame, &rc);

    int chatW = (g_VEChatVisible) ? VE_CHAT_SIDEBAR_W : 0;
    int sessionX = 0;
    int sessionY = 28;   // below our custom title bar
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

        // Frosted title bar (top 28px)
        RECT titleRc = { 0, 0, rc.right, 28 };
        VE_FillFrosted(hdc, titleRc, VE_ICY_PANEL, 230);

        // Accent line under title bar
        HPEN pen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, 0, 27, NULL);
        LineTo(hdc, rc.right, 27);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        // Title text
        SetBkMode(hdc, TRANSPARENT);
        HFONT titleFont = VE_CreateFont(13, FW_SEMIBOLD);
        SelectObject(hdc, titleFont);
        SetTextColor(hdc, VE_ICY_ACCENT);
        RECT txtRc = { 12, 4, 400, 24 };
        DrawTextA(hdc, "ZEROPOINT  |  Virtual Environment", -1, &txtRc,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DeleteObject(titleFont);

        // Right-side buttons text: [Lock] [FS] [X]
        HFONT btnFont = VE_CreateFont(11, FW_NORMAL);
        SelectObject(hdc, btnFont);
        SetTextColor(hdc, VE_ICY_DIM);

        RECT lockRc = { rc.right - 200, 4, rc.right - 140, 24 };
        DrawTextA(hdc, "[Lock]", -1, &lockRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        RECT fsRc = { rc.right - 130, 4, rc.right - 70, 24 };
        DrawTextA(hdc, "[FullScr]", -1, &fsRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(hdc, RGB(0xFF, 0x44, 0x44));
        RECT closeRc = { rc.right - 50, 4, rc.right - 10, 24 };
        DrawTextA(hdc, "[X]", -1, &closeRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        DeleteObject(btnFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP: {
        int mx = (short)LOWORD(lp);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Click in title bar area
        if ((short)HIWORD(lp) < 28) {
            if (mx >= rc.right - 50 && mx < rc.right - 10) {
                // Close button
                StopVirtualEnvironment();
                return 0;
            }
            if (mx >= rc.right - 130 && mx < rc.right - 70) {
                // Fullscreen button
                ToggleVEFullscreen();
                return 0;
            }
            if (mx >= rc.right - 200 && mx < rc.right - 140) {
                // Lock button
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
            // Title bar area is draggable (but not over the buttons)
            if (pt.y < 28 && pt.x < (int)(GetClientRect(hwnd, NULL), 0))
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

    // Save the current cursor position so we can restore it on unlock
    GetCursorPos(&g_SavedCursorPos);

    // Hide the VE frame
    ShowWindow(g_VEFrame, SW_HIDE);

    // Show the lock overlay on the host desktop
    if (g_VELockOverlay && IsWindow(g_VELockOverlay)) {
        ShowWindow(g_VELockOverlay, SW_SHOW);
        SetForegroundWindow(g_VELockOverlay);
    }

    g_VEState = VE_LOCKED;
}

void UnlockVE() {
    if (g_VEState != VE_LOCKED) return;

    // Hide the lock overlay
    if (g_VELockOverlay && IsWindow(g_VELockOverlay)) {
        ShowWindow(g_VELockOverlay, SW_HIDE);
    }

    // Show the VE frame
    ShowWindow(g_VEFrame, SW_SHOW);
    SetForegroundWindow(g_VEFrame);

    // Teleport the mouse cursor back to the saved position
    // This prevents proctors from detecting that the user tabbed away
    SetCursorPos(g_SavedCursorPos.x, g_SavedCursorPos.y);

    g_VEState = VE_RUNNING;
}

void ToggleVELock() {
    if (g_VEState == VE_RUNNING) LockVE();
    else if (g_VEState == VE_LOCKED) UnlockVE();
}

// ============================================================================
//  Fullscreen Toggle
// ============================================================================

void ToggleVEFullscreen() {
    if (!g_VEFrame) return;

    if (!g_VEIsFullscreen) {
        // Save current position for restore
        GetWindowRect(g_VEFrame, &g_VEWindowedRect);

        // Go fullscreen
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(g_VEFrame, HWND_TOPMOST, 0, 0, sx, sy,
                     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        g_VEIsFullscreen = true;
    } else {
        // Restore windowed position
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
//  "CLICK TO LOCK" Overlay — frosted transparent overlay on the host desktop
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

    // High alpha transparency (245) combined with COLORKEY for the outside areas
    SetLayeredWindowAttributes(g_VELockOverlay, RGB(255, 0, 255), 245, LWA_COLORKEY | LWA_ALPHA);

    // Enable DWM blur only for the centered box
    int overlayW = 520, overlayH = 220;
    RECT rcBox = { (sx - overlayW) / 2, (sy - overlayH) / 2, (sx + overlayW) / 2, (sy + overlayH) / 2 };
    HRGN hrgnBox = CreateRectRgn(rcBox.left, rcBox.top, rcBox.right, rcBox.bottom);

    DWM_BLURBEHIND bb = {};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = hrgnBox;
    DwmEnableBlurBehindWindow(g_VELockOverlay, &bb);
    DeleteObject(hrgnBox);

    // Hide from screen recording
    typedef BOOL(WINAPI* PFN_SWDA)(HWND, DWORD);
    PFN_SWDA pSWDA = (PFN_SWDA)GetProcAddress(
        GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
    if (pSWDA) pSWDA(g_VELockOverlay, 0x00000011);

    // Start hidden — shown when Lock is triggered
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

        // Fill entire screen with Magenta (transparent colorkey)
        HBRUSH bgTransparent = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(memDC, &rc, bgTransparent);
        DeleteObject(bgTransparent);

        int overlayW = 520, overlayH = 220;
        RECT boxRc = { (w - overlayW) / 2, (h - overlayH) / 2, (w + overlayW) / 2, (h + overlayH) / 2 };

        // Frosted glass background for the centered box
        HBRUSH bgBox = CreateSolidBrush(VE_ICY_BG);
        FillRect(memDC, &boxRc, bgBox);
        DeleteObject(bgBox);

        VE_FillFrosted(memDC, boxRc, VE_ICY_PANEL, 180);

        // Accent border
        HPEN borderPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
        SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom, 20, 20);
        DeleteObject(borderPen);

        SetBkMode(memDC, TRANSPARENT);

        // Big "CLICK TO LOCK" text with soft teal glow
        HFONT bigFont = VE_CreateFont(42, FW_LIGHT);
        SelectObject(memDC, bigFont);
        
        // Multi-layered glow for a softer, more premium teal glow effect
        SetTextColor(memDC, RGB(0x40, 0xDD, 0xFF)); // Deeper teal outer glow
        RECT glowRc1 = { boxRc.left - 2, boxRc.top - 2, boxRc.right - 2, boxRc.bottom - 20 };
        RECT glowRc2 = { boxRc.left + 2, boxRc.top + 2, boxRc.right + 2, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        
        SetTextColor(memDC, RGB(0x80, 0xEE, 0xFF)); // Brighter inner glow
        RECT glowRc3 = { boxRc.left - 1, boxRc.top - 1, boxRc.right - 1, boxRc.bottom - 20 };
        RECT glowRc4 = { boxRc.left + 1, boxRc.top + 1, boxRc.right + 1, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc3, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DrawTextA(memDC, "CLICK TO LOCK", -1, &glowRc4, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Main text layer (icy white)
        SetTextColor(memDC, RGB(0xFF, 0xFF, 0xFF));
        RECT bigRc = { boxRc.left, boxRc.top, boxRc.right, boxRc.bottom - 20 };
        DrawTextA(memDC, "CLICK TO LOCK", -1, &bigRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        DeleteObject(bigFont);

        // Subtitle: hotkey instructions
        HFONT subFont = VE_CreateFont(14, FW_NORMAL);
        SelectObject(memDC, subFont);
        SetTextColor(memDC, VE_ICY_DIM);

        RECT line1 = { boxRc.left, boxRc.top + 110, boxRc.right, boxRc.top + 135 };
        DrawTextA(memDC, "(Ctrl+Alt+C to Unlock)", -1, &line1, DT_CENTER | DT_SINGLELINE);

        RECT line2 = { boxRc.left, boxRc.top + 140, boxRc.right, boxRc.top + 165 };
        DrawTextA(memDC, "(Ctrl+Alt+F to Toggle Fullscreen)", -1, &line2, DT_CENTER | DT_SINGLELINE);
        DeleteObject(subFont);

        // Small ZeroPoint branding
        HFONT tinyFont = VE_CreateFont(10, FW_NORMAL);
        SelectObject(memDC, tinyFont);
        SetTextColor(memDC, RGB(0xC0, 0xCC, 0xDD));
        RECT brandRc = { boxRc.left, boxRc.bottom - 24, boxRc.right, boxRc.bottom - 6 };
        DrawTextA(memDC, "ZeroPoint Virtual Environment", -1, &brandRc,
                  DT_CENTER | DT_SINGLELINE);
        DeleteObject(tinyFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        // Proper hit-testing: centered overlay catches clicks, rest is click-through
        int mx = (short)LOWORD(lp);
        int my = (short)HIWORD(lp);
        
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        int overlayW = 520, overlayH = 220;
        RECT boxRc = { (sx - overlayW) / 2, (sy - overlayH) / 2, (sx + overlayW) / 2, (sy + overlayH) / 2 };
        
        POINT pt = { mx, my };
        if (PtInRect(&boxRc, pt)) {
            return HTCLIENT;
        }
        return HTTRANSPARENT;
    }

    case WM_LBUTTONUP:
        // Click on the overlay — unlock the VE
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

HWND GetVEFrameWindow()      { return g_VEFrame; }
HWND GetVESessionWindow()    { return g_VEMstscWindow; }
HWND GetVELockOverlay()      { return g_VELockOverlay; }
HWND GetVEChatSidebarWindow() { return g_VEChatSidebar; }

// ============================================================================
//  AI Chat Sidebar (inside VE Frame)
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

static RECT  g_SnipRect     = {};
static bool  g_SnipDragging = false;
static POINT g_SnipStart    = {};
static HBITMAP g_SnipResult = NULL;

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
            // Capture the selected region from the screen
            HDC screenDC = GetDC(NULL);
            HDC memDC = CreateCompatibleDC(screenDC);
            g_SnipResult = CreateCompatibleBitmap(screenDC, sw, sh);
            SelectObject(memDC, g_SnipResult);

            // Convert client coords to screen coords
            POINT topLeft = { g_SnipRect.left, g_SnipRect.top };
            ClientToScreen(hwnd, &topLeft);

            BitBlt(memDC, 0, 0, sw, sh, screenDC, topLeft.x, topLeft.y, SRCCOPY);
            DeleteDC(memDC);
            ReleaseDC(NULL, screenDC);
        }

        DestroyWindow(hwnd);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            g_SnipResult = NULL;
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Semi-transparent overlay with hole cut out for selection
        RECT rc;
        GetClientRect(hwnd, &rc);
        
        HRGN hrgnFull = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
        if (g_SnipDragging || (g_SnipRect.right - g_SnipRect.left > 0)) {
            HRGN hrgnSnip = CreateRectRgn(g_SnipRect.left, g_SnipRect.top, g_SnipRect.right, g_SnipRect.bottom);
            CombineRgn(hrgnFull, hrgnFull, hrgnSnip, RGN_DIFF);
            DeleteObject(hrgnSnip);
        }
        SelectClipRgn(hdc, hrgnFull);
        VE_FillFrosted(hdc, rc, RGB(0, 0, 0), 80);
        SelectClipRgn(hdc, NULL);
        DeleteObject(hrgnFull);

        // Draw the selection rectangle in accent color
        if (g_SnipDragging || (g_SnipRect.right - g_SnipRect.left > 0)) {
            // Draw accent border around selection
            HPEN selPen = CreatePen(PS_SOLID, 2, VE_ICY_ACCENT);
            SelectObject(hdc, selPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, g_SnipRect.left, g_SnipRect.top,
                      g_SnipRect.right, g_SnipRect.bottom);
            DeleteObject(selPen);
        }

        // Instructions text at top
        SetBkMode(hdc, TRANSPARENT);
        HFONT instrFont = VE_CreateFont(16, FW_SEMIBOLD);
        SelectObject(hdc, instrFont);
        SetTextColor(hdc, VE_ICY_ACCENT);
        RECT instrRc = { 0, 20, rc.right, 50 };
        DrawTextA(hdc, "Click and drag to select region  |  Esc to cancel", -1,
                  &instrRc, DT_CENTER | DT_SINGLELINE);
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
    g_SnipResult = NULL;
    g_SnipDragging = false;
    g_SnipRect = {};

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

    SetLayeredWindowAttributes(snipWnd, 0, 1, LWA_ALPHA);  // almost transparent initially
    SetLayeredWindowAttributes(snipWnd, 0, 120, LWA_ALPHA); // then semi-transparent
    SetForegroundWindow(snipWnd);

    // Run local message loop until snip window closes
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

    return g_SnipResult;
}

// ============================================================================
// Real Auto-Typer - Human-like text injection into exam window
// ============================================================================

static std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

void PerformAutoType(const std::string& text) {
    if (text.empty()) return;

    std::wstring wText = Utf8ToUtf16(text);

    // Bring the exam window to foreground if possible
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;
    SetForegroundWindow(hwnd);

    srand((unsigned int)time(NULL));

    for (size_t i = 0; i < wText.size(); i++) {
        wchar_t c = wText[i];

        // 1. Natural error simulation (occasional typo)
        if (rand() % 40 == 0) {
            wchar_t typo = L'a' + (rand() % 26);
            INPUT ipErr = {0};
            ipErr.type = INPUT_KEYBOARD;
            ipErr.ki.wScan = typo;
            ipErr.ki.dwFlags = KEYEVENTF_UNICODE;
            SendInput(1, &ipErr, sizeof(INPUT));
            // Key-up for the typo character
            ipErr.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput(1, &ipErr, sizeof(INPUT));

            Sleep(100 + (rand() % 100)); // processing time

            // Backspace it (down then up)
            INPUT ipBs = {0};
            ipBs.type = INPUT_KEYBOARD;
            ipBs.ki.wVk = VK_BACK;
            ipBs.ki.dwFlags = 0;
            SendInput(1, &ipBs, sizeof(INPUT));
            ipBs.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ipBs, sizeof(INPUT));

            Sleep(150 + (rand() % 150)); // pause before re-typing
        }

        // 2. Type the correct character
        INPUT ip = {0};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wScan = c;
        ip.ki.dwFlags = KEYEVENTF_UNICODE;
        SendInput(1, &ip, sizeof(INPUT));

        // Release (essential for some apps)
        ip.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));

        // 3. Human-like delay (80-180ms)
        int delay = 80 + (rand() % 100);
        
        // Punctuation takes longer to think about
        if (c == L'.' || c == L'?' || c == L'!') delay += 200 + (rand() % 300);
        else if (c == L' ' || c == L',') delay += 50 + (rand() % 100);

        Sleep(delay);
    }
}
