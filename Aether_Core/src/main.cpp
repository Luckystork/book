// ============================================================================
//  ZeroPoint — Premium Windows Utility
//  main.cpp — Frosted glass launcher, custom theme, AI hotkeys
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

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

// ============================================================================
//  Theme / Color Customization
// ============================================================================
// Accent color is saved to config.ini as "accent=#RRGGBB".
// Default accent: icy cyan #00DDFF.

static COLORREF g_AccentColor   = RGB(0x00, 0xDD, 0xFF);   // icy cyan
static COLORREF g_BgColor       = RGB(0x0C, 0x10, 0x1E);   // deep navy
static COLORREF g_BgLight       = RGB(0x14, 0x1A, 0x2E);   // panel bg
static COLORREF g_TextColor     = RGB(0xE8, 0xF0, 0xFF);   // icy white
static COLORREF g_TextDim       = RGB(0x70, 0x80, 0xA0);   // dimmed text
static COLORREF g_BtnHoverBg    = RGB(0x1C, 0x24, 0x3A);   // button hover

static const char* THEME_CONFIG = "C:\\ProgramData\\ZeroPoint\\config.ini";

// ---------------------------------------------------------------------------
//  Persist / restore accent color from config.ini
// ---------------------------------------------------------------------------
static void LoadAccentColor() {
    std::ifstream f(THEME_CONFIG);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("accent=", 0) == 0 && line.size() >= 14) {
            // Parse accent=#RRGGBB
            unsigned int r = 0, g = 0, b = 0;
            if (sscanf(line.c_str() + 8, "%02x%02x%02x", &r, &g, &b) == 3) {
                g_AccentColor = RGB(r, g, b);
            }
        }
    }
}

static void SaveAccentColor() {
    // Read existing config, replace or append accent line
    std::vector<std::string> lines;
    {
        std::ifstream f(THEME_CONFIG);
        std::string line;
        bool found = false;
        while (std::getline(f, line)) {
            if (line.rfind("accent=", 0) == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "accent=#%02X%02X%02X",
                         GetRValue(g_AccentColor),
                         GetGValue(g_AccentColor),
                         GetBValue(g_AccentColor));
                lines.push_back(buf);
                found = true;
            } else {
                lines.push_back(line);
            }
        }
        if (!found) {
            char buf[32];
            snprintf(buf, sizeof(buf), "accent=#%02X%02X%02X",
                     GetRValue(g_AccentColor),
                     GetGValue(g_AccentColor),
                     GetBValue(g_AccentColor));
            lines.push_back(buf);
        }
    }
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    std::ofstream out(THEME_CONFIG);
    for (auto& l : lines) out << l << "\n";
}

// ============================================================================
//  GDI helpers — frosted glass drawing primitives
// ============================================================================

static HFONT CreateAppFont(int size, int weight = FW_NORMAL) {
    return CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

static HBRUSH MakeSolidBrush(COLORREF c) {
    return CreateSolidBrush(c);
}

// Fill a rect with a translucent overlay to simulate frosted glass
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

    // Fill with premultiplied alpha color
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

// Draw rounded-rect button with accent border
static void DrawIcyButton(HDC hdc, const RECT& rc, const char* text,
                           bool hovered, bool accent) {
    COLORREF bg  = hovered ? g_BtnHoverBg : g_BgLight;
    COLORREF brd = accent  ? g_AccentColor : RGB(0x30, 0x3A, 0x50);

    HBRUSH hbr = MakeSolidBrush(bg);
    HPEN   pen = CreatePen(PS_SOLID, 1, brd);
    SelectObject(hdc, hbr);
    SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    DeleteObject(hbr);
    DeleteObject(pen);

    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, accent ? g_AccentColor : g_TextColor);
    HFONT font = CreateAppFont(15, FW_SEMIBOLD);
    HGDIOBJ oldFont = SelectObject(hdc, font);
    RECT textRc = rc;
    DrawTextA(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// Draw a horizontal accent line
static void DrawAccentLine(HDC hdc, int x, int y, int w) {
    HPEN pen = CreatePen(PS_SOLID, 2, g_AccentColor);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + w, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// ============================================================================
//  CallAI — OpenRouter chat completion
// ============================================================================

std::string CallAI(const std::string& question) {
    std::string key   = GetOpenRouterKey();
    std::string model = GetActiveModel();

    if (key.empty()) return "[ERROR] No OpenRouter API key set";

    HINTERNET hSession = WinHttpOpen(L"ZeroPoint/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "[ERROR] WinHttp session failed";

    HINTERNET hConnect = WinHttpConnect(hSession,
        L"openrouter.ai", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        L"/api/v1/chat/completions", NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    // Escape basic JSON chars in the question
    std::string escaped;
    for (char c : question) {
        if (c == '"')       escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else                escaped += c;
    }

    std::string jsonPayload =
        R"({"model":")" + model +
        R"(","messages":[{"role":"user","content":")" + escaped +
        R"("}],"max_tokens":800})";

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer ";
    headers += std::wstring(key.begin(), key.end());
    headers += L"\r\n";

    WinHttpSendRequest(hRequest,
        headers.c_str(), (DWORD)headers.length(),
        (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(),
        (DWORD)jsonPayload.length(), 0);
    WinHttpReceiveResponse(hRequest, NULL);

    std::string resp;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        resp += buffer;
        bytesRead = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Extract "content" from the response JSON
    size_t pos = resp.find("\"content\"");
    if (pos != std::string::npos) {
        // Look for the string value after "content":"
        size_t start = resp.find(":\"", pos);
        if (start != std::string::npos) {
            start += 2;
            // Find closing quote (skip escaped quotes)
            size_t end = start;
            while (end < resp.size()) {
                if (resp[end] == '"' && resp[end - 1] != '\\') break;
                end++;
            }
            return "[" + model + "]\n" + resp.substr(start, end - start);
        }
    }
    return "[" + model + "]\n" + resp.substr(0, 400);
}

// ============================================================================
//  Invisible Browser placeholder
// ============================================================================

void OpenInvisibleBrowser() {
    // Placeholder — real WebView2 integration can be added later
    MessageBoxA(NULL,
        "Invisible Browser Activated\n\n"
        "Browse any website invisibly.\n"
        "Press Ctrl+Alt+B again to close.",
        "ZeroPoint", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
//  Custom Color Picker Dialog
// ============================================================================
// Uses the Windows CHOOSECOLOR common dialog, then saves to config.ini.

static bool PickAccentColor(HWND owner) {
    static COLORREF customColors[16] = {
        RGB(0x00, 0xDD, 0xFF),  // icy cyan (default)
        RGB(0x00, 0xFF, 0x88),  // mint
        RGB(0x88, 0x00, 0xFF),  // purple
        RGB(0xFF, 0x44, 0x88),  // pink
        RGB(0xFF, 0xAA, 0x00),  // amber
        RGB(0x00, 0xFF, 0xCC),  // teal
        RGB(0x44, 0x88, 0xFF),  // blue
        RGB(0xFF, 0x00, 0x44),  // red
        RGB(0xFF, 0xFF, 0xFF),  // white
        RGB(0xCC, 0xDD, 0xFF),  // ice
        RGB(0x00, 0xBB, 0xFF),  // sky
        RGB(0x66, 0xFF, 0x66),  // lime
        RGB(0xFF, 0xDD, 0x00),  // gold
        RGB(0xFF, 0x66, 0x00),  // orange
        RGB(0xDD, 0x00, 0xFF),  // magenta
        RGB(0x00, 0xFF, 0xFF),  // aqua
    };

    CHOOSECOLORA cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.lpCustColors = customColors;
    cc.rgbResult    = g_AccentColor;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorA(&cc)) {
        g_AccentColor = cc.rgbResult;
        SaveAccentColor();
        return true;
    }
    return false;
}

// ============================================================================
//  Launcher Window — frosted glass look
// ============================================================================

// Control IDs
#define ID_BTN_INJECT   1001
#define ID_BTN_MODEL    1002
#define ID_BTN_THEME    1003
#define ID_BTN_QUIT     1004
#define ID_COMBO_MODEL  1005

static int  g_LauncherResult = 0;  // 0 = quit, 1 = inject, 2 = model changed
static int  g_HoveredBtn     = 0;
static bool g_LauncherDone   = false;

// Enable DWM blur-behind for frosted glass effect
static void EnableBlurBehind(HWND hwnd) {
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1); // whole window
    DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(bb.hRgnBlur);

    // On Windows 10+, try to set dark mode and mica/acrylic if available
    BOOL darkMode = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
}

static LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hCombo = NULL;

    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);

        // Model selector combo box (custom positioned)
        hCombo = CreateWindowA("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            60, 200, 320, 200, hwnd, (HMENU)ID_COMBO_MODEL, NULL, NULL);

        for (int i = 0; i < (int)g_AvailableModels.size(); i++) {
            SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)g_AvailableModels[i].c_str());
        }
        SendMessageA(hCombo, CB_SETCURSEL, g_ActiveModelIndex, 0);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_COMBO_MODEL && HIWORD(wp) == CBN_SELCHANGE) {
            int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) SetActiveModel(sel);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // we paint everything ourselves

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int w = clientRc.right;
        int h = clientRc.bottom;

        // Double-buffer
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Background — deep navy
        HBRUSH bgBrush = MakeSolidBrush(g_BgColor);
        FillRect(memDC, &clientRc, bgBrush);
        DeleteObject(bgBrush);

        // Frosted panel overlay
        RECT panelRc = { 20, 20, w - 20, h - 20 };
        FillFrosted(memDC, panelRc, g_BgLight, 200);

        // Title: "ZeroPoint"
        SetBkMode(memDC, TRANSPARENT);
        HFONT titleFont = CreateAppFont(36, FW_BOLD);
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, g_TextColor);
        RECT titleRc = { 40, 30, w - 40, 80 };
        DrawTextA(memDC, "ZeroPoint", -1, &titleRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(titleFont);

        // Accent line under title
        DrawAccentLine(memDC, 60, 82, w - 120);

        // Subtitle
        HFONT subFont = CreateAppFont(14, FW_NORMAL);
        SelectObject(memDC, subFont);
        SetTextColor(memDC, g_TextDim);
        RECT subRc = { 40, 90, w - 40, 110 };
        DrawTextA(memDC, "Premium AI-Powered Utility", -1, &subRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(subFont);

        // Hotkey info
        HFONT infoFont = CreateAppFont(13, FW_NORMAL);
        SelectObject(memDC, infoFont);
        SetTextColor(memDC, g_TextDim);

        const char* hotkeyLines[] = {
            "Ctrl+Shift+Z    AI Snapshot",
            "Ctrl+Alt+H      Full Menu",
            "Ctrl+Alt+B      Invisible Browser",
            "Ctrl+Shift+X    Panic Killswitch",
        };
        for (int i = 0; i < 4; i++) {
            RECT lr = { 60, 125 + i * 18, w - 60, 143 + i * 18 };
            DrawTextA(memDC, hotkeyLines[i], -1, &lr, DT_LEFT | DT_SINGLELINE);
        }
        DeleteObject(infoFont);

        // Active model label
        HFONT labelFont = CreateAppFont(12, FW_NORMAL);
        SelectObject(memDC, labelFont);
        SetTextColor(memDC, g_AccentColor);
        RECT modelLabel = { 60, 205, w - 60, 220 };
        std::string mlText = "Model: " + GetActiveModel();
        // Draw label above combo (combo is at y=200)
        RECT modelLabelRc = { 60, 185, w - 60, 200 };
        DrawTextA(memDC, "Select Model:", -1, &modelLabelRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(labelFont);

        // Accent line before buttons
        DrawAccentLine(memDC, 60, 240, w - 120);

        // Buttons
        RECT btnInject = { 60, 258, 220, 295 };
        RECT btnTheme  = { 230, 258, 380, 295 };
        RECT btnQuit   = { 130, 308, 310, 340 };

        DrawIcyButton(memDC, btnInject, "INJECT", g_HoveredBtn == ID_BTN_INJECT, true);
        DrawIcyButton(memDC, btnTheme,  "CUSTOMIZE THEME", g_HoveredBtn == ID_BTN_THEME, false);
        DrawIcyButton(memDC, btnQuit,   "QUIT", g_HoveredBtn == ID_BTN_QUIT, false);

        // Frost border around the whole panel
        HPEN borderPen = CreatePen(PS_SOLID, 1, g_AccentColor);
        SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, panelRc.left, panelRc.top, panelRc.right, panelRc.bottom, 16, 16);
        DeleteObject(borderPen);

        // Blit
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

        RECT btnInject = { 60, 258, 220, 295 };
        RECT btnTheme  = { 230, 258, 380, 295 };
        RECT btnQuit   = { 130, 308, 310, 340 };
        POINT pt = { mx, my };

        if (PtInRect(&btnInject, pt)) g_HoveredBtn = ID_BTN_INJECT;
        else if (PtInRect(&btnTheme, pt))  g_HoveredBtn = ID_BTN_THEME;
        else if (PtInRect(&btnQuit, pt))   g_HoveredBtn = ID_BTN_QUIT;

        if (prev != g_HoveredBtn) InvalidateRect(hwnd, NULL, FALSE);

        // Track mouse leave
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
        int mx = LOWORD(lp), my = HIWORD(lp);
        POINT pt = { mx, my };

        RECT btnInject = { 60, 258, 220, 295 };
        RECT btnTheme  = { 230, 258, 380, 295 };
        RECT btnQuit   = { 130, 308, 310, 340 };

        if (PtInRect(&btnInject, pt)) {
            g_LauncherResult = 1;
            g_LauncherDone = true;
            DestroyWindow(hwnd);
        } else if (PtInRect(&btnTheme, pt)) {
            PickAccentColor(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (PtInRect(&btnQuit, pt)) {
            g_LauncherResult = 0;
            g_LauncherDone = true;
            DestroyWindow(hwnd);
        }
        return 0;
    }

    // Allow dragging the window by clicking anywhere on the client area
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) return HTCAPTION;
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

    int winW = 440, winH = 380;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        className, "ZeroPoint",
        WS_POPUP | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    // Set layered window opacity for subtle transparency
    SetLayeredWindowAttributes(hwnd, 0, 240, LWA_ALPHA);

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
//  Frosted AI Answer Popup (bottom-right toast)
// ============================================================================

static std::string g_PopupText;

static LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill background
        HBRUSH bg = MakeSolidBrush(g_BgColor);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // Frosted overlay
        FillFrosted(hdc, rc, g_BgLight, 210);

        // Accent border
        HPEN pen = CreatePen(PS_SOLID, 2, g_AccentColor);
        SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, rc.right, rc.bottom, 12, 12);
        DeleteObject(pen);

        // Title bar
        SetBkMode(hdc, TRANSPARENT);
        HFONT titleFont = CreateAppFont(13, FW_BOLD);
        SelectObject(hdc, titleFont);
        SetTextColor(hdc, g_AccentColor);
        RECT titleRc = { 12, 8, rc.right - 12, 26 };
        DrawTextA(hdc, "ZeroPoint AI", -1, &titleRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(titleFont);

        // Answer text
        HFONT textFont = CreateAppFont(13, FW_NORMAL);
        SelectObject(hdc, textFont);
        SetTextColor(hdc, g_TextColor);
        RECT textRc = { 12, 30, rc.right - 12, rc.bottom - 8 };
        DrawTextA(hdc, g_PopupText.c_str(), -1, &textRc, DT_LEFT | DT_WORDBREAK);
        DeleteObject(textFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP:
        DestroyWindow(hwnd);
        return 0;

    case WM_TIMER:
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
    WNDCLASSA wc = {};
    wc.lpfnWndProc   = PopupProc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = cls;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int w = 420, h = 260;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP | WS_VISIBLE,
        sx - w - 20, sy - h - 60, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(hwnd, 0, 235, LWA_ALPHA);
    SetTimer(hwnd, 1, 12000, NULL); // auto-dismiss after 12s

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterClassA(cls, GetModuleHandle(NULL));
}

// ============================================================================
//  Entry Point
// ============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Initialize common controls for the combo box
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    LoadConfig();
    LoadAccentColor();

    // ------------------------------------------------------------------
    //  First-launch: prompt for OpenRouter API key
    // ------------------------------------------------------------------
    if (GetOpenRouterKey().empty()) {
        // Simple input dialog for the API key
        char keyBuf[256] = {};
        // For now, use a basic prompt — can be upgraded to a custom dialog
        int result = MessageBoxA(NULL,
            "ZeroPoint\n\n"
            "No OpenRouter API key detected.\n\n"
            "Please set your API key in:\n"
            "C:\\ProgramData\\ZeroPoint\\config.ini\n\n"
            "Then relaunch ZeroPoint.",
            "ZeroPoint - First Launch",
            MB_OKCANCEL | MB_ICONINFORMATION);
        if (result == IDCANCEL) return 0;
    }

    // ------------------------------------------------------------------
    //  Show Launcher
    // ------------------------------------------------------------------
    int action = ShowLauncher();

    if (action == 0) {
        // User chose Quit
        return 0;
    }

    // action == 1: Inject
    if (!PerformHollowing()) {
        MessageBoxA(NULL, "Injection failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ------------------------------------------------------------------
    //  Hotkey loop — runs in background after injection
    // ------------------------------------------------------------------
    bool keyZ = false, keyH = false, keyB = false, keyX = false;

    while (true) {
        bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;

        // Ctrl+Shift+Z — AI Snapshot
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

        // Ctrl+Alt+H — Full menu (placeholder)
        if (ctrl && alt && (GetAsyncKeyState(0x48) & 0x8000)) {
            if (!keyH) {
                keyH = true;
                // ToggleFullMenu(); — reserved for future implementation
            }
        } else {
            keyH = false;
        }

        // Ctrl+Alt+B — Invisible browser
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
                PanicKillAndWipe();
                break;
            }
        } else {
            keyX = false;
        }

        Sleep(10);
    }

    return 0;
}
