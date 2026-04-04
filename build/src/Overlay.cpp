#include "Overlay.h"
#include "Stealth.h"
#include <iostream>

namespace UI {
    Overlay* g_Overlay = nullptr;

    LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_PAINT) {
            if (g_Overlay) g_Overlay->Render();
            ValidateRect(hwnd, NULL);
            return 0;
        }
        if (uMsg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        // Passthrough click events via HTTRANSPARENT to allow click-through if Layer 2 is hidden
        if (uMsg == WM_NCHITTEST) {
            if (g_Overlay && !g_Overlay->m_showLayer2) {
                return HTTRANSPARENT; 
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    Overlay::Overlay() : m_hwnd(NULL), m_pD2DFactory(NULL), m_pRenderTarget(NULL), 
                         m_pDWriteFactory(NULL), m_pTextFormat(NULL), 
                         m_pBrushText(NULL), m_pBrushFrost(NULL),
                         m_pWICFactory(NULL), m_pLogoBitmap(NULL),
                         m_layer1Text(""), m_showLayer2(false) {
        g_Overlay = this;
    }

    Overlay::~Overlay() {
        CleanupDirect2D();
        if (m_hwnd) DestroyWindow(m_hwnd);
    }

    bool Overlay::CreateTransparentWindow(HINSTANCE hInstance) {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = "ZeroPointWinterGlass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wc);

        // Create boundaryless, transparent, topmost window
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        m_hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
            wc.lpszClassName, "ZP",
            WS_POPUP,
            0, 0, screenWidth, screenHeight,
            NULL, NULL, hInstance, NULL
        );

        if (!m_hwnd) return false;

        // Make window background fully transparent/click-through
        SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);

        // VERY IMPORTANT: Hide this specific window from screenshares and proctors!
        Stealth::CloakWindow(m_hwnd);

        if (!InitializeDirect2D()) return false;

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        return true;
    }

    bool Overlay::InitializeDirect2D() {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory))) return false;
        
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        if (FAILED(m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            &m_pRenderTarget))) return false;

        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.12f, 0.17f, 1.0f), &m_pBrushText); // Clean dark text for icy theme
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.8f), &m_pBrushFrost); // Winter Glass Frosted White/Icy

        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory)))) return false;
        
        m_pDWriteFactory->CreateTextFormat(
            L"Inter", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &m_pTextFormat
        );

        // Initialize WIC to load the PNG logo
        CoInitialize(NULL);
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWICFactory)))) {
            IWICBitmapDecoder* pDecoder = NULL;
            // Assumes the transparent PNG logo is dropped into the same directory as the executable
            if (SUCCEEDED(m_pWICFactory->CreateDecoderFromFilename(L"zeropoint_logo_transparent.png", NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder))) {
                IWICBitmapFrameDecode* pSource = NULL;
                if (SUCCEEDED(pDecoder->GetFrame(0, &pSource))) {
                    IWICFormatConverter* pConverter = NULL;
                    if (SUCCEEDED(m_pWICFactory->CreateFormatConverter(&pConverter))) {
                        pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);
                        m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &m_pLogoBitmap);
                        pConverter->Release();
                    }
                    pSource->Release();
                }
                pDecoder->Release();
            }
        }

        return true;
    }

    void Overlay::CleanupDirect2D() {
        if (m_pLogoBitmap) m_pLogoBitmap->Release();
        if (m_pWICFactory) m_pWICFactory->Release();
        if (m_pBrushText) m_pBrushText->Release();
        if (m_pBrushFrost) m_pBrushFrost->Release();
        if (m_pTextFormat) m_pTextFormat->Release();
        if (m_pDWriteFactory) m_pDWriteFactory->Release();
        if (m_pRenderTarget) m_pRenderTarget->Release();
        if (m_pD2DFactory) m_pD2DFactory->Release();
    }

    void Overlay::UpdateLayer1(const std::string& text) {
        m_layer1Text = text;
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void Overlay::ToggleLayer2(bool show) {
        m_showLayer2 = show;
        
        // If Layer 2 is up, remove transparent click-through so user can interact with the menu
        long exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);
        if (m_showLayer2) {
            SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        } else {
            SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        }

        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void Overlay::Render() {
        if (!m_pRenderTarget) return;

        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0.0f)); // Transparent bg

        D2D1_SIZE_F renderTargetSize = m_pRenderTarget->GetSize();

        // Convert ansi to wide string for DirectWrite
        std::wstring wText(m_layer1Text.begin(), m_layer1Text.end());

        // Render Layer 2 (Winter Glass Menu) if active
        if (m_showLayer2) {
            D2D1_RECT_F menuRect = D2D1::RectF(
                renderTargetSize.width - 400.0f, 50.0f, 
                renderTargetSize.width - 20.0f, renderTargetSize.height - 100.0f
            );
            // Draw frosted background
            m_pRenderTarget->FillRectangle(&menuRect, m_pBrushFrost);
            
            // Render the Logo Graphic inside the HUD (Evadus style)
            if (m_pLogoBitmap) {
                // Pin logo to top center of the menu
                float logoWidth = 120.0f;
                float logoHeight = 120.0f;
                float logoX = (menuRect.left + menuRect.right) / 2.0f - (logoWidth / 2.0f);
                float logoY = menuRect.top + 20.0f;
                D2D1_RECT_F logoDest = D2D1::RectF(logoX, logoY, logoX + logoWidth, logoY + logoHeight);
                m_pRenderTarget->DrawBitmap(m_pLogoBitmap, logoDest);
            }
            
            // Render Chat History placeholder below the logo
            std::wstring wMenuTitle = L"ZeroPoint :: Winter Glass\nModel: Claude 4.6 Opus\nEngine: CDP Extraction\n\nChat History Locked: " + wText;
            D2D1_RECT_F textRect = D2D1::RectF(menuRect.left + 20, menuRect.top + 160.0f, menuRect.right - 20, menuRect.bottom - 20);
            m_pRenderTarget->DrawTextW(wMenuTitle.c_str(), wMenuTitle.length(), m_pTextFormat, textRect, m_pBrushText);
        }

        // Render Layer 1 (Ghost Text in bottom right)
        if (!m_layer1Text.empty()) {
            D2D1_RECT_F layoutRect = D2D1::RectF(
                renderTargetSize.width - 200.0f, 
                renderTargetSize.height - 40.0f, 
                renderTargetSize.width - 20.0f, 
                renderTargetSize.height - 10.0f
            );
            m_pRenderTarget->DrawTextW(wText.c_str(), wText.length(), m_pTextFormat, layoutRect, m_pBrushText);
        }

        m_pRenderTarget->EndDraw();
    }
}
idColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), &pGhostTextBrush);
            if (pGhostTextBrush) {
                m_pRenderTarget->DrawTextW(wText.c_str(), wText.length(), m_pTextFormat, layoutRect, pGhostTextBrush);
                pGhostTextBrush->Release();
            }
        }

        m_pRenderTarget->EndDraw();
    }
}
