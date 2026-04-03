import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

settings_proc_replacement = """
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
"""

start_str = "// ============================================================================\n//  Settings Popover — compact modal spawned from sidebar gear icon\n// ============================================================================\n"
end_str = "// Toggle sidebar visibility\n"

start_idx = content.find(start_str)
end_idx = content.find(end_str)

if start_idx != -1 and end_idx != -1:
    new_content = content[:start_idx] + settings_proc_replacement + "\n" + content[end_idx:]
    with open('src/main.cpp', 'w') as f:
        f.write(new_content)
    print("Patched Settings Popover")
else:
    print("Could not find bounds for Settings Popover")
