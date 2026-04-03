// ============================================================================
//  ZeroPoint — Premium Windows Utility
//  main.cpp — Frosted glass launcher, icy/snowy theme, WebView2 invisible
//             browser, enhanced AI popup, full menu overlay, custom accent
//             color + transparency, inline API key dialog
// ============================================================================
//
//  BUILD REQUIREMENTS:
//    - Microsoft.Web.WebView2 NuGet package (WebView2Loader.dll + headers)
//    - Windows 10 1809+ with Edge WebView2 Runtime installed
//    - Link: winhttp, dwmapi, comctl32, gdi32, msimg32, WebView2Loader
//    - Include path must contain the Aether_Core/include directory
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
        // Format: accent=#RRGGBB  (7 chars for "accent=", then '#', then 6 hex)
        if (line.rfind("accent=", 0) == 0 && line.size() >= 14) {
            unsigned int r = 0, g = 0, b = 0;
            // +8 skips "accent=#", then parse 6 hex digits
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
//  JSON Escape Helper — prevents malformed payloads from DOM text
// ============================================================================

static std::string EscapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", (unsigned)c);
                    out += hex;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Helper: extract "content":"..." from OpenAI-compatible JSON response
// Works for OpenRouter, OpenAI, Grok (choices[0].message.content)
static std::string ParseAIContent(const std::string& json) {
    size_t pos = json.find("\"content\":\"");
    if (pos == std::string::npos) {
        pos = json.find("\"content\": \"");
        if (pos != std::string::npos) pos += 12;
        else return json.substr(0, 500);
    } else {
        pos += 11;
    }
    std::string result;
    for (size_t i = pos; i < json.size(); i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            if (next == '"')       { result += '"'; i++; }
            else if (next == 'n')  { result += '\n'; i++; }
            else if (next == 'r')  { result += '\r'; i++; }
            else if (next == 't')  { result += '\t'; i++; }
            else if (next == '\\') { result += '\\'; i++; }
            else { result += json[i]; }
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

// Helper: extract "text":"..." from Anthropic Messages API response
// Response format: {"content":[{"type":"text","text":"..."}]}
static std::string ParseAnthropicContent(const std::string& json) {
    size_t pos = json.find("\"text\":\"");
    if (pos == std::string::npos) {
        pos = json.find("\"text\": \"");
        if (pos != std::string::npos) pos += 9;
        else return json.substr(0, 500);
    } else {
        pos += 8;
    }
    std::string result;
    for (size_t i = pos; i < json.size(); i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            if (next == '"')       { result += '"'; i++; }
            else if (next == 'n')  { result += '\n'; i++; }
            else if (next == '\\') { result += '\\'; i++; }
            else { result += json[i]; }
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

// Helper: extract text from Gemini generateContent response
// Response: {"candidates":[{"content":{"parts":[{"text":"..."}]}}]}
static std::string ParseGeminiContent(const std::string& json) {
    return ParseAnthropicContent(json);  // same "text":"..." pattern
}

// ============================================================================
//  CallAI — Multi-Provider chat completion
// ============================================================================
//  Routes to the correct API based on GetActiveProvider():
//    - OpenRouter, OpenAI, Grok  → OpenAI-compatible /v1/chat/completions
//    - Anthropic                 → Messages API /v1/messages
//    - Gemini                    → generateContent REST API
//
//  Each provider has different: host, path, headers, body, response format.

// Internal helper: send an HTTPS POST and return the response body
static std::string HttpsPost(const std::wstring& host, const std::wstring& path,
                             const std::wstring& headers, const std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"ZeroPoint/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "[ERROR] WinHttp session failed";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    BOOL ok = WinHttpSendRequest(hRequest,
        headers.c_str(), (DWORD)headers.length(),
        (LPVOID)body.c_str(), (DWORD)body.length(),
        (DWORD)body.length(), 0);

    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "[ERROR] HTTP request failed (check network)";
    }

    WinHttpReceiveResponse(hRequest, NULL);

    std::string resp;
    char buffer[8192];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        resp += buffer;
        bytesRead = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

std::string CallAI(const std::string& question) {
    Provider prov     = GetActiveProvider();
    std::string key   = GetProviderKey(prov);
    std::string model = GetActiveModelID();
    std::string escaped = EscapeJsonString(question);

    if (key.empty()) {
        return "[ERROR] No API key set for " + GetActiveProviderName() +
               ". Open settings to add your key.";
    }

    std::wstring wKey(key.begin(), key.end());
    std::string resp;

    switch (prov) {
    // -----------------------------------------------------------------
    //  OpenAI-compatible: Grok, GPT, Deepseek, OpenRouter
    //  All use /v1/chat/completions with Bearer token
    // -----------------------------------------------------------------
    case PROV_GROK:
    case PROV_GPT:
    case PROV_DEEPSEEK:
    case PROV_OPENROUTER: {
        std::wstring host, path;
        if (prov == PROV_OPENROUTER) {
            host = L"openrouter.ai";
            path = L"/api/v1/chat/completions";
        } else if (prov == PROV_GPT) {
            host = L"api.openai.com";
            path = L"/v1/chat/completions";
        } else if (prov == PROV_GROK) {
            host = L"api.x.ai";
            path = L"/v1/chat/completions";
        } else {  // PROV_DEEPSEEK
            host = L"api.deepseek.com";
            path = L"/v1/chat/completions";
        }

        std::string body =
            R"({"model":")" + model +
            R"(","messages":[{"role":"user","content":")" + escaped +
            R"("}],"max_tokens":1200})";

        std::wstring headers =
            L"Content-Type: application/json\r\nAuthorization: Bearer " + wKey + L"\r\n";

        resp = HttpsPost(host, path, headers, body);
        return ParseAIContent(resp);
    }

    // -----------------------------------------------------------------
    //  Claude — Anthropic Messages API (different headers + body)
    // -----------------------------------------------------------------
    case PROV_CLAUDE: {
        std::string body =
            R"({"model":")" + model +
            R"(","max_tokens":1200,"messages":[{"role":"user","content":")" + escaped +
            R"("}]})";

        std::wstring headers =
            L"Content-Type: application/json\r\n"
            L"x-api-key: " + wKey + L"\r\n"
            L"anthropic-version: 2023-06-01\r\n";

        resp = HttpsPost(L"api.anthropic.com", L"/v1/messages", headers, body);
        return ParseAnthropicContent(resp);
    }

    default:
        return "[ERROR] Unknown provider";
    }
}

// ============================================================================
//  WebView2 Invisible Browser — with URL address bar
// ============================================================================
// Real WebView2-based browser window that is:
//   - Hidden from screen recording via WDA_EXCLUDEFROMCAPTURE (Win10 2004+)
//   - Layered + transparent for stealth
//   - Full browsing capability (Google, YouTube, ChatGPT, etc.)
//   - Embedded URL bar with Go button for navigation
//   - Toggle on/off with Ctrl+Alt+B, dismiss with Escape
//
// Requires: WebView2 Runtime + WebView2Loader.dll alongside the .exe

static HWND                            g_BrowserHwnd    = NULL;
static HWND                            g_UrlEdit        = NULL;   // URL bar
static ComPtr<ICoreWebView2>           g_WebView        = nullptr;
static ComPtr<ICoreWebView2Controller> g_WebController  = nullptr;
static bool                            g_BrowserVisible = false;

#define ID_URL_EDIT     3001
#define ID_URL_GO       3002
#define BROWSER_BAR_H   36   // height of the URL bar area in pixels

// Forward declarations
static LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void             InitWebView2(HWND hwnd);

// Navigate to whatever is in the URL edit box
static void BrowserNavigate() {
    if (!g_UrlEdit || !g_WebView) return;
    char url[2048] = {};
    GetWindowTextA(g_UrlEdit, url, sizeof(url) - 1);
    std::string sUrl(url);
    // Auto-prefix https:// if user didn't type a protocol
    if (sUrl.find("://") == std::string::npos) {
        sUrl = "https://" + sUrl;
    }
    std::wstring wUrl(sUrl.begin(), sUrl.end());
    g_WebView->Navigate(wUrl.c_str());
}

static void CreateBrowserWindow() {
    if (g_BrowserHwnd && IsWindow(g_BrowserHwnd)) return;

    const char* cls = "ZPInvisibleBrowser";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = BrowserProc;
        wc.hInstance      = GetModuleHandle(NULL);
        wc.lpszClassName  = cls;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground  = CreateSolidBrush(g_BgColor);  // icy bg instead of plain white
        RegisterClassA(&wc);
        registered = true;
    }

    int w = 1100, h = 750;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_BrowserHwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "ZeroPoint Browser",
        WS_POPUP | WS_CLIPCHILDREN,
        (sx - w) / 2, (sy - h) / 2, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(g_BrowserHwnd, 0, g_WindowAlpha, LWA_ALPHA);

    // Hide from screen recording (WDA_EXCLUDEFROMCAPTURE = 0x11)
    typedef BOOL(WINAPI* PFN_SWDA)(HWND, DWORD);
    PFN_SWDA pSWDA = (PFN_SWDA)GetProcAddress(
        GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
    if (pSWDA) pSWDA(g_BrowserHwnd, 0x00000011);

    // ---- URL bar: [Edit control] [Go button] ----
    g_UrlEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "https://www.google.com",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, 4, w - 80, BROWSER_BAR_H - 8,
        g_BrowserHwnd, (HMENU)ID_URL_EDIT, GetModuleHandle(NULL), NULL);

    CreateWindowExA(0, "BUTTON", "Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 68, 4, 56, BROWSER_BAR_H - 8,
        g_BrowserHwnd, (HMENU)ID_URL_GO, GetModuleHandle(NULL), NULL);

    // Set a clean font on the URL bar
    HFONT urlFont = CreateAppFont(13, FW_NORMAL);
    SendMessage(g_UrlEdit, WM_SETFONT, (WPARAM)urlFont, TRUE);

    InitWebView2(g_BrowserHwnd);
}

static void InitWebView2(HWND hwnd) {
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

                            // Offset bounds below the URL bar
                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            bounds.top = BROWSER_BAR_H;
                            g_WebController->put_Bounds(bounds);

                            g_WebView->Navigate(L"https://www.google.com");

                            // Transparent background for frosted effect
                            ComPtr<ICoreWebView2Controller2> ctrl2;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2)))) {
                                COREWEBVIEW2_COLOR bg = { 0, 255, 255, 255 };
                                ctrl2->put_DefaultBackgroundColor(bg);
                            }

                            // Update URL bar when navigation completes
                            EventRegistrationToken token;
                            g_WebView->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                        if (g_UrlEdit && sender) {
                                            LPWSTR uri = nullptr;
                                            sender->get_Source(&uri);
                                            if (uri) {
                                                // Convert wide to narrow for SetWindowTextA
                                                int len = WideCharToMultiByte(CP_UTF8, 0, uri, -1, NULL, 0, NULL, NULL);
                                                std::string narrow(len, 0);
                                                WideCharToMultiByte(CP_UTF8, 0, uri, -1, &narrow[0], len, NULL, NULL);
                                                SetWindowTextA(g_UrlEdit, narrow.c_str());
                                                CoTaskMemFree(uri);
                                            }
                                        }
                                        return S_OK;
                                    }).Get(), &token);

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
            bounds.top = BROWSER_BAR_H;  // keep URL bar visible at top
            g_WebController->put_Bounds(bounds);
        }
        // Resize the URL edit to match new width
        if (g_UrlEdit) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(g_UrlEdit, 8, 4, rc.right - 80, BROWSER_BAR_H - 8, TRUE);
        }
        return 0;
    }

    case WM_COMMAND:
        // "Go" button clicked or Enter pressed in URL edit
        if (LOWORD(wp) == ID_URL_GO ||
            (LOWORD(wp) == ID_URL_EDIT && HIWORD(wp) == EN_KILLFOCUS)) {
            BrowserNavigate();
        }
        return 0;

    case WM_KEYDOWN:
        // Escape key dismisses the browser
        if (wp == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
            g_BrowserVisible = false;
            return 0;
        }
        // Enter key navigates
        if (wp == VK_RETURN) {
            BrowserNavigate();
            return 0;
        }
        break;

    case WM_DESTROY:
        g_WebController  = nullptr;
        g_WebView        = nullptr;
        g_BrowserHwnd    = NULL;
        g_UrlEdit        = NULL;
        g_BrowserVisible = false;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Toggle browser visibility — Ctrl+Alt+B hotkey
void OpenInvisibleBrowser() {
    if (g_BrowserVisible && g_BrowserHwnd && IsWindow(g_BrowserHwnd)) {
        ShowWindow(g_BrowserHwnd, SW_HIDE);
        g_BrowserVisible = false;
    } else {
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

#define ID_BTN_INJECT       1001
#define ID_BTN_THEME        1002
#define ID_BTN_ALPHA        1003
#define ID_BTN_QUIT         1004
#define ID_COMBO_PROVIDER   1005

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
    static HWND hProvCombo = NULL;

    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);

        // ---- Provider dropdown ----
        hProvCombo = CreateWindowA("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            70, 218, 300, 200, hwnd, (HMENU)ID_COMBO_PROVIDER, NULL, NULL);
        for (int i = 0; i < PROV_COUNT; i++) {
            SendMessageA(hProvCombo, CB_ADDSTRING, 0,
                         (LPARAM)g_Providers[i].displayName);
        }
        SendMessageA(hProvCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wp) == ID_COMBO_PROVIDER && HIWORD(wp) == CBN_SELCHANGE) {
            int sel = (int)SendMessage(hProvCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) SetActiveProvider(sel);
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

        // ---- AI Provider label ----
        HFONT labelFont = CreateAppFont(11, FW_SEMIBOLD);
        SelectObject(memDC, labelFont);
        SetTextColor(memDC, g_AccentColor);
        RECT provLabelRc = { 70, 203, w - 70, 218 };
        DrawTextA(memDC, "AI PROVIDER", -1, &provLabelRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(labelFont);

        // Accent divider (below combo)
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

        // ---- Footer (version + opacity) ----
        HFONT footerFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, footerFont);
        SetTextColor(memDC, g_ShadowColor);
        RECT footerRc = { 30, h - 36, w - 30, h - 20 };
        char footerBuf[96];
        snprintf(footerBuf, sizeof(footerBuf), "ZeroPoint v2.0  |  opacity %d/255", g_WindowAlpha);
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
            if (pt.y >= 200 && pt.y <= 245 && pt.x >= 60 && pt.x <= 380)
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
//  API Key Input Dialog — inline prompt (no manual config.ini editing)
// ============================================================================

static HWND g_hKeyEdit = NULL;
static bool g_KeyDialogOK = false;

static LRESULT CALLBACK KeyDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // Dynamic label showing which provider we're entering a key for
        std::string label = "Enter your " + GetActiveProviderName() + " API key:";
        CreateWindowA("STATIC", label.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 16, 360, 20, hwnd, NULL, NULL, NULL);

        g_hKeyEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
            20, 42, 360, 24, hwnd, (HMENU)4001, NULL, NULL);

        HFONT f = CreateAppFont(13, FW_NORMAL);
        SendMessage(g_hKeyEdit, WM_SETFONT, (WPARAM)f, TRUE);

        CreateWindowA("BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            130, 80, 80, 28, hwnd, (HMENU)IDOK, NULL, NULL);

        CreateWindowA("BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 80, 80, 28, hwnd, (HMENU)IDCANCEL, NULL, NULL);

        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            char buf[512] = {};
            GetWindowTextA(g_hKeyEdit, buf, sizeof(buf) - 1);
            std::string key(buf);
            if (!key.empty()) {
                // Save to the ACTIVE provider
                SetProviderKey(GetActiveProvider(), key);
                g_KeyDialogOK = true;
            }
            DestroyWindow(hwnd);
        } else if (LOWORD(wp) == IDCANCEL) {
            g_KeyDialogOK = false;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool ShowKeyInputDialog() {
    const char* cls = "ZPKeyDialog";
    WNDCLASSA wc = {};
    wc.lpfnWndProc   = KeyDialogProc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = cls;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, "ZeroPoint — API Key",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        (sw - 400) / 2, (sh - 130) / 2, 400, 130,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnregisterClassA(cls, GetModuleHandle(NULL));
    return g_KeyDialogOK;
}

// ============================================================================
//  Sidebar Menu — right-edge panel for live provider switching
// ============================================================================
//  Triggered by Ctrl+Alt+H. Frosted sidebar on the right edge with a
//  dropdown to switch providers, key config, active provider + last answer.

#define ID_SIDEBAR_COMBO  5001
#define ID_SIDEBAR_KEYBTN 5002

static std::string g_LastAnswer = "(no AI query yet)";
static HWND        g_MenuHwnd   = NULL;
static HWND        g_SidebarCombo = NULL;
static HWND        g_SidebarKeyBtn = NULL;

static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);

        // ---- Provider dropdown (native combobox) ----
        g_SidebarCombo = CreateWindowA("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            12, 44, 216, 200, hwnd, (HMENU)ID_SIDEBAR_COMBO, NULL, NULL);

        HFONT comboFont = CreateAppFont(13, FW_SEMIBOLD);
        SendMessage(g_SidebarCombo, WM_SETFONT, (WPARAM)comboFont, TRUE);

        for (int i = 0; i < PROV_COUNT; i++) {
            SendMessageA(g_SidebarCombo, CB_ADDSTRING, 0,
                         (LPARAM)g_Providers[i].displayName);
        }
        SendMessageA(g_SidebarCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);

        // ---- "Set API Key" button ----
        g_SidebarKeyBtn = CreateWindowA("BUTTON", "Set API Key...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 78, 216, 28, hwnd, (HMENU)ID_SIDEBAR_KEYBTN, NULL, NULL);

        HFONT btnFont = CreateAppFont(11, FW_NORMAL);
        SendMessage(g_SidebarKeyBtn, WM_SETFONT, (WPARAM)btnFont, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        // Provider dropdown changed
        if (LOWORD(wp) == ID_SIDEBAR_COMBO && HIWORD(wp) == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_SidebarCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) SetActiveProvider(sel);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        // "Set API Key" button clicked
        if (LOWORD(wp) == ID_SIDEBAR_KEYBTN) {
            ShowWindow(hwnd, SW_HIDE);
            extern bool ShowKeyInputDialog();
            ShowKeyInputDialog();
            // Refresh combo selection (provider may have been set via dialog)
            SendMessageA(g_SidebarCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);
            InvalidateRect(hwnd, NULL, TRUE);
            ShowWindow(hwnd, SW_SHOW);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Frosted background
        HBRUSH bg = CreateSolidBrush(g_BgColor);
        FillRect(memDC, &rc, bg);
        DeleteObject(bg);

        FillFrosted(memDC, rc, g_BgPanel, 210);
        DrawInnerGlow(memDC, rc);

        // Left accent border
        HPEN pen = CreatePen(PS_SOLID, 2, g_AccentColor);
        HPEN oldPen = (HPEN)SelectObject(memDC, pen);
        MoveToEx(memDC, 0, 0, NULL);
        LineTo(memDC, 0, h);
        SelectObject(memDC, oldPen);
        DeleteObject(pen);

        SetBkMode(memDC, TRANSPARENT);

        // ---- Title ----
        HFONT titleFont = CreateAppFont(16, FW_LIGHT);
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, g_AccentColor);
        RECT titleRc = { 14, 10, w - 10, 34 };
        DrawTextA(memDC, "AI PROVIDER", -1, &titleRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(titleFont);

        DrawAccentLine(memDC, 14, 36, w - 28);

        // (Combo box at y=44 and key button at y=78 are native controls,
        //  drawn by Windows on top of our GDI paint — no need to paint them)

        // ---- Divider below controls ----
        DrawAccentLine(memDC, 14, 114, w - 28);

        // ---- Key status indicator ----
        HFONT statusFont = CreateAppFont(10, FW_SEMIBOLD);
        SelectObject(memDC, statusFont);
        bool hasKey = !GetProviderKey(GetActiveProvider()).empty();
        SetTextColor(memDC, hasKey ? RGB(0x22, 0xDD, 0x66) : RGB(0xFF, 0x44, 0x44));
        std::string keyStatus = hasKey ? "API Key: Configured" : "API Key: Not Set";
        RECT keyRc = { 14, 120, w - 10, 134 };
        DrawTextA(memDC, keyStatus.c_str(), -1, &keyRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(statusFont);

        // ---- Active provider label ----
        HFONT activeFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, activeFont);
        SetTextColor(memDC, g_TextSecondary);
        std::string activeLine = "Active: " + GetActiveProviderName();
        RECT activeLblRc = { 14, 140, w - 10, 154 };
        DrawTextA(memDC, activeLine.c_str(), -1, &activeLblRc,
                  DT_LEFT | DT_SINGLELINE);
        DeleteObject(activeFont);

        // ---- Divider ----
        DrawAccentLine(memDC, 14, 160, w - 28);

        // ---- "Last Answer" header ----
        HFONT answerHdrFont = CreateAppFont(10, FW_SEMIBOLD);
        SelectObject(memDC, answerHdrFont);
        SetTextColor(memDC, g_AccentColor);
        RECT hdrRc = { 14, 166, w - 10, 180 };
        DrawTextA(memDC, "LAST ANSWER", -1, &hdrRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(answerHdrFont);

        // ---- Last answer text ----
        HFONT textFont = CreateAppFont(11, FW_NORMAL);
        SelectObject(memDC, textFont);
        SetTextColor(memDC, g_TextPrimary);
        RECT textRc = { 14, 184, w - 10, h - 24 };
        DrawTextA(memDC, g_LastAnswer.c_str(), -1, &textRc,
                  DT_LEFT | DT_WORDBREAK);
        DeleteObject(textFont);

        // ---- Dismiss hint ----
        HFONT hintFont = CreateAppFont(9, FW_NORMAL);
        SelectObject(memDC, hintFont);
        SetTextColor(memDC, g_ShadowColor);
        RECT hintRc = { 10, h - 20, w - 10, h - 4 };
        DrawTextA(memDC, "Ctrl+Alt+H or Esc to dismiss", -1, &hintRc,
                  DT_CENTER | DT_SINGLELINE);
        DeleteObject(hintFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        g_MenuHwnd = NULL;
        g_SidebarCombo = NULL;
        g_SidebarKeyBtn = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Toggle sidebar visibility
static void ToggleFullMenu() {
    if (g_MenuHwnd && IsWindow(g_MenuHwnd) && IsWindowVisible(g_MenuHwnd)) {
        ShowWindow(g_MenuHwnd, SW_HIDE);
        return;
    }

    if (!g_MenuHwnd || !IsWindow(g_MenuHwnd)) {
        const char* cls = "ZPSidebar";
        static bool registered = false;
        if (!registered) {
            WNDCLASSA wc = {};
            wc.lpfnWndProc   = SidebarProc;
            wc.hInstance      = GetModuleHandle(NULL);
            wc.lpszClassName  = cls;
            wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
            RegisterClassA(&wc);
            registered = true;
        }

        // Sidebar: right edge, full height minus taskbar
        int sideW = 240;
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        int barH = sy - 50;  // approximate taskbar height

        g_MenuHwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            cls, NULL, WS_POPUP,
            sx - sideW, 0, sideW, barH,
            NULL, NULL, GetModuleHandle(NULL), NULL);

        SetLayeredWindowAttributes(g_MenuHwnd, 0, g_WindowAlpha, LWA_ALPHA);

        // Hide from screen recording
        typedef BOOL(WINAPI* PFN_SWDA)(HWND, DWORD);
        PFN_SWDA pSWDA = (PFN_SWDA)GetProcAddress(
            GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
        if (pSWDA) pSWDA(g_MenuHwnd, 0x00000011);
    }

    // Sync the dropdown with current provider when showing
    if (g_SidebarCombo)
        SendMessageA(g_SidebarCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);
    InvalidateRect(g_MenuHwnd, NULL, TRUE);
    ShowWindow(g_MenuHwnd, SW_SHOW);
    SetForegroundWindow(g_MenuHwnd);
}

// ============================================================================
//  Entry Point
// ============================================================================

static const char* ZEROPOINT_VERSION = "v2.0.0";

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
    //  First-launch: prompt for API key if the active provider has none
    // ------------------------------------------------------------------
    if (GetProviderKey(GetActiveProvider()).empty()) {
        if (!ShowKeyInputDialog()) {
            CoUninitialize();
            return 0;   // user cancelled
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
        MessageBoxA(NULL,
            "Process injection failed.\n\n"
            "Check that ZeroPointPayload.exe exists in:\n"
            "C:\\ProgramData\\ZeroPoint\\",
            "ZeroPoint", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // ------------------------------------------------------------------
    //  Background hotkey loop
    // ------------------------------------------------------------------
    bool keyZ = false, keyH = false, keyB = false, keyX = false;

    while (true) {
        // Process pending messages (WebView2 async callbacks need this)
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
                g_LastAnswer = answer;  // store for full menu display
                ShowAIPopup(answer);
            }
        } else {
            keyZ = false;
        }

        // Ctrl+Alt+H — Full frosted menu overlay (toggle)
        if (ctrl && alt && (GetAsyncKeyState(0x48) & 0x8000)) {
            if (!keyH) {
                keyH = true;
                ToggleFullMenu();
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
                // Destroy all overlay windows
                if (g_BrowserHwnd && IsWindow(g_BrowserHwnd))
                    DestroyWindow(g_BrowserHwnd);
                if (g_MenuHwnd && IsWindow(g_MenuHwnd))
                    DestroyWindow(g_MenuHwnd);
                PanicKillAndWipe();
                break;    // PanicKillAndWipe terminates, but just in case
            }
        } else {
            keyX = false;
        }

        Sleep(10);
    }

    CoUninitialize();
    return 0;
}

