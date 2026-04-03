import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

launcher_proc_replacement = """
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
            HBRUSH dotBr = CreateSolidBrush(proctors[i].available ? RGB(0, 200, 100) : RGB(200, 100, 100));
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
"""

start_str = "// ============================================================================\n//  Launcher Window — icy frosted glass WNDPROC with full double-buffered paint\n// ============================================================================\n"
end_str = "// ============================================================================\n//  Enhanced Frosted AI Answer Popup — icy/snowy theme\n// ============================================================================\n"

start_idx = content.find(start_str)
end_idx = content.find(end_str)

if start_idx != -1 and end_idx != -1:
    new_content = content[:start_idx] + launcher_proc_replacement + "\n" + content[end_idx:]
    with open('src/main.cpp', 'w') as f:
        f.write(new_content)
    print("Patched Launcher")
else:
    print("Could not find bounds for Launcher")

