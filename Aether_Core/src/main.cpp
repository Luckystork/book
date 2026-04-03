#include "CDPExtractor.h"
#include "Config.h"
#include "Stealth.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

// Full AI call using ONE OpenRouter key + selected model
std::string CallAI(const std::string &question) {
  std::string key = GetOpenRouterKey();
  std::string model = GetActiveModel();

  if (key.empty())
    return "[ERROR] No OpenRouter API key set";

  HINTERNET hSession = WinHttpOpen(
      L"ZeroPoint/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
  if (!hSession)
    return "[ERROR] WinHttp failed";

  std::wstring hostW = L"openrouter.ai";
  std::wstring pathW = L"/api/v1/chat/completions";

  HINTERNET hConnect =
      WinHttpConnect(hSession, hostW.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"POST", pathW.c_str(), NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

  std::string jsonPayload = R"({"model":")" + model +
                            R"(","messages":[{"role":"user","content":")" +
                            question + R"("}],"max_tokens":800})";

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

// Invisible browser placeholder (expand to full WebView2 later)
void OpenInvisibleBrowser() {
  MessageBoxA(
      NULL,
      "Invisible Browser Activated\n\nYou can now open any website (Google, "
      "YouTube, ChatGPT web, etc.)\n\nPress Ctrl+Alt+B again to close",
      "ZeroPoint - Invisible Browser", MB_OK | MB_ICONINFORMATION);
  // Real WebView2 code can be added here later
}

int main() {
  LoadConfig();

  // First run - ask for OpenRouter key if none exists
  if (GetOpenRouterKey().empty()) {
    int result = MessageBoxA(NULL,
                             "ZeroPoint Launcher\n\nNo OpenRouter API key "
                             "detected.\n\nPaste your OpenRouter API key now:",
                             "ZeroPoint - First Launch",
                             MB_OKCANCEL | MB_ICONINFORMATION);
    if (result == IDCANCEL)
      return 0;
    SetOpenRouterKey(
        "your-openrouter-key-here"); // replace with real input dialog later
  }

  // Main launcher with ALL hotkeys clearly displayed
  std::string launcherText =
      "ZeroPoint Launcher\n\n"
      "Active Model: " +
      GetActiveModel() +
      "\n\n"
      "Hotkeys (after Inject):\n"
      "Ctrl + Shift + Z   → AI Snapshot (bottom-right answer)\n"
      "Ctrl + Alt + H     → Full frosted menu\n"
      "Ctrl + Alt + B     → Invisible full browser (YouTube, Google, etc.)\n"
      "Ctrl + Shift + X   → Panic killswitch\n\n"
      "Click OK to INJECT stealth mode\n"
      "Click Cancel to change model";

  int launchResult =
      MessageBoxA(NULL, launcherText.c_str(), "ZeroPoint Launcher",
                  MB_OKCANCEL | MB_ICONINFORMATION);

  if (launchResult == IDCANCEL) {
    // Model selector (sidebar style like your Gemini screenshot)
    std::string selectorMsg = "Select Model:\n\n";
    for (int i = 0; i < 4; i++) {
      selectorMsg += (i == g_ActiveModelIndex ? "→ " : "   ") +
                     std::to_string(i + 1) + ". " + g_AvailableModels[i] + "\n";
    }
    selectorMsg += "\nType the number (1-4) to select.";
    MessageBoxA(NULL, selectorMsg.c_str(), "ZeroPoint - Model Selector",
                MB_OK | MB_ICONINFORMATION);
    SetActiveModel(0); // default for testing - expand later
  } else {
    // Inject stealth
    if (PerformHollowing()) {
      MessageBoxA(NULL,
                  "Stealth ACTIVATED.\nHollowed successfully.\nActive model: " +
                      GetActiveModel(),
                  "ZeroPoint", MB_OK | MB_ICONINFORMATION);
    } else {
      MessageBoxA(NULL, "Hollowing failed.", "ZeroPoint", MB_OK | MB_ICONERROR);
      return 1;
    }
  }

  // Background hotkey loop
  while (true) {
    // Ctrl+Shift+Z = AI Snapshot
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x5A) & 0x8000)) {
      std::string question = ExtractBluebookDOM();
      std::string answer = CallAI(question);
      MessageBoxA(NULL, answer.c_str(), "ZeroPoint Answer", MB_OK);
    }

    // Ctrl+Alt+H = Full menu
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000) &&
        (GetAsyncKeyState(0x48) & 0x8000)) {
      // ToggleFullMenu();
    }

    // Ctrl+Alt+B = Invisible Browser
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000) &&
        (GetAsyncKeyState(0x42) & 0x8000)) {
      OpenInvisibleBrowser();
    }

    // Ctrl+Shift+X = Panic
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(0x58) & 0x8000)) {
      PanicKillAndWipe();
      break;
    }

    Sleep(10);
  }
  return 0;
}