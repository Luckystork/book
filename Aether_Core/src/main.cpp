#include "Stealth.h"
#include "CDPExtractor.h"
#include <windows.h>
#include <string>
#include <iostream>

// Force Windows GUI subsystem for absolute stealth (no console)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#define BTN_INJECT 1001
#define BTN_EXIT 1002
#define BTN_SETTINGS 1003
#define BTN_SAVE 1004
#define EDIT_API 2001
#define CHK_CDP 2002
#define CHK_OCR 2003

enum ViewState { VIEW_DASHBOARD, VIEW_SETTINGS };
ViewState g_View = VIEW_DASHBOARD;
bool g_Injected = false;
HWND g_hWnd = NULL;

HWND g_hBtnInject = NULL;
HWND g_hBtnExit = NULL;
HWND g_hBtnSettings = NULL;

// Settings Controls
HWND g_hBtnSave = NULL;
HWND g_hEditAPI = NULL;
HWND g_hChkCDP = NULL;
HWND g_hChkOCR = NULL;

HFONT g_hFontTitle = NULL;
HFONT g_hFontSubtitle = NULL;
HFONT g_hFontBtn = NULL;

void SwitchView(HWND hwnd, ViewState newView) {
    g_View = newView;
    if (newView == VIEW_DASHBOARD) {
        ShowWindow(g_hBtnInject, SW_SHOW);
        ShowWindow(g_hBtnSettings, SW_SHOW);
        
        ShowWindow(g_hBtnSave, SW_HIDE);
        ShowWindow(g_hEditAPI, SW_HIDE);
        ShowWindow(g_hChkCDP, SW_HIDE);
        ShowWindow(g_hChkOCR, SW_HIDE);
    } else {
        ShowWindow(g_hBtnInject, SW_HIDE);
        ShowWindow(g_hBtnSettings, SW_HIDE);
        
        ShowWindow(g_hBtnSave, SW_SHOW);
        ShowWindow(g_hEditAPI, SW_SHOW);
        ShowWindow(g_hChkCDP, SW_SHOW);
        ShowWindow(g_hChkOCR, SW_SHOW);
        
        // Simulate reading config
        SetWindowTextA(g_hEditAPI, "sk-ant-api03-..."); 
        SendMessage(g_hChkCDP, BM_SETCHECK, BST_CHECKED, 0); // Default to Bluebook
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CALLBACK LauncherProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Inter");
            g_hFontSubtitle = CreateFontA(14, 0, 0, 0, FW_DEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Inter");
            g_hFontBtn = CreateFontA(14, 0, 0, 0, FW_DEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Inter");

            // Dashboard Controls
            g_hBtnInject = CreateWindowA("BUTTON", "INITIALIZE GHOST", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT, 
                          125, 110, 150, 35, hwnd, (HMENU)BTN_INJECT, GetModuleHandle(NULL), NULL);
            g_hBtnSettings = CreateWindowA("BUTTON", "SETTINGS", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT, 
                          125, 155, 150, 25, hwnd, (HMENU)BTN_SETTINGS, GetModuleHandle(NULL), NULL);
            
            // Settings Controls (Hidden initially)
            g_hEditAPI = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                          50, 100, 300, 25, hwnd, (HMENU)EDIT_API, GetModuleHandle(NULL), NULL);
            
            g_hChkCDP = CreateWindowA("BUTTON", "Bluebook Engine (CDP WebSocket)", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
                          50, 135, 250, 20, hwnd, (HMENU)CHK_CDP, GetModuleHandle(NULL), NULL);
            g_hChkOCR = CreateWindowA("BUTTON", "Respondus Engine (OCR Capture)", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 
                          50, 160, 250, 20, hwnd, (HMENU)CHK_OCR, GetModuleHandle(NULL), NULL);
                          
            g_hBtnSave = CreateWindowA("BUTTON", "SAVE & RETURN", WS_CHILD | WS_VISIBLE | BS_FLAT, 
                          125, 195, 150, 30, hwnd, (HMENU)BTN_SAVE, GetModuleHandle(NULL), NULL);
            
            // Global Exit
            g_hBtnExit = CreateWindowA("BUTTON", "X", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT, 
                          370, 10, 20, 20, hwnd, (HMENU)BTN_EXIT, GetModuleHandle(NULL), NULL);
            
            SendMessage(g_hBtnInject, WM_SETFONT, (WPARAM)g_hFontBtn, TRUE);
            SendMessage(g_hBtnSettings, WM_SETFONT, (WPARAM)g_hFontBtn, TRUE);
            SendMessage(g_hBtnSave, WM_SETFONT, (WPARAM)g_hFontBtn, TRUE);
            SendMessage(g_hBtnExit, WM_SETFONT, (WPARAM)g_hFontBtn, TRUE);
            SendMessage(g_hEditAPI, WM_SETFONT, (WPARAM)g_hFontSubtitle, TRUE);
            SendMessage(g_hChkCDP, WM_SETFONT, (WPARAM)g_hFontSubtitle, TRUE);
            SendMessage(g_hChkOCR, WM_SETFONT, (WPARAM)g_hFontSubtitle, TRUE);
            
            SwitchView(hwnd, VIEW_DASHBOARD);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Winter Glass Pure Snow Background
            HBRUSH hbg = CreateSolidBrush(RGB(248, 251, 255));
            FillRect(hdc, &ps.rcPaint, hbg);
            DeleteObject(hbg);
            
            SetBkMode(hdc, TRANSPARENT);
            
            if (g_View == VIEW_DASHBOARD) {
                SetTextColor(hdc, RGB(18, 32, 53)); // Dark charcoal
                SelectObject(hdc, g_hFontTitle);
                TextOutA(hdc, 132, 35, "ZeroPoint", 9);
                SetTextColor(hdc, RGB(80, 180, 240)); // Icy frost
                SelectObject(hdc, g_hFontSubtitle);
                TextOutA(hdc, 145, 68, "WINTER GLASS", 12);
            } else if (g_View == VIEW_SETTINGS) {
                SetTextColor(hdc, RGB(18, 32, 53));
                SelectObject(hdc, g_hFontTitle);
                TextOutA(hdc, 135, 25, "Configure", 9);
                SetTextColor(hdc, RGB(100, 100, 110)); 
                SelectObject(hdc, g_hFontSubtitle);
                TextOutA(hdc, 50, 80, "Anthropic API Key:", 18);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            // Make radio buttons and text transparent to the white background
            SetBkMode((HDC)wParam, TRANSPARENT);
            SetTextColor((HDC)wParam, RGB(18, 32, 53)); 
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
            if (hit == HTCLIENT) return HTCAPTION;
            return hit;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == BTN_EXIT) {
                PostQuitMessage(0);
            }
            if (LOWORD(wParam) == BTN_SETTINGS) {
                SwitchView(hwnd, VIEW_SETTINGS);
            }
            if (LOWORD(wParam) == BTN_SAVE) {
                char buffer[256];
                GetWindowTextA(g_hEditAPI, buffer, 256);
                // bool useCDP = (SendMessage(g_hChkCDP, BM_GETCHECK, 0, 0) == BST_CHECKED);
                // Config::SaveSettings(buffer, useCDP); 
                SwitchView(hwnd, VIEW_DASHBOARD);
            }
            if (LOWORD(wParam) == BTN_INJECT) {
                if (!g_Injected) {
                    if (PerformHollowing()) {
                        g_Injected = true;
                        ShowWindow(hwnd, SW_HIDE);
                    } else {
                        MessageBoxA(hwnd, "Initialization failed. Check payload offsets.", "ZeroPoint Error", MB_OK | MB_ICONERROR);
                    }
                }
            }
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = LauncherProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "ZPLauncherClass";
    RegisterClassA(&wc);

    int w = 400;
    int h = 250; // Slightly taller for settings
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowExA(0, wc.lpszClassName, "ZeroPoint", WS_POPUP | WS_VISIBLE, 
        (sw - w) / 2, (sh - h) / 2, w, h, NULL, NULL, wc.hInstance, NULL);

    MSG msg = { 0 };
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_Injected) {
            // Ctrl+Shift+Z = Snapshot
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState(0x5A) & 0x8000)) { 
                std::string question = ExtractBluebookDOM();
            }
            // Ctrl+Alt+H = Full Menu
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(0x48) & 0x8000)) { 
                // ToggleFullMenu();
            }
            // Ctrl+Shift+X = Panic
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState(0x58) & 0x8000)) { 
                break; 
            }
        }
        Sleep(10);
    }
    
    return 0;
}