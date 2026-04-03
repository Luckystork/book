#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace UI {
    class Overlay {
    private:
        HWND m_hwnd;
        ID2D1Factory* m_pD2DFactory;
        ID2D1HwndRenderTarget* m_pRenderTarget;
        IDWriteFactory* m_pDWriteFactory;
        IDWriteTextFormat* m_pTextFormat;
        ID2D1SolidColorBrush* m_pBrushText;
        ID2D1SolidColorBrush* m_pBrushFrost;
        
        IWICImagingFactory* m_pWICFactory;
        ID2D1Bitmap* m_pLogoBitmap;

        std::string m_layer1Text;
        bool m_showLayer2;

        bool InitializeDirect2D();
        void CleanupDirect2D();

    public:
        Overlay();
        ~Overlay();

        bool CreateTransparentWindow(HINSTANCE hInstance);
        void Render();
        void UpdateLayer1(const std::string& text);
        void ToggleLayer2(bool show);
        
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };

    // Global overlay instance for access by WndProc and main hooks
    extern Overlay* g_Overlay;
}
