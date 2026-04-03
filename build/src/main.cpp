#include "Stealth.h"
#include "Hooks.h"
#include "CDPExtractor.h"
#include "Overlay.h"
#include "Vision.h"
#include "Networking.h"
#include "Config.h"
#include <iostream>
#include <windows.h>

// Globals
extern bool g_Processing;

// Configs (These would be set in Layer 2 by Ivan)
bool g_UseCDPMode = false;  // False = Respondus/OCR mode, True = Bluebook/CDP mode
std::string g_AnthropicKey = "sk-ant-..."; // Placeholder

void ExecuteSwiftCapture() {
    g_Processing = true;
    if (UI::g_Overlay) UI::g_Overlay->UpdateLayer1("[...]");

    // Engine routing: Are we in Bluebook Mode or Universal Mode?
    // Simulate CDP Extract/AI latency
    Sleep(1500); 

    std::string mockAIResponse = "13 D [+]"; 
    
    if (UI::g_Overlay) UI::g_Overlay->UpdateLayer1(mockAIResponse);
    g_Processing = false;
}

void ToggleEvadusMenu() {
    static bool isWinterGlassVisible = false;
    isWinterGlassVisible = !isWinterGlassVisible;
    if (UI::g_Overlay) {
        UI::g_Overlay->ToggleLayer2(isWinterGlassVisible);
    }
}

void ExecutePanicKill() {
    Keybinder::RemoveHooks();
    ExitProcess(0);
}

int main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 0. Hard Lock: Prevent stealth proxy injection if no API keys are configured
    if (!Config::LoadAPIKeys()) {
        std::cout << "========================================" << std::endl;
        std::cout << "        Z E R O P O I N T  S E T U P    " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "[FATAL] No API Keys found in local configuration." << std::endl;
        std::cout << "ZeroPoint cannot initialize. Please enter your Claude/Gemini API key below:" << std::endl;
        
        std::string newKey;
        std::cout << "Anthropic Key: ";
        std::getline(std::cin, newKey);
        
        Config::SaveAPIKeys(newKey, "");
        if (!Config::LoadAPIKeys()) {
            std::cerr << "[!] Authentication failed. Shutting down." << std::endl;
            return 1;
        }
        std::cout << "[*] Keys loaded. Initializing stealth proxy..." << std::endl;
        Sleep(1000);
    }

    // 1. Initialize self-protection (Cloak the console if it exists)
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        Stealth::CloakWindow(consoleWnd);
        // ShowWindow(consoleWnd, SW_HIDE); // Hide for production
    }

    // 2. Initialize the Winter Glass UI Engine
    UI::Overlay overlayEngine;
    if (!overlayEngine.CreateTransparentWindow(GetModuleHandle(NULL))) {
        return 1;
    }

    // 3. Install low-level keyboard hooks
    if (!Keybinder::InitializeHooks()) {
        return 1;
    }

    // 4. Keep the thread alive to process Windows messages (required for UI and hooks)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Keybinder::RemoveHooks();
    return 0;
}
