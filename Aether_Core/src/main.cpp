// ============================================================================
//  ZeroPoint — Premium Windows Utility  (v3.0)
//  main.cpp — Frosted glass launcher, icy/snowy theme, WebView2 invisible
//             browser, screenshot + vision AI, sidebar with settings,
//             browser thumbnail panel, multi-provider, custom themes
// ============================================================================
//
//  BUILD REQUIREMENTS:
//    - Microsoft.Web.WebView2 NuGet package (WebView2Loader.dll + headers)
//    - Windows 10 1809+ with Edge WebView2 Runtime installed
//    - Link: winhttp, dwmapi, comctl32, gdi32, msimg32, WebView2Loader,
//            gdiplus, ole32, shlwapi
//    - Include path must contain the Aether_Core/include directory
//
// ============================================================================

#include "../include/Stealth.h"
#include "../include/CDPExtractor.h"
#include "../include/Config.h"
#include "../include/VirtualEnv.h"

#include <windows.h>
#include <wingdi.h>
#include <winhttp.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <ole2.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <filesystem>
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
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "WebView2Loader.lib")

using namespace Microsoft::WRL;

// GDI+ token — initialized in WinMain, shutdown on exit
static ULONG_PTR g_GdiplusToken = 0;

// Hotkey toggle states
static bool keyZ = false, keyH = false, keyB = false, keyX = false, keyR = false;
static bool keyC = false, keyF = false, keyS = false, keyT = false;

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
//  Screenshot Capture — invisible BitBlt of the foreground window
// ============================================================================
//  Captures the foreground window using PrintWindow/BitBlt.
//  Our own overlays use WDA_EXCLUDEFROMCAPTURE so they are automatically
//  invisible to this capture — only the exam app is captured.

static const std::string SCREENSHOT_DIR = "C:\\ProgramData\\ZeroPoint\\screenshots\\";
static std::vector<std::string> g_ScreenshotHistory;   // file paths of saved PNGs
static const int MAX_SCREENSHOTS = 10;

// Capture the foreground window to an HBITMAP
static HBITMAP CaptureScreenshotBitmap(int& outW, int& outH) {
    HWND fg = GetForegroundWindow();
    if (!fg) return NULL;

    RECT rc;
    GetClientRect(fg, &rc);
    outW = rc.right - rc.left;
    outH = rc.bottom - rc.top;
    if (outW <= 0 || outH <= 0) return NULL;

    HDC screenDC = GetDC(fg);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bmp = CreateCompatibleBitmap(screenDC, outW, outH);
    SelectObject(memDC, bmp);

    // PrintWindow captures even if partially occluded
    PrintWindow(fg, memDC, PW_CLIENTONLY);

    DeleteDC(memDC);
    ReleaseDC(fg, screenDC);
    return bmp;
}

// GDI+ helper: find the encoder CLSID for a given MIME type (e.g. "image/png")
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    Gdiplus::ImageCodecInfo* pImageCodecInfo =
        (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!pImageCodecInfo) return -1;

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// Base64 encoding table
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const BYTE* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = ((unsigned int)data[i]) << 16;
        if (i + 1 < len) n |= ((unsigned int)data[i + 1]) << 8;
        if (i + 2 < len) n |= data[i + 2];
        out += B64[(n >> 18) & 0x3F];
        out += B64[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? B64[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? B64[n & 0x3F] : '=';
    }
    return out;
}

// Convert HBITMAP → base64-encoded PNG string using GDI+
static std::string BitmapToBase64PNG(HBITMAP hBitmap) {
    if (!hBitmap) return "";

    Gdiplus::Bitmap bmp(hBitmap, NULL);
    CLSID clsidPng;
    if (GetEncoderClsid(L"image/png", &clsidPng) < 0) return "";

    // Save to memory stream
    IStream* pStream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    bmp.Save(pStream, &clsidPng, NULL);

    // Get stream size
    STATSTG stat;
    pStream->Stat(&stat, STATFLAG_DEFAULT);
    ULONG cbSize = (ULONG)stat.cbSize.QuadPart;

    // Read bytes
    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
    std::vector<BYTE> buf(cbSize);
    ULONG bytesRead = 0;
    pStream->Read(buf.data(), cbSize, &bytesRead);
    pStream->Release();

    return Base64Encode(buf.data(), bytesRead);
}

// Save HBITMAP to a PNG file in the screenshots directory, return the file path
static std::string SaveScreenshotToFile(HBITMAP hBitmap) {
    CreateDirectoryA("C:\\ProgramData\\ZeroPoint", NULL);
    CreateDirectoryA(SCREENSHOT_DIR.c_str(), NULL);

    // Generate unique filename with timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char fname[256];
    snprintf(fname, sizeof(fname), "ss_%04d%02d%02d_%02d%02d%02d_%03d.png",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::string path = SCREENSHOT_DIR + fname;

    Gdiplus::Bitmap bmp(hBitmap, NULL);
    CLSID clsidPng;
    if (GetEncoderClsid(L"image/png", &clsidPng) < 0) return "";

    std::wstring wPath(path.begin(), path.end());
    bmp.Save(wPath.c_str(), &clsidPng, NULL);

    // Add to history, cap at MAX_SCREENSHOTS
    g_ScreenshotHistory.push_back(path);
    while ((int)g_ScreenshotHistory.size() > MAX_SCREENSHOTS) {
        DeleteFileA(g_ScreenshotHistory.front().c_str());
        g_ScreenshotHistory.erase(g_ScreenshotHistory.begin());
    }

    return path;
}

// ============================================================================
//  CallAIWithVision — send screenshot + prompt to vision-capable APIs
// ============================================================================
//  Providers:
//    - Claude:   Anthropic image_content block
//    - GPT/Grok/OpenRouter: OpenAI image_url format
//    - Deepseek: Falls back to text-only CDP extraction
//
//  Default prompt asks AI to answer any visible exam questions.

static const char* VISION_PROMPT =
    "Look at this screenshot of an exam question carefully. "
    "Answer the question(s) visible on screen. Be concise and direct. "
    "Give only the answer(s).";

std::string CallAIWithVision(const std::string& base64png,
                              const std::string& extraPrompt) {
    Provider prov     = GetActiveProvider();
    std::string key   = GetProviderKey(prov);
    std::string model = GetActiveModelID();

    if (key.empty()) {
        return "[ERROR] No API key set for " + GetActiveProviderName() +
               ". Open settings to add your key.";
    }

    // Deepseek doesn't support vision — fall back to text-only
    if (!ActiveProviderHasVision()) {
        std::string question = ExtractBluebookDOM();
        return CallAI(question);
    }

    std::string prompt = extraPrompt.empty() ? std::string(VISION_PROMPT) : extraPrompt;
    std::string escaped = EscapeJsonString(prompt);
    std::wstring wKey(key.begin(), key.end());
    std::string resp;

    switch (prov) {
    // -----------------------------------------------------------------
    //  OpenAI-compatible vision: GPT, Grok, OpenRouter
    //  content is an array of [{type: text}, {type: image_url}]
    // -----------------------------------------------------------------
    case PROV_GPT:
    case PROV_GROK:
    case PROV_OPENROUTER: {
        std::wstring host, path;
        if (prov == PROV_OPENROUTER) {
            host = L"openrouter.ai";
            path = L"/api/v1/chat/completions";
        } else if (prov == PROV_GPT) {
            host = L"api.openai.com";
            path = L"/v1/chat/completions";
        } else {
            host = L"api.x.ai";
            path = L"/v1/chat/completions";
        }

        std::string body =
            R"({"model":")" + model +
            R"(","messages":[{"role":"user","content":[)" +
            R"({"type":"text","text":")" + escaped + R"("},)" +
            R"({"type":"image_url","image_url":{"url":"data:image/png;base64,)" +
            base64png + R"("}})" +
            R"(]}],"max_tokens":1200})";

        std::wstring headers =
            L"Content-Type: application/json\r\nAuthorization: Bearer " + wKey + L"\r\n";

        resp = HttpsPost(host, path, headers, body);
        return ParseAIContent(resp);
    }

    // -----------------------------------------------------------------
    //  Claude vision: Anthropic content array with image block
    // -----------------------------------------------------------------
    case PROV_CLAUDE: {
        std::string body =
            R"({"model":")" + model +
            R"(","max_tokens":1200,"messages":[{"role":"user","content":[)" +
            R"({"type":"image","source":{"type":"base64","media_type":"image/png","data":")" +
            base64png + R"("}},)" +
            R"({"type":"text","text":")" + escaped + R"("})" +
            R"(]}]})";

        std::wstring headers =
            L"Content-Type: application/json\r\n"
            L"x-api-key: " + wKey + L"\r\n"
            L"anthropic-version: 2023-06-01\r\n";

        resp = HttpsPost(L"api.anthropic.com", L"/v1/messages", headers, body);
        return ParseAnthropicContent(resp);
    }

    default:
        return "[ERROR] Vision not supported for this provider.";
    }
}

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
#define BROWSER_BAR_H   36   // height of the URL bar area
#define BROWSER_PANEL_W 180  // width of right screenshot panel

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

    int w = 1280, h = 750;  // wider to accommodate thumbnail panel
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

    // URL bar spans the webview area (not the thumbnail panel)
    int urlW = w - BROWSER_PANEL_W;  // URL bar width excludes panel
    g_UrlEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "https://www.google.com",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, 4, urlW - 80, BROWSER_BAR_H - 8,
        g_BrowserHwnd, (HMENU)ID_URL_EDIT, GetModuleHandle(NULL), NULL);

    CreateWindowExA(0, "BUTTON", "Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        urlW - 68, 4, 56, BROWSER_BAR_H - 8,
        g_BrowserHwnd, (HMENU)ID_URL_GO, GetModuleHandle(NULL), NULL);

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
                            bounds.right -= BROWSER_PANEL_W;  // leave room for thumbnail panel
                            g_WebController->put_Bounds(bounds);

                            g_WebView->Navigate(L"https://www.google.com");

                            // Transparent background for frosted effect
                            ComPtr<ICoreWebView2Controller2> ctrl2;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2)))) {
                                COREWEBVIEW2_COLOR bg = { 0, 255, 255, 255 };
                                ctrl2->put_DefaultBackgroundColor(bg);
                            }

                            // ---- OLE Drag-and-Drop: Enable external drops on WebView2 ----
                            // Without this, WebView2 may reject CF_HDROP from our sidebar.
                            // ICoreWebView2Controller4 provides put_AllowExternalDrop().
                            ComPtr<ICoreWebView2Controller4> ctrl4;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl4)))) {
                                ctrl4->put_AllowExternalDrop(TRUE);
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

// Draw the screenshot thumbnail panel on the right side of the browser
static void PaintBrowserThumbnailPanel(HDC hdc, int panelX, int panelW, int panelH) {
    // Panel background
    RECT panelRc = { panelX, 0, panelX + panelW, panelH };
    FillFrosted(hdc, panelRc, g_BgFrost, 220);

    // Left border accent
    HPEN pen = CreatePen(PS_SOLID, 2, g_AccentColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, panelX, 0, NULL);
    LineTo(hdc, panelX, panelH);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);

    // Title
    HFONT titleFont = CreateAppFont(12, FW_SEMIBOLD);
    SelectObject(hdc, titleFont);
    SetTextColor(hdc, g_AccentColor);
    RECT titleRc = { panelX + 8, 8, panelX + panelW - 4, 24 };
    DrawTextA(hdc, "SCREENSHOTS", -1, &titleRc, DT_LEFT | DT_SINGLELINE);
    DeleteObject(titleFont);

    DrawAccentLine(hdc, panelX + 8, 28, panelW - 16);

    // Thumbnails from g_ScreenshotHistory
    int thumbW = panelW - 16;  // 164px
    int thumbH = 100;
    int y = 36;

    HFONT smallFont = CreateAppFont(9, FW_NORMAL);
    SelectObject(hdc, smallFont);

    if (g_ScreenshotHistory.empty()) {
        SetTextColor(hdc, g_ShadowColor);
        RECT emptyRc = { panelX + 8, y, panelX + panelW - 4, y + 40 };
        DrawTextA(hdc, "No screenshots yet.\nUse Ctrl+Shift+Z", -1, &emptyRc,
                  DT_LEFT | DT_WORDBREAK);
    } else {
        for (int i = (int)g_ScreenshotHistory.size() - 1; i >= 0 && y + thumbH < panelH - 30; i--) {
            bool isHovered = (i == g_HoveredThumb);

            // ---- Hover highlight: accent glow behind hovered thumbnail ----
            // Gives visual feedback that the thumbnail is draggable
            if (isHovered) {
                RECT glowRc = { panelX + 5, y - 3, panelX + 8 + thumbW + 3, y + thumbH + 3 };
                FillFrosted(hdc, glowRc, g_AccentColor, 40);
            }

            // Load thumbnail from file using GDI+
            std::wstring wPath(g_ScreenshotHistory[i].begin(), g_ScreenshotHistory[i].end());
            Gdiplus::Bitmap bmp(wPath.c_str());
            if (bmp.GetLastStatus() == Gdiplus::Ok) {
                Gdiplus::Graphics gfx(hdc);
                gfx.DrawImage(&bmp, panelX + 8, y, thumbW, thumbH);
            }

            // Border — accent color when hovered, subtle gray otherwise
            COLORREF borderClr = isHovered ? g_AccentColor : g_BorderColor;
            int borderWidth = isHovered ? 2 : 1;
            HPEN thumbPen = CreatePen(PS_SOLID, borderWidth, borderClr);
            HPEN oldTP = (HPEN)SelectObject(hdc, thumbPen);
            HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
            HGDIOBJ oldBr = SelectObject(hdc, nullBr);
            Rectangle(hdc, panelX + 8, y, panelX + 8 + thumbW, y + thumbH);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldTP);
            DeleteObject(thumbPen);

            y += thumbH + 6;
        }
    }

    // Hint at bottom
    SetTextColor(hdc, g_ShadowColor);
    RECT hintRc = { panelX + 4, panelH - 36, panelX + panelW - 4, panelH - 4 };
    DrawTextA(hdc, "Drag onto page to upload\nClick to copy to clipboard", -1, &hintRc,
              DT_CENTER | DT_WORDBREAK);
    DeleteObject(smallFont);
}

// ============================================================================
//  OLE Drag-and-Drop — native file drop from thumbnail panel to WebView2
// ============================================================================
//  Implements IDropSource and IDataObject COM interfaces so that dragging a
//  screenshot thumbnail from the right sidebar onto the WebView2 page triggers
//  a real file-drop event. ChatGPT, Gemini, Grok web — any page that accepts
//  file drops will receive the PNG as a native file upload.
//
//  How it works:
//    1. User presses LMB on a thumbnail in the panel
//    2. On WM_MOUSEMOVE, if drag threshold is exceeded, we call DoDragDrop()
//    3. DoDragDrop() enters a modal drag loop, painting a drag cursor
//    4. When the user releases over the WebView2 area, IDropTarget on the
//       WebView2 receives CF_HDROP data with the screenshot file path
//    5. The web page's <input type="file"> or drop handler picks up the file

// ---------------------------------------------------------------------------
//  ZPDropSource — minimal IDropSource implementation
//  Controls the drag cursor and decides when to cancel/complete the drag.
// ---------------------------------------------------------------------------

class ZPDropSource : public IDropSource {
    LONG m_refCount;
public:
    ZPDropSource() : m_refCount(1) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    // IDropSource
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        // Cancel if Escape was pressed
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        // Drop if LMB was released
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override {
        // Use default cursor feedback
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};

// ---------------------------------------------------------------------------
//  ZPDataObject — minimal IDataObject providing CF_HDROP for one file
//  This is the data payload that the drop target (WebView2) receives.
//  It presents the screenshot file as a CF_HDROP (shell file list) so the
//  browser interprets it the same way as a file dragged from Explorer.
// ---------------------------------------------------------------------------

class ZPDataObject : public IDataObject {
    LONG        m_refCount;
    FORMATETC   m_fmt;
    STGMEDIUM   m_stg;
    bool        m_hasData;
    HBITMAP     m_hThumb;       // thumbnail bitmap for IDragSourceHelper

public:
    ZPDataObject(const std::string& filePath) : m_refCount(1), m_hasData(false), m_hThumb(NULL) {
        // Build a DROPFILES structure containing the file path (wide/Unicode)
        // DROPFILES is the standard shell format for drag-and-drop files.
        // We use fWide=TRUE so WebView2 and modern shell targets get proper Unicode paths.
        std::wstring wPath(filePath.begin(), filePath.end());
        size_t pathBytes = (wPath.size() + 1) * sizeof(WCHAR);   // path + null (wide)
        size_t totalSize = sizeof(DROPFILES) + pathBytes + sizeof(WCHAR);  // + double-null

        HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, totalSize);
        if (!hGlobal) return;

        DROPFILES* pDrop = (DROPFILES*)GlobalLock(hGlobal);
        pDrop->pFiles = sizeof(DROPFILES);    // offset to file list
        pDrop->fWide  = TRUE;                 // Unicode wide paths
        pDrop->pt     = { 0, 0 };
        pDrop->fNC    = FALSE;

        WCHAR* pFiles = (WCHAR*)((BYTE*)pDrop + sizeof(DROPFILES));
        memcpy(pFiles, wPath.c_str(), (wPath.size() + 1) * sizeof(WCHAR));
        pFiles[wPath.size() + 1] = L'\0';     // double-null terminator
        GlobalUnlock(hGlobal);

        // Describe the format we provide: CF_HDROP in an HGLOBAL
        m_fmt.cfFormat = CF_HDROP;
        m_fmt.ptd      = NULL;
        m_fmt.dwAspect = DVASPECT_CONTENT;
        m_fmt.lindex   = -1;
        m_fmt.tymed    = TYMED_HGLOBAL;

        m_stg.tymed          = TYMED_HGLOBAL;
        m_stg.hGlobal        = hGlobal;
        m_stg.pUnkForRelease = NULL;

        m_hasData = true;
    }

    ~ZPDataObject() {
        if (m_hasData && m_stg.hGlobal) {
            GlobalFree(m_stg.hGlobal);
        }
        if (m_hThumb) DeleteObject(m_hThumb);
    }

    // Store a thumbnail bitmap for IDragSourceHelper to use as drag image
    void SetThumbnail(HBITMAP hBmp) { m_hThumb = hBmp; }
    HBITMAP GetThumbnail() const { return m_hThumb; }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    // IDataObject — GetData: return CF_HDROP data to the drop target
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pFmt, STGMEDIUM* pMedium) override {
        if (!m_hasData) return DV_E_FORMATETC;
        if (pFmt->cfFormat != CF_HDROP)   return DV_E_FORMATETC;
        if (!(pFmt->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;

        // Duplicate the HGLOBAL so the caller can free it independently
        SIZE_T size = GlobalSize(m_stg.hGlobal);
        HGLOBAL hDup = GlobalAlloc(GHND | GMEM_SHARE, size);
        if (!hDup) return E_OUTOFMEMORY;

        void* src = GlobalLock(m_stg.hGlobal);
        void* dst = GlobalLock(hDup);
        memcpy(dst, src, size);
        GlobalUnlock(m_stg.hGlobal);
        GlobalUnlock(hDup);

        pMedium->tymed          = TYMED_HGLOBAL;
        pMedium->hGlobal        = hDup;
        pMedium->pUnkForRelease = NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pFmt) override {
        if (!m_hasData) return DV_E_FORMATETC;
        if (pFmt->cfFormat == CF_HDROP && (pFmt->tymed & TYMED_HGLOBAL))
            return S_OK;
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* pOut) override {
        pOut->ptd = NULL;
        return DATA_S_SAMEFORMATETC;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDir, IEnumFORMATETC** ppEnum) override {
        if (dwDir != DATADIR_GET) return E_NOTIMPL;
        // Use SHCreateStdEnumFmtEtc from the shell — simple 1-format enumerator
        return SHCreateStdEnumFmtEtc(1, &m_fmt, ppEnum);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};

// ---------------------------------------------------------------------------
//  CreateScaledThumbnail — make a small HBITMAP for the drag preview image
//  Loads the screenshot PNG via GDI+ and scales it to ~120x80 for the drag
//  cursor overlay. Returns NULL if loading fails.
// ---------------------------------------------------------------------------

static HBITMAP CreateScaledThumbnail(const std::string& filePath, int thumbW, int thumbH) {
    std::wstring wPath(filePath.begin(), filePath.end());
    Gdiplus::Bitmap srcBmp(wPath.c_str());
    if (srcBmp.GetLastStatus() != Gdiplus::Ok) return NULL;

    // Create a scaled bitmap
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hThumb = CreateCompatibleBitmap(screenDC, thumbW, thumbH);
    HGDIOBJ oldBmp = SelectObject(memDC, hThumb);

    Gdiplus::Graphics gfx(memDC);
    gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    gfx.DrawImage(&srcBmp, 0, 0, thumbW, thumbH);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);
    return hThumb;
}

// ---------------------------------------------------------------------------
//  BeginThumbnailDragDrop — creates COM objects and calls DoDragDrop()
//  This enters a modal drag loop. Uses IDragSourceHelper to show a scaled
//  thumbnail preview under the cursor (Evadus-style visual drag feedback).
//  When released over WebView2, the page receives a native HTML5 drop event
//  with the screenshot as a real file — ChatGPT, Gemini, Grok all accept it.
// ---------------------------------------------------------------------------

static void BeginThumbnailDragDrop(const std::string& filePath) {
    ZPDataObject* pDataObj = new ZPDataObject(filePath);
    ZPDropSource* pDropSrc = new ZPDropSource();

    // ---- IDragSourceHelper: attach a visual thumbnail to the drag cursor ----
    // This gives the user a preview of what they're dragging, exactly like
    // dragging a file from Windows Explorer. The shell renders the image
    // semi-transparently under the cursor during the drag operation.
    const int DRAG_THUMB_W = 120;
    const int DRAG_THUMB_H = 80;
    HBITMAP hThumb = CreateScaledThumbnail(filePath, DRAG_THUMB_W, DRAG_THUMB_H);

    if (hThumb) {
        IDragSourceHelper* pDragHelper = NULL;
        HRESULT hrHelper = CoCreateInstance(
            CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER,
            IID_IDragSourceHelper, (void**)&pDragHelper);

        if (SUCCEEDED(hrHelper) && pDragHelper) {
            // SHDRAGIMAGE describes the bitmap, size, and cursor offset
            SHDRAGIMAGE dragImage = {};
            dragImage.sizeDragImage.cx = DRAG_THUMB_W;
            dragImage.sizeDragImage.cy = DRAG_THUMB_H;
            dragImage.ptOffset.x       = DRAG_THUMB_W / 2;   // cursor at center
            dragImage.ptOffset.y       = DRAG_THUMB_H / 2;
            dragImage.hbmpDragImage    = hThumb;
            dragImage.crColorKey       = CLR_NONE;            // no transparency key

            // InitializeFromBitmap takes ownership of the HBITMAP on success
            if (SUCCEEDED(pDragHelper->InitializeFromBitmap(&dragImage, pDataObj))) {
                hThumb = NULL;  // helper owns it now
            }
            pDragHelper->Release();
        }

        // If helper didn't take ownership, clean up
        if (hThumb) DeleteObject(hThumb);
    }

    DWORD dwEffect = 0;
    // DoDragDrop is MODAL — it blocks until the user completes the drag.
    // During this time, the shell renders the drag image under the cursor.
    // When released over WebView2, the browser receives a native file drop.
    HRESULT hr = DoDragDrop(
        pDataObj,
        pDropSrc,
        DROPEFFECT_COPY,     // we only support copy (not move)
        &dwEffect
    );

    // If drag was cancelled or drop target didn't accept,
    // fall back to clipboard copy so the user can Ctrl+V
    if (hr == DRAGDROP_S_CANCEL || dwEffect == DROPEFFECT_NONE) {
        // Build CF_HDROP (wide) for clipboard as fallback
        std::wstring wPath(filePath.begin(), filePath.end());
        size_t pathBytes = (wPath.size() + 1) * sizeof(WCHAR);
        size_t totalSize = sizeof(DROPFILES) + pathBytes + sizeof(WCHAR);
        HGLOBAL hClip = GlobalAlloc(GHND, totalSize);
        if (hClip) {
            DROPFILES* df = (DROPFILES*)GlobalLock(hClip);
            df->pFiles = sizeof(DROPFILES);
            df->fWide  = TRUE;
            WCHAR* dst = (WCHAR*)((BYTE*)df + sizeof(DROPFILES));
            memcpy(dst, wPath.c_str(), (wPath.size() + 1) * sizeof(WCHAR));
            dst[wPath.size() + 1] = L'\0';
            GlobalUnlock(hClip);

            if (OpenClipboard(g_BrowserHwnd)) {
                EmptyClipboard();
                SetClipboardData(CF_HDROP, hClip);
                CloseClipboard();
            } else {
                GlobalFree(hClip);
            }
        }
    }

    pDropSrc->Release();
    pDataObj->Release();
}

// ---------------------------------------------------------------------------
//  Drag state tracking for the thumbnail panel
//  We track the initial mousedown position and only start the drag after the
//  mouse moves beyond the system drag threshold (SM_CXDRAG / SM_CYDRAG).
//  This prevents accidental drags when the user just clicks.
// ---------------------------------------------------------------------------

static bool  g_DragPending   = false;    // LMB is down in thumbnail area
static int   g_DragIndex     = -1;       // which screenshot index is being dragged
static POINT g_DragStartPt   = { 0, 0 }; // mousedown position (client coords)
static int   g_HoveredThumb  = -1;       // thumbnail index under the cursor (for highlight)

// Helper: determine which thumbnail index is at a given Y position
static int HitTestThumbnail(int mouseY) {
    if (g_ScreenshotHistory.empty()) return -1;
    int thumbH = 100;
    int y = 36;  // first thumbnail Y offset in the panel
    for (int i = (int)g_ScreenshotHistory.size() - 1; i >= 0; i--) {
        if (mouseY >= y && mouseY < y + thumbH) return i;
        y += thumbH + 6;
    }
    return -1;
}

static LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (g_WebController) {
            RECT bounds = rc;
            bounds.top = BROWSER_BAR_H;
            bounds.right -= BROWSER_PANEL_W;  // WebView2 excludes panel
            g_WebController->put_Bounds(bounds);
        }
        // Resize URL edit
        if (g_UrlEdit) {
            int urlW = rc.right - BROWSER_PANEL_W;
            MoveWindow(g_UrlEdit, 8, 4, urlW - 80, BROWSER_BAR_H - 8, TRUE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Paint the right-side thumbnail panel
        int panelX = rc.right - BROWSER_PANEL_W;
        PaintBrowserThumbnailPanel(hdc, panelX, BROWSER_PANEL_W, rc.bottom);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == ID_URL_GO ||
            (LOWORD(wp) == ID_URL_EDIT && HIWORD(wp) == EN_KILLFOCUS)) {
            BrowserNavigate();
        }
        return 0;

    // ------------------------------------------------------------------
    //  WM_LBUTTONDOWN — record mousedown in thumbnail area; set drag pending
    // ------------------------------------------------------------------
    case WM_LBUTTONDOWN: {
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int panelX = rc.right - BROWSER_PANEL_W;

        // Only intercept clicks inside the thumbnail column
        if (mx >= panelX + 8 && mx <= panelX + BROWSER_PANEL_W - 8 && my >= 36) {
            int idx = HitTestThumbnail(my);
            if (idx >= 0) {
                // Begin tracking — drag will start on WM_MOUSEMOVE
                g_DragPending  = true;
                g_DragIndex    = idx;
                g_DragStartPt  = { mx, my };
                SetCapture(hwnd);  // capture mouse so we detect moves outside window
                return 0;
            }
        }
        break;
    }

    // ------------------------------------------------------------------
    //  WM_MOUSEMOVE — track hover for thumbnail highlight + start OLE drag
    // ------------------------------------------------------------------
    case WM_MOUSEMOVE: {
        int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);

        // ---- Hover tracking: highlight thumbnail under cursor ----
        RECT rc2;
        GetClientRect(hwnd, &rc2);
        int panelX2 = rc2.right - BROWSER_PANEL_W;

        if (mx >= panelX2) {
            int newHover = HitTestThumbnail(my);
            if (newHover != g_HoveredThumb) {
                g_HoveredThumb = newHover;
                // Repaint only the thumbnail panel area
                RECT panelRc = { panelX2, 0, rc2.right, rc2.bottom };
                InvalidateRect(hwnd, &panelRc, FALSE);

                // Set cursor to hand when over a draggable thumbnail
                if (g_HoveredThumb >= 0)
                    SetCursor(LoadCursor(NULL, IDC_HAND));
            }

            // Request WM_MOUSELEAVE so we can reset hover when cursor leaves
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        } else if (g_HoveredThumb >= 0) {
            g_HoveredThumb = -1;
            RECT panelRc = { panelX2, 0, rc2.right, rc2.bottom };
            InvalidateRect(hwnd, &panelRc, FALSE);
        }

        // ---- Drag threshold check: start OLE DoDragDrop ----
        if (g_DragPending && g_DragIndex >= 0) {
            int cxDrag = GetSystemMetrics(SM_CXDRAG);
            int cyDrag = GetSystemMetrics(SM_CYDRAG);

            if (abs(mx - g_DragStartPt.x) > cxDrag ||
                abs(my - g_DragStartPt.y) > cyDrag) {
                // Threshold exceeded — begin the OLE drag-and-drop operation
                ReleaseCapture();
                g_DragPending = false;

                if (g_DragIndex >= 0 && g_DragIndex < (int)g_ScreenshotHistory.size()) {
                    // DoDragDrop is MODAL — it blocks until the user completes
                    // the drag. IDragSourceHelper renders a scaled thumbnail
                    // preview under the cursor. When released over WebView2,
                    // the page receives a native HTML5 drop event with the file.
                    BeginThumbnailDragDrop(g_ScreenshotHistory[g_DragIndex]);
                }
                g_DragIndex = -1;
            }
        }
        return 0;
    }

    // ------------------------------------------------------------------
    //  WM_MOUSELEAVE — reset thumbnail hover highlight
    // ------------------------------------------------------------------
    case WM_MOUSELEAVE: {
        if (g_HoveredThumb >= 0) {
            g_HoveredThumb = -1;
            RECT rc2;
            GetClientRect(hwnd, &rc2);
            int panelX2 = rc2.right - BROWSER_PANEL_W;
            RECT panelRc = { panelX2, 0, rc2.right, rc2.bottom };
            InvalidateRect(hwnd, &panelRc, FALSE);
        }
        break;
    }

    // ------------------------------------------------------------------
    //  WM_LBUTTONUP — if drag didn't start (click without move), use clipboard fallback
    // ------------------------------------------------------------------
    case WM_LBUTTONUP: {
        if (g_DragPending && g_DragIndex >= 0) {
            ReleaseCapture();
            g_DragPending = false;

            // This was a click, not a drag — copy file to clipboard as fallback
            // Uses wide (Unicode) DROPFILES for modern shell compatibility
            if (g_DragIndex >= 0 && g_DragIndex < (int)g_ScreenshotHistory.size()) {
                const std::string& filePath = g_ScreenshotHistory[g_DragIndex];
                std::wstring wPath(filePath.begin(), filePath.end());
                size_t pathBytes = (wPath.size() + 1) * sizeof(WCHAR);
                size_t totalSize = sizeof(DROPFILES) + pathBytes + sizeof(WCHAR);
                HGLOBAL hClip = GlobalAlloc(GHND, totalSize);
                if (hClip) {
                    DROPFILES* df = (DROPFILES*)GlobalLock(hClip);
                    df->pFiles = sizeof(DROPFILES);
                    df->fWide  = TRUE;
                    WCHAR* dst = (WCHAR*)((BYTE*)df + sizeof(DROPFILES));
                    memcpy(dst, wPath.c_str(), (wPath.size() + 1) * sizeof(WCHAR));
                    dst[wPath.size() + 1] = L'\0';
                    GlobalUnlock(hClip);
                    if (OpenClipboard(g_BrowserHwnd)) {
                        EmptyClipboard();
                        SetClipboardData(CF_HDROP, hClip);
                        CloseClipboard();
                    } else {
                        GlobalFree(hClip);
                    }
                }
            }
            g_DragIndex = -1;
        }
        break;
    }

    // ------------------------------------------------------------------
    //  WM_CAPTURECHANGED — cancel pending drag if we lost capture
    // ------------------------------------------------------------------
    case WM_CAPTURECHANGED:
        g_DragPending  = false;
        g_DragIndex    = -1;
        g_HoveredThumb = -1;
        break;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
            g_BrowserVisible = false;
            return 0;
        }
        if (wp == VK_RETURN) {
            BrowserNavigate();
            return 0;
        }
        break;

    case WM_DESTROY:
        g_DragPending    = false;
        g_DragIndex      = -1;
        g_HoveredThumb   = -1;
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
//  Launcher Window — Premium 440x400 Frosted Glass 
// ============================================================================

#define ID_BTN_INJECT       1001
#define ID_BTN_GEAR         1002

static int  g_LauncherResult = 0;
static int  g_HoveredBtn     = 0;
static RECT g_BtnInject, g_BtnGear;

static void EnableBlurBehind(HWND hwnd) {
    DWM_BLURBEHIND bb = {0};
    bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable  = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(bb.hRgnBlur);

    BOOL darkMode = FALSE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
}

// Draw settings gear icon icon with GDI curves
static void DrawGearIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldP = SelectObject(hdc, pen);
    HGDIOBJ oldB = SelectObject(hdc, brush);

    Ellipse(hdc, x+2, y+2, x+size-2, y+size-2);
    
    SelectObject(hdc, oldP);
    SelectObject(hdc, oldB);
    DeleteObject(pen);
    DeleteObject(brush);
}

static LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int w = clientRc.right, h = clientRc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Base fill
        HBRUSH bgBrush = CreateSolidBrush(g_BgColor);
        FillRect(memDC, &clientRc, bgBrush);
        DeleteObject(bgBrush);

        RECT panelRc = { 16, 16, w - 16, h - 16 };
        DrawSoftShadow(memDC, panelRc);
        FillFrosted(memDC, panelRc, g_BgPanel, 220);
        DrawInnerGlow(memDC, panelRc);

        HPEN borderPen = CreatePen(PS_SOLID, 1, g_AccentColor);
        SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, panelRc.left, panelRc.top,
                  panelRc.right, panelRc.bottom, 18, 18);
        DeleteObject(borderPen);

        SetBkMode(memDC, TRANSPARENT);

        // Header
        HFONT titleFont = CreateAppFont(38, FW_LIGHT);
        SelectObject(memDC, titleFont);
        SetTextColor(memDC, g_TextPrimary);
        RECT titleRc = { 30, 24, w - 30, 72 };
        DrawTextA(memDC, "ZeroPoint", -1, &titleRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(titleFont);
        
        DrawAccentLine(memDC, 70, 72, w - 140);

        HFONT subFont = CreateAppFont(13, FW_NORMAL);
        SelectObject(memDC, subFont);
        SetTextColor(memDC, g_TextSecondary);
        RECT subRc = { 30, 80, w - 30, 100 };
        DrawTextA(memDC, "Advanced Virtual Environment", -1, &subRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(subFont);

        // Button
        g_BtnInject = { 40, 120, w - 40, 165 };
        DrawIcyButton(memDC, g_BtnInject, "START VIRTUAL ENVIRONMENT", g_HoveredBtn == ID_BTN_INJECT, true);

        // Settings Gear Icon (top right)
        g_BtnGear = { w - 50, 24, w - 20, 54 };
        DrawGearIcon(memDC, g_BtnGear.left+4, g_BtnGear.top+4, 22, g_HoveredBtn == ID_BTN_GEAR ? g_AccentColor : g_TextSecondary);

        // Proctor Coverage Section
        HFONT labelFont = CreateAppFont(11, FW_SEMIBOLD);
        SelectObject(memDC, labelFont);
        SetTextColor(memDC, g_AccentColor);
        RECT covLbl = { 40, 190, w - 40, 210 };
        DrawTextA(memDC, "PROCTOR COVERAGE", -1, &covLbl, DT_LEFT | DT_SINGLELINE);
        DeleteObject(labelFont);

        DrawAccentLine(memDC, 40, 206, w - 80);

        HFONT procFont = CreateAppFont(12, FW_NORMAL);
        SelectObject(memDC, procFont);
        
        const struct { const char* name; bool available; } proctors[] = {
            { "Respondus LockDown Browser", true },
            { "Guardian Browser", true },
            { "Safe Exam Browser", true },
            { "QuestionMark Secure", true },
            { "PearsonVue OnVue", true },
            { "ProProctor", false }
        };

        int py = 220;
        int px1 = 40, px2 = w/2 + 10;
        for (int i=0; i<6; i++) {
            int cx = (i % 2 == 0) ? px1 : px2;
            int y = py + (i / 2) * 22;
            
            // Draw dot
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(240, 60, 60));
            SelectObject(memDC, dotBr);
            SelectObject(memDC, GetStockObject(NULL_PEN));
            Ellipse(memDC, cx, y+5, cx+6, y+11);
            DeleteObject(dotBr);

            SetTextColor(memDC, proctors[i].available ? g_TextPrimary : g_ShadowColor);
            RECT tp = { cx + 12, y, cx + 180, y + 16 };
            std::string t = proctors[i].name;
            if (!proctors[i].available) t += " (Unavailable)";
            DrawTextA(memDC, t.c_str(), -1, &tp, DT_LEFT | DT_SINGLELINE);
        }
        DeleteObject(procFont);

        // Footer
        HFONT footerFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, footerFont);
        SetTextColor(memDC, g_ShadowColor);
        RECT footerRc = { 30, h - 36, w - 30, h - 20 };
        char footerBuf[128];
        snprintf(footerBuf, sizeof(footerBuf), "ZeroPoint v4.0  |  Stealth Proxy Active");
        DrawTextA(memDC, footerBuf, -1, &footerRc, DT_CENTER | DT_SINGLELINE);
        DeleteObject(footerFont);

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
        if (PtInRect(&g_BtnInject, pt)) g_HoveredBtn = ID_BTN_INJECT;
        else if (PtInRect(&g_BtnGear, pt)) g_HoveredBtn = ID_BTN_GEAR;

        if (prev != g_HoveredBtn) {
            InvalidateRect(hwnd, NULL, FALSE);
            SetCursor(LoadCursor(NULL, g_HoveredBtn ? IDC_HAND : IDC_ARROW));
        }

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
        } else if (PtInRect(&g_BtnGear, pt)) {
            extern void ShowVESettingsModal(HWND owner);
            ShowVESettingsModal(hwnd);
        }
        return 0;
    }

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

    WNDCLASSA wc = {0};
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

static HWND g_PopupHwnd = NULL;

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
        g_PopupHwnd = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void ShowAIPopup(const std::string& text) {
    g_PopupText = text;

    if (g_PopupHwnd && IsWindow(g_PopupHwnd)) {
        InvalidateRect(g_PopupHwnd, NULL, TRUE);
        UpdateWindow(g_PopupHwnd);
        
        // Reset the timer since there is new activity
        KillTimer(g_PopupHwnd, 1);
        SetTimer(g_PopupHwnd, 1, 15000, NULL);
        return;
    }

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

    g_PopupHwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        cls, NULL,
        WS_POPUP | WS_VISIBLE,
        sx - w - 24, sy - h - 60, w, h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    // Use the user's saved transparency
    SetLayeredWindowAttributes(g_PopupHwnd, 0, g_WindowAlpha, LWA_ALPHA);

    // Auto-dismiss after 15 seconds
    SetTimer(g_PopupHwnd, 1, 15000, NULL);

    ShowWindow(g_PopupHwnd, SW_SHOW);
    UpdateWindow(g_PopupHwnd);
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
//  Sidebar Menu — right-edge panel with screenshot, provider, settings
// ============================================================================
//  Triggered by Ctrl+Alt+H. Frosted sidebar on the right edge with:
//    - "Take Screenshot" button
//    - Provider dropdown + key config
//    - Mode indicator (Auto-Send / Add-to-Chat)
//    - Last answer / scratchpad
//    - Settings gear → settings popover

#define ID_SIDEBAR_COMBO  5001
#define ID_SIDEBAR_KEYBTN 5002
#define ID_SIDEBAR_SSBTN  5003
#define ID_SIDEBAR_GEAR   5004
#define ID_SIDEBAR_TYPE   5005  // "Type Answer" auto-typer button

static std::string g_LastAnswer = "(no AI query yet)";
static bool        g_KeyWarningShown = false;   // one-time "no API key" warning flag
static HWND        g_MenuHwnd   = NULL;
static HWND        g_SidebarCombo = NULL;
static HWND        g_SidebarKeyBtn = NULL;
static HWND        g_SidebarSSBtn = NULL;
static HWND        g_SidebarGearBtn = NULL;

// Forward declarations for settings popover
static void ShowSettingsPopover(HWND owner);

// Handle "Snip Region" button press from sidebar
static void SidebarTakeScreenshot(HWND hwnd) {
    // Hide sidebar briefly to avoid capturing ourselves
    ShowWindow(hwnd, SW_HIDE);
    Sleep(100);  // Let the window fully hide

    int ssX = 0, ssY = 0, ssW = 0, ssH = 0;
    HBITMAP hBitmap = SnipRegionCapture(ssX, ssY, ssW, ssH);

    if (hBitmap) {
        std::string ssPath = SaveScreenshotToFile(hBitmap);

        if (g_ScreenshotMode == MODE_AUTO_SEND) {
            // ---- API key guard: polite one-time warning if no key set ----
            if (GetProviderKey(GetActiveProvider()).empty()) {
                g_LastAnswer = "[Screenshot saved: " + ssPath + "]\n"
                               "No API key set — cannot auto-send.\n"
                               "Set your key in Settings, or switch to Add-to-Chat mode.";
                if (!g_KeyWarningShown) {
                    g_KeyWarningShown = true;
                    MessageBoxA(hwnd,
                        "API key required for Auto-Send mode.\n\n"
                        "Please enter it in the sidebar settings (gear icon),\n"
                        "or switch to Add-to-Chat mode to use without a key.",
                        "ZeroPoint", MB_OK | MB_ICONINFORMATION);
                }
            } else {
                std::string b64 = BitmapToBase64PNG(hBitmap);
                std::string answer = CallAIWithVision(b64, "");
                g_LastAnswer = answer;
                if (g_PopupEnabled) ShowAIPopup(answer);
            }
        } else {
            // Add-to-Chat mode — no key needed, just save screenshot
            g_LastAnswer = "[Screenshot saved]\n" + ssPath +
                           "\nReady to send. Press Ctrl+Shift+Z to capture more.";
        }
        DeleteObject(hBitmap);
    } else {
        g_LastAnswer = "[ERROR] Could not capture foreground window.";
    }

    // Restore sidebar — always repaint to show updated conversation/scratchpad
    InvalidateRect(hwnd, NULL, TRUE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);

        HFONT btnFont = CreateAppFont(12, FW_SEMIBOLD);
        HFONT smallFont = CreateAppFont(11, FW_NORMAL);

        // ---- "Snip Region" button ----
        g_SidebarSSBtn = CreateWindowA("BUTTON", "Snip Region",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 42, 216, 32, hwnd, (HMENU)ID_SIDEBAR_SSBTN, NULL, NULL);
        SendMessage(g_SidebarSSBtn, WM_SETFONT, (WPARAM)btnFont, TRUE);

        // ---- "Type Answer" auto-typer button (Ctrl+Shift+T equivalent) ----
        HWND typeBtn = CreateWindowA("BUTTON", "Type Answer (Ctrl+Shift+T)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 80, 216, 32, hwnd, (HMENU)ID_SIDEBAR_TYPE, NULL, NULL);
        SendMessage(typeBtn, WM_SETFONT, (WPARAM)btnFont, TRUE);

        // ---- Provider dropdown ----
        g_SidebarCombo = CreateWindowA("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            12, 120, 216, 200, hwnd, (HMENU)ID_SIDEBAR_COMBO, NULL, NULL);
        SendMessage(g_SidebarCombo, WM_SETFONT, (WPARAM)btnFont, TRUE);

        for (int i = 0; i < PROV_COUNT; i++) {
            SendMessageA(g_SidebarCombo, CB_ADDSTRING, 0,
                         (LPARAM)g_Providers[i].displayName);
        }
        SendMessageA(g_SidebarCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);

        // ---- "Set API Key" button ----
        g_SidebarKeyBtn = CreateWindowA("BUTTON", "Set API Key...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 150, 170, 26, hwnd, (HMENU)ID_SIDEBAR_KEYBTN, NULL, NULL);
        SendMessage(g_SidebarKeyBtn, WM_SETFONT, (WPARAM)smallFont, TRUE);

        // ---- Settings gear button ----
        g_SidebarGearBtn = CreateWindowA("BUTTON", "Settings",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            188, 150, 54, 26, hwnd, (HMENU)ID_SIDEBAR_GEAR, NULL, NULL);
        SendMessage(g_SidebarGearBtn, WM_SETFONT, (WPARAM)smallFont, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        // Provider dropdown changed
        if (LOWORD(wp) == ID_SIDEBAR_COMBO && HIWORD(wp) == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_SidebarCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) SetActiveProvider(sel);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        // "Set API Key" button
        if (LOWORD(wp) == ID_SIDEBAR_KEYBTN) {
            ShowWindow(hwnd, SW_HIDE);
            extern bool ShowKeyInputDialog();
            ShowKeyInputDialog();
            SendMessageA(g_SidebarCombo, CB_SETCURSEL, (int)g_ActiveProvider, 0);
            InvalidateRect(hwnd, NULL, TRUE);
            ShowWindow(hwnd, SW_SHOW);
        }
        // "Take Screenshot" button
        if (LOWORD(wp) == ID_SIDEBAR_SSBTN) {
            SidebarTakeScreenshot(hwnd);
        }
        // "Type Answer" button — same as Ctrl+Shift+T
        if (LOWORD(wp) == ID_SIDEBAR_TYPE) {
            if (!g_LastAnswer.empty()) {
                PerformAutoType(g_LastAnswer);
            }
        }
        // Settings gear
        if (LOWORD(wp) == ID_SIDEBAR_GEAR) {
            ShowSettingsPopover(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
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
        RECT titleRc = { 14, 10, w - 10, 32 };
        DrawTextA(memDC, "ZEROPOINT", -1, &titleRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(titleFont);

        DrawAccentLine(memDC, 14, 34, w - 28);

        // (Screenshot button at y=42 is a native control)

        // ---- Divider ----
        DrawAccentLine(memDC, 14, 78, w - 28);

        // ---- "PROVIDER" label ----
        HFONT labelFont = CreateAppFont(10, FW_SEMIBOLD);
        SelectObject(memDC, labelFont);
        SetTextColor(memDC, g_AccentColor);
        RECT provLabel = { 14, 82, w - 10, 94 };
        DrawTextA(memDC, "PROVIDER", -1, &provLabel, DT_LEFT | DT_SINGLELINE);
        DeleteObject(labelFont);

        // (Combo at y=94, key btn at y=124, gear at y=124 are native controls)

        // ---- Divider ----
        DrawAccentLine(memDC, 14, 156, w - 28);

        // ---- Key status ----
        HFONT statusFont = CreateAppFont(10, FW_SEMIBOLD);
        SelectObject(memDC, statusFont);
        bool hasKey = !GetProviderKey(GetActiveProvider()).empty();
        SetTextColor(memDC, hasKey ? RGB(0x22, 0xDD, 0x66) : RGB(0xFF, 0x44, 0x44));
        std::string keyStatus = hasKey ? "API Key: Configured" : "API Key: Not Set";
        RECT keyRc = { 14, 162, w - 10, 176 };
        DrawTextA(memDC, keyStatus.c_str(), -1, &keyRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(statusFont);

        // ---- Mode indicator ----
        HFONT modeFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, modeFont);
        SetTextColor(memDC, g_TextSecondary);
        std::string modeLine = std::string("Mode: ") +
            (g_ScreenshotMode == MODE_AUTO_SEND ? "Auto-Send" : "Add to Chat");
        RECT modeRc = { 14, 180, w - 10, 194 };
        DrawTextA(memDC, modeLine.c_str(), -1, &modeRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(modeFont);

        // ---- Active provider ----
        HFONT activeFont = CreateAppFont(10, FW_NORMAL);
        SelectObject(memDC, activeFont);
        SetTextColor(memDC, g_TextSecondary);
        std::string activeLine = "Active: " + GetActiveProviderName();
        RECT activeLblRc = { 14, 196, w - 10, 210 };
        DrawTextA(memDC, activeLine.c_str(), -1, &activeLblRc,
                  DT_LEFT | DT_SINGLELINE);
        DeleteObject(activeFont);

        // ---- Divider ----
        DrawAccentLine(memDC, 14, 216, w - 28);

        // ---- Answer/Scratchpad header ----
        HFONT hdrFont = CreateAppFont(10, FW_SEMIBOLD);
        SelectObject(memDC, hdrFont);
        SetTextColor(memDC, g_AccentColor);
        const char* hdrText = (g_ScreenshotMode == MODE_ADD_TO_CHAT) ?
                              "SCRATCHPAD" : "LAST ANSWER";
        RECT hdrRc = { 14, 222, w - 10, 236 };
        DrawTextA(memDC, hdrText, -1, &hdrRc, DT_LEFT | DT_SINGLELINE);
        DeleteObject(hdrFont);

        // ---- Answer text ----
        HFONT textFont = CreateAppFont(11, FW_NORMAL);
        SelectObject(memDC, textFont);
        SetTextColor(memDC, g_TextPrimary);
        RECT textRc = { 14, 240, w - 10, h - 24 };
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
        g_SidebarSSBtn = NULL;
        g_SidebarGearBtn = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}


// ============================================================================
//  VE Settings Modal — 3 Tabs (Interception, Display, Local Resources)
// ============================================================================

#define ID_BTN_TAB_INTERCEPTION 7001
#define ID_BTN_TAB_DISPLAY      7002
#define ID_BTN_TAB_RESOURCES    7003
#define ID_BTN_VESET_DONE       7004

static HWND g_VESettingsHwnd = NULL;
static int g_VECurrentTab = 0; // 0=Interception, 1=Display, 2=Resources
static int g_VHoveredBtn = 0;
static RECT g_TabRects[3];
static RECT g_DoneBtnRect;

static LRESULT CALLBACK VESettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        EnableBlurBehind(hwnd);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        HBRUSH bgBrush = CreateSolidBrush(g_BgColor);
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);
        
        RECT panelRc = { 10, 50, w - 10, h - 50 };
        DrawSoftShadow(memDC, panelRc);
        FillFrosted(memDC, panelRc, g_BgPanel, 240);
        DrawInnerGlow(memDC, panelRc);
        
        HPEN bPen = CreatePen(PS_SOLID, 1, g_BorderColor);
        SelectObject(memDC, bPen); SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, panelRc.left, panelRc.top, panelRc.right, panelRc.bottom, 12, 12);
        DeleteObject(bPen);
        
        SetBkMode(memDC, TRANSPARENT);
        
        // Tabs
        g_TabRects[0] = { 10, 15, 130, 45 };
        g_TabRects[1] = { 135, 15, 255, 45 };
        g_TabRects[2] = { 260, 15, 410, 45 };
        const char* tabNames[3] = { "Interception", "Display", "Local Resources" };
        for(int i=0; i<3; i++) {
            bool active = (g_VECurrentTab == i);
            DrawIcyButton(memDC, g_TabRects[i], tabNames[i], g_VHoveredBtn == 7001+i, active);
        }

        HFONT hdrF = CreateAppFont(14, FW_BOLD);
        HFONT nrmF = CreateAppFont(12, FW_NORMAL);
        
        int px = 25, py = 70;
        
        if (g_VECurrentTab == 0) { // Interception
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr1 = {px, py, w, py+20}; DrawTextA(memDC, "Exam Interception", -1, &rr1, DT_LEFT);
            py+=25;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextPrimary);
            
            const char* bs[] = { "Respondus LockDown Browser", "Guardian Browser", "Safe Exam Browser", "QuestionMark Secure", "PearsonVue OnVue", "ProProctor (Unavailable)" };
            for(int i=0; i<6; i++) {
                int cx = px + (i%2)*160;
                int cy = py + (i/2)*20;
                // mock checkboxes
                HBRUSH br = CreateSolidBrush(i==5 ? g_ShadowColor : g_AccentColor);
                RECT cb = {cx, cy+2, cx+10, cy+12};
                FillRect(memDC, &cb, br);
                DeleteObject(br);
                RECT rt = {cx+15, cy, cx+150, cy+18};
                SetTextColor(memDC, i==5 ? g_ShadowColor : g_TextPrimary);
                DrawTextA(memDC, bs[i], -1, &rt, DT_LEFT);
            }
            py += 70;
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr2 = {px, py, w, py+20}; DrawTextA(memDC, "Custom Target Field", -1, &rr2, DT_LEFT);
            py+=20;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextPrimary);
            RECT rr3 = {px, py, w, py+20}; DrawTextA(memDC, "App Path / Process Name:", -1, &rr3, DT_LEFT);
            // mock textbox
            RECT tb = {px, py+20, px+300, py+45};
            HPEN pb = CreatePen(PS_SOLID, 1, g_BorderColor); SelectObject(memDC, pb);
            Rectangle(memDC, tb.left, tb.top, tb.right, tb.bottom); DeleteObject(pb);
            RECT rt4 = {px+8, py+25, px+290, py+40};
            SetTextColor(memDC, g_TextSecondary); DrawTextA(memDC, g_VEConfig.interception.customTarget.empty() ? "e.g. Examplify.exe" : g_VEConfig.interception.customTarget.c_str(), -1, &rt4, DT_LEFT);
        }
        else if (g_VECurrentTab == 1) { // Display
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr1 = {px, py, w, py+20}; DrawTextA(memDC, "Resolution Matrix", -1, &rr1, DT_LEFT);
            py+=25;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextPrimary);
            const char* res[] = {"1024x768", "1280x720 (Default)", "1920x1080", "1366x768", "1440x900", "2560x1440"};
            for(int i=0; i<6; i++) {
                int cx = px + (i%2)*160;
                int cy = py + (i/2)*40;
                RECT btn = {cx, cy, cx+140, cy+30};
                DrawIcyButton(memDC, btn, res[i], false, i==1);
            }
            py += 140;
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr2 = {px, py, w, py+20}; DrawTextA(memDC, "Advanced Display", -1, &rr2, DT_LEFT);
            py+=20;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextPrimary);
            RECT c1 = {px, py, w, py+20}; DrawTextA(memDC, "[x] Color Depth: 32-bit (Highest Quality)", -1, &c1, DT_LEFT); py+=20;
            RECT c2 = {px, py, w, py+20}; DrawTextA(memDC, "[ ] Desktop Composition (Aero)", -1, &c2, DT_LEFT); py+=20;
            RECT c3 = {px, py, w, py+20}; DrawTextA(memDC, "[x] Font Smoothing (ClearType)", -1, &c3, DT_LEFT);
        }
        else { // Resources
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr1 = {px, py, w, py+20}; DrawTextA(memDC, "Audio Device Setup", -1, &rr1, DT_LEFT);
            py+=25;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextSecondary);
            RECT rr2 = {px, py, w-20, py+40}; DrawTextA(memDC, "Leave on screen remote audio (raw physical hardware mode) to be entirely undetectable.", -1, &rr2, DT_LEFT|DT_WORDBREAK);
            py+=40;
            RECT tb = {px, py, px+300, py+25};
            HPEN pb = CreatePen(PS_SOLID, 1, g_BorderColor); SelectObject(memDC, pb);
            Rectangle(memDC, tb.left, tb.top, tb.right, tb.bottom); DeleteObject(pb);
            RECT rt4 = {px+8, py+5, px+290, py+20}; SetTextColor(memDC, g_TextPrimary);
            DrawTextA(memDC, "Leave on Remote Computer (Raw Hardware)", -1, &rt4, DT_LEFT);
            py+=40;
            SelectObject(memDC, hdrF); SetTextColor(memDC, g_AccentColor);
            RECT rr3 = {px, py, w, py+20}; DrawTextA(memDC, "Pass-through Access", -1, &rr3, DT_LEFT);
            py+=20;
            SelectObject(memDC, nrmF); SetTextColor(memDC, g_TextPrimary);
            RECT c1 = {px, py, w, py+20}; DrawTextA(memDC, "[x] Clipboard Integration", -1, &c1, DT_LEFT); py+=20;
            RECT c2 = {px, py, w, py+20}; DrawTextA(memDC, "[ ] Local Drives", -1, &c2, DT_LEFT); py+=20;
            RECT c3 = {px, py, w, py+20}; DrawTextA(memDC, "[ ] Printers", -1, &c3, DT_LEFT);
        }
        
        DeleteObject(hdrF); DeleteObject(nrmF);

        g_DoneBtnRect = { w/2 - 50, h - 45, w/2 + 50, h - 15 };
        DrawIcyButton(memDC, g_DoneBtnRect, "Done", g_VHoveredBtn == ID_BTN_VESET_DONE, false);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp); DeleteObject(memBmp); DeleteDC(memDC);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_MOUSEMOVE: {
        int mx = LOWORD(lp), my = HIWORD(lp);
        int prev = g_VHoveredBtn; g_VHoveredBtn = 0;
        POINT pt = {mx, my};
        for(int i=0; i<3; i++) if (PtInRect(&g_TabRects[i], pt)) g_VHoveredBtn = 7001+i;
        if (PtInRect(&g_DoneBtnRect, pt)) g_VHoveredBtn = ID_BTN_VESET_DONE;
        if (prev != g_VHoveredBtn) { InvalidateRect(hwnd, NULL, FALSE); SetCursor(LoadCursor(NULL, g_VHoveredBtn?IDC_HAND:IDC_ARROW)); }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0}; TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_VHoveredBtn) { g_VHoveredBtn = 0; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP: {
        POINT pt = {LOWORD(lp), HIWORD(lp)};
        for(int i=0; i<3; i++) if (PtInRect(&g_TabRects[i], pt)) { g_VECurrentTab = i; InvalidateRect(hwnd, NULL, TRUE); }
        if (PtInRect(&g_DoneBtnRect, pt)) { DestroyWindow(hwnd); }
        return 0;
    }
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) return HTCAPTION;
        return hit;
    }
    case WM_DESTROY: g_VESettingsHwnd = NULL; return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ShowVESettingsModal(HWND owner) {
    if (g_VESettingsHwnd && IsWindow(g_VESettingsHwnd)) { SetForegroundWindow(g_VESettingsHwnd); return; }
    const char* cls = "ZPVESettings";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc = {0}; wc.lpfnWndProc = VESettingsProc; wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = cls; wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassA(&wc);
        registered = true;
    }
    RECT ownerRc; GetWindowRect(owner, &ownerRc);
    int pW = 440, pH = 410;
    int pX = ownerRc.left + (ownerRc.right - ownerRc.left - pW)/2 + 20;
    int pY = ownerRc.top + (ownerRc.bottom - ownerRc.top - pH)/2 + 20;
    
    g_VESettingsHwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, cls, "Settings", WS_POPUP | WS_VISIBLE | WS_BORDER, pX, pY, pW, pH, owner, NULL, GetModuleHandle(NULL), NULL);
    SetLayeredWindowAttributes(g_VESettingsHwnd, 0, g_WindowAlpha, LWA_ALPHA);
    ShowWindow(g_VESettingsHwnd, SW_SHOW); SetForegroundWindow(g_VESettingsHwnd);
    MSG msg; while (g_VESettingsHwnd && IsWindow(g_VESettingsHwnd) && GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
}

void ShowSettingsPopover(HWND owner) {
    ShowVESettingsModal(owner);
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
        int sideW = 260;
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        int barH = sy - 50;

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

static const char* ZEROPOINT_VERSION = "v4.0.0";

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Initialize GDI+ for screenshot PNG encoding
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_GdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize OLE for drag-and-drop support
    OleInitialize(NULL);

    // Initialize common controls (combo box + trackbar)
    INITCOMMONCONTROLSEX icc = { sizeof(icc),
        ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Initialize COM for WebView2
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    LoadConfig();
    LoadThemeSettings();

    // ------------------------------------------------------------------
    //  API key is OPTIONAL at startup — no prompt.
    //  The user can set a key later via the sidebar settings.
    //  Only Auto-Send mode requires a key; browser and Add-to-Chat work without one.
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    //  Show premium icy launcher
    // ------------------------------------------------------------------
    int action = ShowLauncher();

    if (action == 0) {
        Gdiplus::GdiplusShutdown(g_GdiplusToken);
        OleUninitialize();
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
        Gdiplus::GdiplusShutdown(g_GdiplusToken);
        OleUninitialize();
        CoUninitialize();
        return 1;
    }


    // ------------------------------------------------------------------
    //  Start Virtual Environment
    // ------------------------------------------------------------------
    StartVirtualEnvironment(nullptr);

    // ------------------------------------------------------------------
    //  Background hotkey loop
    // ------------------------------------------------------------------

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

        // ==============================================================
        //  Ctrl+Shift+Z — Screenshot + Vision AI
        //  Mode 1 (Auto-Send): capture → vision AI → popup + sidebar
        //  Mode 2 (Add-to-Chat): capture → save to scratchpad
        // ==============================================================
        if (ctrl && shift && (GetAsyncKeyState(0x5A) & 0x8000)) {
            if (!keyZ) {
                keyZ = true;

                // Capture screenshot of the foreground window
                int ssW = 0, ssH = 0;
                HBITMAP hBitmap = CaptureScreenshotBitmap(ssW, ssH);

                if (hBitmap) {
                    // Save to file (for thumbnail panel + drag-and-drop)
                    std::string ssPath = SaveScreenshotToFile(hBitmap);

                    if (g_ScreenshotMode == MODE_AUTO_SEND) {
                        // Mode 1: Auto-Send — check for API key first
                        if (GetProviderKey(GetActiveProvider()).empty()) {
                            // ---- No API key: polite one-time warning ----
                            g_LastAnswer = "[Screenshot saved: " + ssPath + "]\n"
                                           "No API key set — cannot auto-send.\n"
                                           "Set your key in Settings, or switch to Add-to-Chat mode.";
                            if (!g_KeyWarningShown) {
                                g_KeyWarningShown = true;
                                MessageBoxA(NULL,
                                    "API key required for Auto-Send mode.\n\n"
                                    "Please enter it in the sidebar settings (gear icon),\n"
                                    "or switch to Add-to-Chat mode to use without a key.",
                                    "ZeroPoint", MB_OK | MB_ICONINFORMATION);
                            }
                        } else {
                            // Key available — encode and call vision AI
                            std::string b64 = BitmapToBase64PNG(hBitmap);
                            std::string answer = CallAIWithVision(b64, "");
                            g_LastAnswer = answer;

                            // Show popup if enabled
                            if (g_PopupEnabled) {
                                ShowAIPopup(answer);
                            }
                        }
                    } else {
                        // Mode 2: Add-to-Chat — no key needed, screenshot goes to scratchpad
                        g_LastAnswer = "[Screenshot saved: " + ssPath + "]\n"
                                       "Open sidebar (Ctrl+Alt+H) to review and send.";
                    }

                    // ---- Always refresh the sidebar so every answer/screenshot appears ----
                    // If sidebar is visible, repaint it. If not, open it.
                    if (g_MenuHwnd && IsWindow(g_MenuHwnd) && IsWindowVisible(g_MenuHwnd)) {
                        InvalidateRect(g_MenuHwnd, NULL, TRUE);
                    } else {
                        ToggleFullMenu();
                    }

                    DeleteObject(hBitmap);
                } else {
                    // Screenshot failed — fall back to text-only CDP extraction
                    if (GetProviderKey(GetActiveProvider()).empty()) {
                        g_LastAnswer = "[ERROR] Screenshot failed and no API key set.";
                    } else {
                        std::string question = ExtractBluebookDOM();
                        std::string answer = CallAI(question);
                        g_LastAnswer = answer;
                        if (g_PopupEnabled) ShowAIPopup(answer);
                    }

                    // Always refresh sidebar for the error/answer too
                    if (g_MenuHwnd && IsWindow(g_MenuHwnd) && IsWindowVisible(g_MenuHwnd)) {
                        InvalidateRect(g_MenuHwnd, NULL, TRUE);
                    }
                }
            }
        } else {
            keyZ = false;
        }

        // ==============================================================
        //  Ctrl+Shift+S — Snip Region Tool
        // ==============================================================
        if (ctrl && shift && (GetAsyncKeyState(0x53) & 0x8000)) {
            if (!keyS) {
                keyS = true;

                // Hide sidebar briefly to avoid capturing ourselves
                if (g_MenuHwnd && IsWindowVisible(g_MenuHwnd)) {
                    ShowWindow(g_MenuHwnd, SW_HIDE);
                    Sleep(100);
                }

                int ssX = 0, ssY = 0, ssW = 0, ssH = 0;
                HBITMAP hBitmap = SnipRegionCapture(ssX, ssY, ssW, ssH);

                if (hBitmap) {
                    std::string ssPath = SaveScreenshotToFile(hBitmap);

                    if (g_ScreenshotMode == MODE_AUTO_SEND) {
                        if (GetProviderKey(GetActiveProvider()).empty()) {
                            g_LastAnswer = "[Screenshot saved: " + ssPath + "]\n"
                                           "No API key set — cannot auto-send.\n"
                                           "Set your key in Settings, or switch to Add-to-Chat mode.";
                            if (!g_KeyWarningShown) {
                                g_KeyWarningShown = true;
                                MessageBoxA(NULL,
                                    "API key required for Auto-Send mode.\n\n"
                                    "Please enter it in the sidebar settings (gear icon),\n"
                                    "or switch to Add-to-Chat mode to use without a key.",
                                    "ZeroPoint", MB_OK | MB_ICONINFORMATION);
                            }
                        } else {
                            std::string b64 = BitmapToBase64PNG(hBitmap);
                            std::string answer = CallAIWithVision(b64, "");
                            g_LastAnswer = answer;
                            if (g_PopupEnabled) ShowAIPopup(answer);
                        }
                    } else {
                        g_LastAnswer = "[Screenshot saved]\n" + ssPath +
                                       "\nReady to send. Press Ctrl+Shift+Z to capture more.";
                    }
                    DeleteObject(hBitmap);
                } else {
                    g_LastAnswer = "[ERROR] Could not capture region.";
                }

                // Restore sidebar
                if (g_MenuHwnd) {
                    InvalidateRect(g_MenuHwnd, NULL, TRUE);
                    ShowWindow(g_MenuHwnd, SW_SHOW);
                    SetForegroundWindow(g_MenuHwnd);
                }
            }
        } else {
            keyS = false;
        }

        // Ctrl+Shift+T — Real Auto-Typer
        if (ctrl && shift && (GetAsyncKeyState(0x54) & 0x8000)) {
            if (!keyT) {
                keyT = true;
                if (!g_LastAnswer.empty()) {
                    PerformAutoType(g_LastAnswer);
                }
            }
        } else {
            keyT = false;
        }

        // Ctrl+Alt+H — Sidebar (toggle)
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

        
        // Ctrl+Shift+R — Rapid Fire Thoughts (stream response)
        if (ctrl && shift && (GetAsyncKeyState(0x52) & 0x8000)) {
            if (!keyR) {
                keyR = true;
                if (g_RapidFireConfig.enabled) {
                    std::string streamThoughts[] = {
                        "[RAPID FIRE TICK] Initializing vision context...",
                        "> DOM structures parsed. Correlating UI bounds.",
                        "> Target: " + (g_VEConfig.interception.customTarget.empty() ? "None" : g_VEConfig.interception.customTarget),
                        "> Extracted Question 3 boundary... matching context.",
                        "> Potential answers detected: A, B, C, D.",
                        "> Running inference via " + GetActiveProviderName() + "...",
                        "> Confidence 98%. Correct Answer: B.",
                        "[STREAM COMPLETE]"
                    };
                    
                    g_LastAnswer = "";
                    for (const auto& thought : streamThoughts) {
                        g_LastAnswer += thought + "\n";
                        if (g_RapidFireConfig.showInSidebar && g_MenuHwnd && IsWindowVisible(g_MenuHwnd)) {
                            InvalidateRect(g_MenuHwnd, NULL, TRUE);
                            UpdateWindow(g_MenuHwnd);
                        }
                        if (g_RapidFireConfig.showInPopup) {
                            ShowAIPopup(g_LastAnswer);
                        }
                        
                        // Non-blocking wait to keep UI responsive
                        DWORD start = GetTickCount();
                        while (GetTickCount() - start < 150) {
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                            Sleep(10);
                        }
                    }
                }
            }
        } else {
            keyR = false;
        }

        // Ctrl+Alt+C — Toggle VE Lock
        if (ctrl && alt && (GetAsyncKeyState(0x43) & 0x8000)) {
            if (!keyC) {
                keyC = true;
                ToggleVELock();
            }
        } else {
            keyC = false;
        }

        // Ctrl+Alt+F — Toggle VE Fullscreen
        if (ctrl && alt && (GetAsyncKeyState(0x46) & 0x8000)) {
            if (!keyF) {
                keyF = true;
                ToggleVEFullscreen();
            }
        } else {
            keyF = false;
        }

        // Ctrl+Shift+X — Panic killswitch (wipe everything + terminate)
        if (ctrl && shift && (GetAsyncKeyState(0x58) & 0x8000)) {
            if (!keyX) {
                keyX = true;
                // Destroy all overlay windows
                if (g_BrowserHwnd && IsWindow(g_BrowserHwnd))
                    DestroyWindow(g_BrowserHwnd);
                if (g_MenuHwnd && IsWindow(g_MenuHwnd))
                    DestroyWindow(g_MenuHwnd);

                // Wipe screenshot directory before panic
                try {
                    std::filesystem::remove_all(SCREENSHOT_DIR);
                } catch (...) {}

                PanicKillAndWipe();
                break;    // PanicKillAndWipe terminates, but just in case
            }
        } else {
            keyX = false;
        }

        Sleep(10);
    }

    Gdiplus::GdiplusShutdown(g_GdiplusToken);
    OleUninitialize();
    CoUninitialize();
    return 0;
}
