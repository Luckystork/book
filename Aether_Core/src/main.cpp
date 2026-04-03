// ============================================================================
//  ZeroPoint — Premium Windows Utility
//  main.cpp — Frosted glass launcher, icy/snowy theme, WebView2 invisible
//             browser, enhanced AI popup, custom accent color + transparency
// ============================================================================
//
//  BUILD REQUIREMENTS:
//    - Microsoft.Web.WebView2 NuGet package (WebView2Loader.dll + headers)
//    - Windows 10 1809+ with Edge WebView2 Runtime installed
//    - Link: winhttp, dwmapi, comctl32, gdi32, msimg32, WebView2Loader
//
// ============================================================================

#include "Stealth.h"
#include "CDPExtractor.h"
#include "Config.h"

#include <windows.h>
#include <wingdi.h>
#include <winhttp.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <wrl.h>         // Microsoft::WRL::ComPtr for WebView2 COM pointers
#include <wil/com.h>     // wil helpers (optional, from WebView2 SDK samples)

// ---- WebView2 headers ----
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "WebView2Loader.lib")

using namespace Microsoft::WRL;

// ============================================================================
//  Theme Configuration
// ============================================================================
// Icy / snowy / white theme — bright, clean, frosted glass aesthetic.
// Accent color and window alpha are user-configurable and persisted.
//
//  config.ini lines (appended after the OpenRouter key):
//    accent=#RRGGBB           (accent color, default #00DDFF)
//    alpha=230                (window opacity 0-255, default 230)

static COLORREF g_AccentColor   = RGB(0x00, 0xDD, 0xFF);   // icy cyan default
static BYTE     g_WindowAlpha   = 230;                      // transparency level

// Icy/snowy palette — bright whites with subtle blue tints
static COLORREF g_BgColor       = RGB(0xF0, 0xF4, 0xFA);   // soft snow white
static COLORREF g_BgPanel       = RGB(0xFF, 0xFF, 0xFF);   // pure white panels
static COLORREF g_BgFrost       = RGB(0xE8, 0xEF, 0xF8);   // frosted overlay tint
static COLORREF g_TextPrimary   = RGB(0x1A, 0x1E, 0x2C);   // crisp dark text
static COLORREF g_TextSecondary = RGB(0x60, 0x6A, 0x80);   // dimmed cool gray
static COLORREF g_BorderColor   = RGB(0xD0, 0xD8, 0xE8);   // subtle icy border
static COLORREF g_BtnNormal     = RGB(0xF5, 0xF8, 0xFF);   // button bg
static COLORREF g_BtnHover      = RGB(0xE0, 0xEA, 0xF8);   // button hover
static COLORREF g_ShadowColor   = RGB(0xC0, 0xCC, 0xDD);   // soft shadow
static COLORREF g_GlowColor     = RGB(0xD8, 0xEE, 0xFF);   // inner glow tint

static const char* THEME_CONFIG = "C:\\ProgramData\\ZeroPoint\\config.ini";

// ============================================================================
//  Theme Persistence — Load / Save accent color + alpha to config.ini
// ============================================================================

static void LoadThemeSettings() {
    std::ifstream f(THEME_CONFIG);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("accent=", 0) == 0 && line.size() >= 14) {
            unsigned int r = 0, g = 0, b = 0;
            if (sscanf(line.c_str() + 8, "%02x%02x%02x", &r, &g, &b) == 3) {
                g_AccentColor = RGB(r, g, b);
            }
        }
        if (line.rfind("alpha=", 0) == 0) {
            int val = atoi(line.c_str() + 6);
            if (val >= 0 && val <= 255) g_WindowAlpha = (BYTE)val;
        }
    }
}

static void SaveThemeSettings() {
    std::vector<std::string> lines;
    bool foundAccent = false, foundAlpha = false;
    {
        std::ifstream f(THEME_CONFIG);
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("accent=", 0) == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "accent=#%02X%02X%02X",
                         GetRValue(g_AccentColor),
                         GetGValue(g_AccentColor),
                         GetBValue(g_AccentColor));
                lines.push_back(buf);
                foundAccent = true;
            } else if (line.rfind("alpha=", 0) == 0) {
                lines.push_back("alpha=" + std::to_string(g_WindowAlpha));
                foundAlpha = true;
            } else {
                lines.push_back(line);
            }
        }
    }
    if (!foundAccent) {
        char buf[32];
        snprintf(buf, sizeof(buf), "accent=#%02X%02X%02X",
                 GetRValue(g_AccentColor),
                 GetGValue(g_AccentColor),
                 GetBValue(g_AccentColor));
        lines.push_back(buf);
    }
    if (!foundAlpha) {
        lines.push_back("alpha=" + std::to_string(g_WindowAlpha));
    }
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    std::ofstream out(THEME_CONFIG);
    for (auto& l : lines) out << l << "\n";
}

// ============================================================================
//  GDI Helpers — frosted glass drawing primitives
// ============================================================================

static HFONT CreateAppFont(int size, int weight = FW_NORMAL) {
    return CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

// Alpha-blended fill — premultiplied alpha for true translucency
static void FillFrosted(HDC hdc, const RECT& rc, COLORREF base, BYTE alpha) {
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

// Rounded-rect button with optional accent highlight
static void DrawIcyButton(HDC hdc, const RECT& rc, const char* text,
                           bool hovered, bool accented) {
    COLORREF bg  = hovered ? g_BtnHover : g_BtnNormal;
    COLORREF brd = accented ? g_AccentColor : g_BorderColor;

    HBRUSH hbr = CreateSolidBrush(bg);
    HPEN   pen = CreatePen(PS_SOLID, accented ? 2 : 1, brd);
    HGDIOBJ oldBr = SelectObject(hdc, hbr);
    HGDIOBJ oldPn = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPn);
    DeleteObject(hbr);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, accented ? g_AccentColor : g_TextPrimary);
    HFONT font = CreateAppFont(14, FW_SEMIBOLD);
    HGDIOBJ oldFont = SelectObject(hdc, font);
    RECT textRc = rc;
    DrawTextA(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// Accent-colored horizontal rule
static void DrawAccentLine(HDC hdc, int x, int y, int w) {
    HPEN pen = CreatePen(PS_SOLID, 2, g_AccentColor);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + w, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// Soft drop shadow beneath a rect
static void DrawSoftShadow(HDC hdc, const RECT& rc) {
    RECT shadow = { rc.left + 3, rc.top + 3, rc.right + 3, rc.bottom + 3 };
    FillFrosted(hdc, shadow, g_ShadowColor, 60);
}

// Inner glow — a subtle lighter rect inset by 2px for depth
static void DrawInnerGlow(HDC hdc, const RECT& rc) {
    RECT glow = { rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2 };
    FillFrosted(hdc, glow, g_GlowColor, 40);
}

// ============================================================================
//  CallAI — OpenRouter chat completion (unchanged core logic)
// ============================================================================

std::string CallAI(const std::string& question) {
    std::string key   = GetOpenRouterKey();
    std::string model = GetActiveModel();

    if (key.empty()) return "[ERROR] No OpenRouter API key set";

    HINTERNET hSession = WinHttpOpen(L"ZeroPoint/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "[ERROR] WinHttp failed";

    std::wstring hostW = L"openrouter.ai";
    std::wstring pathW = L"/api/v1/chat/completions";

    HINTERNET hConnect = WinHttpConnect(hSession, hostW.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", pathW.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    std::string jsonPayload =
        R"({"model":")" + model +
        R"(","messages":[{"role":"user","content":")" + question +
        R"("}],"max_tokens":800})";

    std::string headers =
        "Content-Type: application/json\r\nAuthorization: Bearer " + key + "\r\n";

    WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
                       (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(),
                       (DWORD)jsonPayload.length(), 0);
    WinHttpReceiveResponse(hRequest, NULL);

    char buffer[16384] = {0};
    DWORD bytesRead = 0;
    WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::string resp(buffer);
    return "[Using " + model + "]\n" + resp.substr(0, 400);
}

// ============================================================================
//  WebView2 Invisible Browser
// ============================================================================
// Real WebView2-based browser window that is:
//   - Hidden from screen recording via WDA_EXCLUDEFROMCAPTURE (Win10 2004+)
//   - Layered + transparent for stealth
//   - Full browsing capability (Google, YouTube, ChatGPT, etc.)
//   - Toggle on/off with Ctrl+Alt+B
//
// Requires: WebView2 Runtime + WebView2Loader.dll alongside the .exe

static HWND                         g_BrowserHwnd    = NULL;
static ComPtr<ICoreWebView2>        g_WebView        = nullptr;
static ComPtr<ICoreWebView2Controller> g_WebController = nullptr;
static bool                         g_BrowserVisible = false;

// Forward declarations
static LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void             InitWebView2(HWND hwnd);

static void CreateBrowserWindow() {
    if (g_BrowserHwnd && IsWindow(g_BrowserHwnd)) return; // already created

    const char* cls = "ZPInvisibleBrowser";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = BrowserProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
        RegisterClassA(&wc);
        registered = true;
    }

    int w = 1100, h = 750;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_BrowserHwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "ZeroPoint Browser",
        WS_POPUP,
        (sx - w) / 2, (sy - h) / 2, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    // Make the window translucent with the user's saved alpha
    SetLayeredWindowAttributes(g_BrowserHwnd, 0, g_WindowAlpha, LWA_ALPHA);

    // Hide from screen recording / capture (Windows 10 2004+)
    // WDA_EXCLUDEFROMCAPTURE = 0x00000011
    typedef BOOL(WINAPI* PFN_SetWindowDisplayAffinity)(HWND, DWORD);
    PFN_SetWindowDisplayAffinity pSetAffinity =
        (PFN_SetWindowDisplayAffinity)GetProcAddress(
            GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
    if (pSetAffinity) {
        pSetAffinity(g_BrowserHwnd, 0x00000011); // WDA_EXCLUDEFROMCAPTURE
    }

    // Initialize WebView2 inside this window
    InitWebView2(g_BrowserHwnd);
}

static void InitWebView2(HWND hwnd) {
    // Create WebView2 environment asynchronously, then create the controller
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;

                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) return result;

                            g_WebController = controller;
                            controller->get_CoreWebView2(&g_WebView);

                            // Fit WebView to the host window
                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            g_WebController->put_Bounds(bounds);

                            // Navigate to a default page
                            g_WebView->Navigate(L"https://www.google.com");

                            // Make WebView background transparent for frosted effect
                            ComPtr<ICoreWebView2Controller2> ctrl2;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2)))) {
                                COREWEBVIEW2_COLOR bgColor = { 0, 255, 255, 255 }; // transparent white
                                ctrl2->put_DefaultBackgroundColor(bgColor);
                            }

                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        MessageBoxA(hwnd,
            "WebView2 Runtime not found.\n\n"
            "Install the Edge WebView2 Runtime from:\n"
            "https://developer.microsoft.com/en-us/microsoft-edge/webview2/\n\n"
            "Falling back to placeholder mode.",
            "ZeroPoint - Browser", MB_OK | MB_ICONWARNING);
    }
}

static LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        if (g_WebController) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            g_WebController->put_Bounds(bounds);
        }
        return 0;
    }
    case WM_DESTROY:
        g_WebController = nullptr;
        g_WebView       = nullptr;
        g_BrowserHwnd   = NULL;
        g_BrowserVisible = false;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Toggle browser visibility — called from Ctrl+Alt+B hotkey
void OpenInvisibleBrowser() {
    if (g_BrowserVisible && g_BrowserHwnd && IsWindow(g_BrowserHwnd)) {
        // Hide the browser
        ShowWindow(g_BrowserHwnd, SW_HIDE);
        g_BrowserVisible = false;
    } else {
        // Create if needed, then show
        CreateBrowserWindow();
        if (g_BrowserHwnd) {
            ShowWindow(g_BrowserHwnd, SW_SHOW);
            SetForegroundWindow(g_BrowserHwnd);
            g_BrowserVisible = true;
        }
    }
}

// ============================================================================
//  Color Picker — accent color customization
// ============================================================================

static bool PickAccentColor(HWND owner) {
    static COLORREF customColors[16] = {
        RGB(0x00, 0xDD, 0xFF),  RGB(0x00, 0xFF, 0x88),
        RGB(0x88, 0xBB, 0xFF),  RGB(0xFF, 0x44, 0x88),
        RGB(0xFF, 0xAA, 0x00),  RGB(0x00, 0xFF, 0xCC),
        RGB(0x44, 0x88, 0xFF),  RGB(0xAA, 0x55, 0xFF),
        RGB(0xFF, 0xFF, 0xFF),  RGB(0xCC, 0xDD, 0xFF),
        RGB(0x00, 0xBB, 0xFF),  RGB(0x66, 0xFF, 0x66),
        RGB(0xFF, 0xDD, 0x00),  RGB(0xFF, 0x66, 0x00),
        RGB(0xDD, 0x00, 0xFF),  RGB(0x00, 0xFF, 0xFF),
    };

    CHOOSECOLORA cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.lpCustColors = customColors;
    cc.rgbResult    = g_AccentColor;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorA(&cc)) {
        g_AccentColor = cc.rgbResult;
        SaveThemeSettings();
        return true;
    }
    return false;
}

// ============================================================================
//  Alpha/Transparency Picker — slider dialog with live preview
// ============================================================================

static HWND g_hAlphaSlider = NULL;
static HWND g_hAlphaLabel  = NULL;
static BYTE g_TempAlpha    = 230;

static LRESULT CALLBACK AlphaDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_TempAlpha = g_WindowAlpha;

        CreateWindowA("STATIC", "Window Transparency",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            20, 15, 260, 20, hwnd, NULL, NULL, NULL);

        g_hAlphaSlider = CreateWindowA(TRACKBAR_CLASSA, NULL,
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_TOOLTIPS,
            30, 50, 240, 30, hwnd, (HMENU)2001, NULL, NULL);
        SendMessage(g_hAlphaSlider, TBM_SETRANGE, TRUE, MAKELPARAM(80, 255));
        SendMessage(g_hAlphaSlider, TBM_SETPOS, TRUE, g_WindowAlpha);

        char buf[32];
        snprintf(buf, sizeof(buf), "Opacity: %d / 255", g_WindowAlpha);
        g_hAlphaLabel = CreateWindowA("STATIC", buf,
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            30, 85, 240, 18, hwnd, NULL, NULL, NULL);

        CreateWindowA("BUTTON", "Apply",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            100, 115, 100, 30, hwnd, (HMENU)IDOK, NULL, NULL);

        return 0;
    }

    case WM_HSCROLL: {
        if ((HWND)lp == g_hAlphaSlider) {
            g_TempAlpha = (BYTE)SendMessage(g_hAlphaSlider, TBM_GETPOS, 0, 0);
            char buf[32];
            snprintf(buf, sizeof(buf), "Opacity: %d / 255", g_TempAlpha);
            SetWindowTextA(g_hAlphaLabel, buf);

            // Live preview on parent launcher
            HWND parent = GetParent(hwnd);
            if (parent) SetLayeredWindowAttributes(parent, 0, g_TempAlpha, LWA_ALPHA);
        }
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            g_WindowAlpha = g_TempAlpha;
            SaveThemeSettings();
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void ShowAlphaPicker(HWND owner) {
    const char* cls = "ZPAlphaPicker";
    WNDCLASSA wc = {};
    wc.lpfnWndProc   = AlphaDialogProc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = cls;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    RECT ownerRc;
    GetWindowRect(owner, &ownerRc);

    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "Transparency",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        ownerRc.left + 60, ownerRc.top + 80, 300, 160,
        owner, NULL, GetModuleHandle(NULL), NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsWindow(hwnd)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterClassA(cls, GetModuleHandle(NULL));
}

// ============================================================================
//  Launcher Window — icy frosted glass WNDPROC with full double-buffered paint
// ============================================================================

#define ID_BTN_INJECT   1001
#define ID_BTN_THEME    1002
#define ID_BTN_ALPHA    1003
#define ID_BTN_QUIT     1004
#define ID_COMBO_MODEL  1005

static int  g_LauncherResult = 0;
static int  g_HoveredBtn     = 0;
static RECT g_BtnInject, g_BtnTheme, g_BtnAlpha, g_BtnQuit;

static void EnableBlurBehind(HWND hwnd) {
    DWM_BLURBEHIND bb = {};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(bb.hRgnBlur);

    // Keep light mode for the icy look
    BOOL darkMode = FALSE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
}

static LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hCombo = NULL;

    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);

        hCombo = CreateWindowA("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            70, 218, 300, 200, hwnd, (HMENU)ID_COMBO_MODEL, NULL, NULL);

        for (int i = 0; i < (int)g_AvailableModels.size(); i++) {
            SendMessageA(hCombo, CB_ADDSTRING, 0,
                         (LPARAM)g_AvailableModels[i].c_str());
        }
        SendMessageA(hCombo, CB_SETCURSEL, g_ActiveModelIndex, 0);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wp) == ID_COMBO_MODEL && HIWORD(wp) == CBN_SELCHANGE) {
            int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) SetActiveModel(sel);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int w = clientRc.right;
        int h = clientRc.bottom;

        // ---- Double-buffer ----
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Snow-white base
        HBRUSH bgBrush = CreateSolidBrush(g_BgColor);
        FillRect(memDC, &clientRc, bgBrush);
        DeleteObject(bgBrush);

        // Frosted glass panel + shadow + glow
        RECT panelRc = { 16, 16, w - 16, h - 16 };
        DrawSoftShadow(memDC, panelRc);
        FillFrosted(memDC, panelRc, g_BgPanel, 220);
        DrawInnerGlow(memDC, panelRc);

        // Outer accent border
        HPEN borderPen = CreatePen(PS_SOLID, 1, g_AccentColor);
        SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, panelRc.left, panelRc.top,
                  panelRc.right, panelRc.bottom, 18, 18);
        DeleteObject(borderPen);

        // Inner subtle border
        HPEN innerPen = CreatePen(PS_SOLID, 1, g_BorderColor);
        SelectObject(memDC, innerPen);
        RoundRect(memDC, panelRc.left + 1, panelRc.top + 1,
                  panelRc.right - 1, panelRc.bottom - 1, 16, 16);
        DeleteObject(innerPen);

        SetBkMode(memDC, TRANSPARENT);

        // ---- Title ----
        HFONT titleFont = CreateAppFont(38, FW_LIGHT);
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, g_TextPrimary);
        RECT titleRc = { 30, 28, w - 30, 76 };
        DrawTextA(memDC, "ZeroPoint", -1, &titleRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(titleFont);

        // Accent underline
        DrawAccentLine(memDC, 70, 78, w - 140);

        // ---- Subtitle ----
        HFONT subFont = CreateAppFont(13, FW_NORMAL);
        SelectObject(memDC, subFont);
        SetTextColor(memDC, g_TextSecondary);
        RECT subRc = { 30, 86, w - 30, 104 };
        DrawTextA(memDC, "Premium AI-Powered Utility", -1, &subRc,
                  DT_CENTER | DT_SINGLELINE);
        DeleteObject(subFont);

        // ---- Hotkeys ----
        HFONT hotkeyFont = CreateAppFont(12, FW_NORMAL);
        SelectObject(memDC, hotkeyFont);

        const struct { const char* key; const char* desc; } hotkeys[] = {
            { "Ctrl+Shift+Z", "AI Snapshot" },
            { "Ctrl+Alt+H",   "Full Menu" },
            { "Ctrl+Alt+B",   "Invisible Browser" },
            { "Ctrl+Shift+X", "Panic Killswitch" },
        };

        int hotkeyY = 118;
        for (int i = 0; i < 4; i++) {
            SetTextColor(memDC, g_AccentColor);
            RECT keyRc = { 70, hotkeyY, 200, hotkeyY + 16 };
            DrawTextA(memDC, hotkeys[i].key, -1, &keyRc, DT_LEFT | DT_SINGLELINE);

            SetTextColor(memDC, g_TextSecondary);
            RECT descRc = { 210, hotkeyY, w - 40, hotkeyY + 16 };
            DrawTextA(memDC, hotkeys[i].desc, -1, &descRc, DT_LEFT | DT_SINGLELINE);

            hotkeyY += 20;
        }
        DeleteObject(hotkeyFont);

        // ---- Model selector label ----
        HFONT labelFont = CreateAppFont(11, FW_SEMIBOLD);
        SelectObject(memDC, labelFont);
        SetTextColor(memDC, g_AccentColor);
        RECT modelLabelRc = { 70, 203, w - 70, 218 };
        DrawTextA(memDC, "SELECT MODEL", -1, &modelLabelRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(labelFont);

        // Accent divider
        DrawAccentLine(memDC, 70, 252, w - 140);

        // ---- Buttons ----
        g_BtnInject = { 40, 268, 215, 306 };
        g_BtnTheme  = { 225, 268, 400, 306 };
        g_BtnAlpha  = { 40, 316, 215, 354 };
        g_BtnQuit   = { 225, 316, 400, 354 };

        DrawIcyButton(memDC, g_BtnInject, "INJECT",
                      g_HoveredBtn == ID_BTN_INJECT, true);
        DrawIcyButton(memDC, g_BtnTheme, "ACCENT COLOR",
                      g_HoveredBtn == ID_BTN_THEME, false);
        DrawIcyButton(memDC, g_BtnAlpha, "TRANSPARENCY",
                      g_HoveredBtn == ID_BTN_ALPHA, false);
        DrawIcyButton(memDC, g_BtnQuit, "QUIT",
                      g_HoveredBtn == ID_BTN_QUIT, false);

        // ---- Accent swatch dot ----
        HBRUSH swatchBrush = CreateSolidBrush(g_AccentColor);
        HPEN swatchPen = CreatePen(PS_SOLID, 1, g_BorderColor);
        SelectObject(memDC, swatchBrush);
        SelectObject(memDC, swatchPen);
        Ellipse(memDC, w - 56, 34, w - 36, 54);
        DeleteObject(swatchBrush);
        DeleteObject(swatchPen);

        // ---- Footer ----
        HFONT footerFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, footerFont);
        SetTextColor(memDC, g_ShadowColor);
        RECT footerRc = { 30, h - 36, w - 30, h - 20 };
        char footerBuf[64];
        snprintf(footerBuf, sizeof(footerBuf), "opacity %d/255", g_WindowAlpha);
        DrawTextA(memDC, footerBuf, -1, &footerRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(footerFont);

        // ---- Blit ----
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = LOWORD(lp), my = HIWORD(lp);
        int prev = g_HoveredBtn;
        g_HoveredBtn = 0;

        POINT pt = { mx, my };
        if (PtInRect(&g_BtnInject, pt))      g_HoveredBtn = ID_BTN_INJECT;
        else if (PtInRect(&g_BtnTheme, pt))  g_HoveredBtn = ID_BTN_THEME;
        else if (PtInRect(&g_BtnAlpha, pt))  g_HoveredBtn = ID_BTN_ALPHA;
        else if (PtInRect(&g_BtnQuit, pt))   g_HoveredBtn = ID_BTN_QUIT;

        if (prev != g_HoveredBtn) InvalidateRect(hwnd, NULL, FALSE);

        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_HoveredBtn != 0) {
            g_HoveredBtn = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        POINT pt = { LOWORD(lp), HIWORD(lp) };

        if (PtInRect(&g_BtnInject, pt)) {
            g_LauncherResult = 1;
            DestroyWindow(hwnd);
        } else if (PtInRect(&g_BtnTheme, pt)) {
            PickAccentColor(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (PtInRect(&g_BtnAlpha, pt)) {
            ShowAlphaPicker(hwnd);
            SetLayeredWindowAttributes(hwnd, 0, g_WindowAlpha, LWA_ALPHA);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (PtInRect(&g_BtnQuit, pt)) {
            g_LauncherResult = 0;
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            POINT pt;
            pt.x = LOWORD(lp); pt.y = HIWORD(lp);
            ScreenToClient(hwnd, &pt);
            // Don't intercept clicks on the combo box region
            if (pt.y >= 210 && pt.y <= 245 && pt.x >= 60 && pt.x <= 380)
                return HTCLIENT;
            return HTCAPTION; // drag the window from anywhere else
        }
        return hit;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

static int ShowLauncher() {
    const char* className = "ZeroPointLauncher";

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = LauncherProc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = className;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = NULL;
    RegisterClassA(&wc);

    int winW = 440, winH = 400;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        className, "ZeroPoint",
        WS_POPUP | WS_VISIBLE,
        (screenW - winW) / 2, (screenH - winH) / 2, winW, winH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(hwnd, 0, g_WindowAlpha, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA(className, GetModuleHandle(NULL));
    return g_LauncherResult;
}

// ============================================================================
//  Enhanced Frosted AI Answer Popup — icy/snowy theme
// ============================================================================
// Premium bottom-right toast: bright white frosted glass, soft shadow,
// inner glow, accent header bar, auto-dismiss, click to close.

static std::string g_PopupText;

static LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // Enable blur-behind for true frosted glass on supported systems
        DWM_BLURBEHIND bb = {};
        bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable  = TRUE;
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(bb.hRgnBlur);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        // ---- Double-buffer ----
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Snow-white base fill
        HBRUSH bgBrush = CreateSolidBrush(g_BgColor);
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Frosted glass overlay with inner glow
        FillFrosted(memDC, rc, g_BgPanel, 230);
        DrawInnerGlow(memDC, rc);

        // Soft shadow at the bottom edge (simulates floating)
        RECT shadowRc = { 4, h - 6, w - 4, h };
        FillFrosted(memDC, shadowRc, g_ShadowColor, 50);

        // ---- Accent header bar ----
        // A thin frosted accent strip at the top
        RECT accentBar = { 0, 0, w, 4 };
        HBRUSH accentBrush = CreateSolidBrush(g_AccentColor);
        FillRect(memDC, &accentBar, accentBrush);
        DeleteObject(accentBrush);

        // Outer border — accent color, rounded
        HPEN borderPen = CreatePen(PS_SOLID, 2, g_AccentColor);
        SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 0, 0, w, h, 16, 16);
        DeleteObject(borderPen);

        // Inner border — subtle icy edge
        HPEN innerPen = CreatePen(PS_SOLID, 1, g_BorderColor);
        SelectObject(memDC, innerPen);
        RoundRect(memDC, 1, 1, w - 1, h - 1, 14, 14);
        DeleteObject(innerPen);

        SetBkMode(memDC, TRANSPARENT);

        // ---- Title "ZeroPoint AI" ----
        HFONT titleFont = CreateAppFont(15, FW_BOLD);
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, g_AccentColor);
        RECT titleRc = { 16, 12, w - 16, 32 };
        DrawTextA(memDC, "ZeroPoint AI", -1, &titleRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(titleFont);

        // Model tag (small, right-aligned)
        HFONT tagFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, tagFont);
        SetTextColor(memDC, g_TextSecondary);
        RECT tagRc = { w - 180, 14, w - 16, 30 };
        std::string modelTag = GetActiveModel();
        DrawTextA(memDC, modelTag.c_str(), -1, &tagRc, DT_RIGHT | DT_SINGLELINE);
        DeleteObject(tagFont);

        // Accent separator
        DrawAccentLine(memDC, 16, 36, w - 32);

        // ---- Close hint ----
        HFONT hintFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, hintFont);
        SetTextColor(memDC, g_ShadowColor);
        RECT hintRc = { 16, h - 22, w - 16, h - 6 };
        DrawTextA(memDC, "click anywhere to dismiss", -1, &hintRc,
                  DT_CENTER | DT_SINGLELINE);
        DeleteObject(hintFont);

        // ---- Answer text ----
        HFONT textFont = CreateAppFont(13, FW_NORMAL);
        SelectObject(memDC, textFont);
        SetTextColor(memDC, g_TextPrimary);
        RECT textRc = { 16, 44, w - 16, h - 26 };
        DrawTextA(memDC, g_PopupText.c_str(), -1, &textRc,
                  DT_LEFT | DT_WORDBREAK);
        DeleteObject(textFont);

        // ---- Blit to screen ----
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP:
        DestroyWindow(hwnd);
        return 0;

    case WM_TIMER:
        // Fade out could be added here; for now, just dismiss
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void ShowAIPopup(const std::string& text) {
    g_PopupText = text;

    const char* cls = "ZeroPointPopup";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = PopupProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
        registered = true;
    }

    int w = 440, h = 300;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP | WS_VISIBLE,
        sx - w - 24, sy - h - 60, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    // Use the user's saved transparency
    SetLayeredWindowAttributes(hwnd, 0, g_WindowAlpha, LWA_ALPHA);

    // Auto-dismiss after 15 seconds
    SetTimer(hwnd, 1, 15000, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// ============================================================================
//  Entry Point
// ============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Initialize common controls (combo box + trackbar)
    INITCOMMONCONTROLSEX icc = { sizeof(icc),
        ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Initialize COM for WebView2
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    LoadConfig();
    LoadThemeSettings();

    // ------------------------------------------------------------------
    //  First-launch: prompt for OpenRouter API key
    // ------------------------------------------------------------------
    if (GetOpenRouterKey().empty()) {
        int result = MessageBoxA(NULL,
            "ZeroPoint\n\n"
            "No OpenRouter API key detected.\n\n"
            "Please set your API key in:\n"
            "C:\\ProgramData\\ZeroPoint\\config.ini\n\n"
            "Then relaunch ZeroPoint.",
            "ZeroPoint - First Launch",
            MB_OKCANCEL | MB_ICONINFORMATION);
        if (result == IDCANCEL) {
            CoUninitialize();
            return 0;
        }
    }

    // ------------------------------------------------------------------
    //  Show premium icy launcher
    // ------------------------------------------------------------------
    int action = ShowLauncher();

    if (action == 0) {
        CoUninitialize();
        return 0;
    }

    // ------------------------------------------------------------------
    //  Inject
    // ------------------------------------------------------------------
    if (!PerformHollowing()) {
        MessageBoxA(NULL, "Injection failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // ------------------------------------------------------------------
    //  Background hotkey loop (unchanged core logic)
    // ------------------------------------------------------------------
    bool keyZ = false, keyH = false, keyB = false, keyX = false;

    while (true) {
        // Process pending messages (needed for WebView2 async callbacks)
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;

        // Ctrl+Shift+Z — AI Snapshot (frosted bottom-right popup)
        if (ctrl && shift && (GetAsyncKeyState(0x5A) & 0x8000)) {
            if (!keyZ) {
                keyZ = true;
                std::string question = ExtractBluebookDOM();
                std::string answer   = CallAI(question);
                ShowAIPopup(answer);
            }
        } else {
            keyZ = false;
        }

        // Ctrl+Alt+H — Full menu (reserved for future)
        if (ctrl && alt && (GetAsyncKeyState(0x48) & 0x8000)) {
            if (!keyH) {
                keyH = true;
                // ToggleFullMenu();
            }
        } else {
            keyH = false;
        }

        // Ctrl+Alt+B — Invisible WebView2 browser (toggle)
        if (ctrl && alt && (GetAsyncKeyState(0x42) & 0x8000)) {
            if (!keyB) {
                keyB = true;
                OpenInvisibleBrowser();
            }
        } else {
            keyB = false;
        }

        // Ctrl+Shift+X — Panic killswitch
        if (ctrl && shift && (GetAsyncKeyState(0x58) & 0x8000)) {
            if (!keyX) {
                keyX = true;
                // Destroy browser window if open
                if (g_BrowserHwnd && IsWindow(g_BrowserHwnd)) {
                    DestroyWindow(g_BrowserHwnd);
                }
                PanicKillAndWipe();
                break;
            }
        } else {
            keyX = false;
        }

        Sleep(10);
    }

    CoUninitialize();
    return 0;
}
