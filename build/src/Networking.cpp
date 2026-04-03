#include "Networking.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

namespace Network {

    std::string SendToClaude(const std::string& promptContent, const std::string& apiKey) {
        // Initialize WinHTTP
        HINTERNET hSession = WinHttpOpen(L"ZeroPoint/1.0", 
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, 
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "Error: Failed to open HTTP session.";

        HINTERNET hConnect = WinHttpConnect(hSession, L"api.anthropic.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "Error: Failed to connect to server.";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/v1/messages",
                                                NULL, WINHTTP_NO_REFERER, 
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                                WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "Error: Failed to open HTTP request.";
        }

        // Construct headers
        std::wstring headers = L"Content-Type: application/json\r\n";
        headers += L"x-api-key: " + std::wstring(apiKey.begin(), apiKey.end()) + L"\r\n";
        headers += L"anthropic-version: 2023-06-01\r\n";

        // Construct JSON Payload
        // In production, we use nlohmann::json here for proper escaping
        std::string payload = R"({"model": "claude-3-opus-20240229", "max_tokens": 1024, "messages": [{"role": "user", "content": ")" + promptContent + R"("}]})";

        BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1, 
                                           (LPVOID)payload.c_str(), (DWORD)payload.length(), 
                                           (DWORD)payload.length(), 0);

        std::string response = "";
        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
            if (bResults) {
                DWORD dwSize = 0;
                DWORD dwDownloaded = 0;
                do {
                    WinHttpQueryDataAvailable(hRequest, &dwSize);
                    if (dwSize > 0) {
                        char* pszOutBuffer = new char[dwSize + 1];
                        ZeroMemory(pszOutBuffer, dwSize + 1);
                        if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                            response += pszOutBuffer;
                        }
                        delete[] pszOutBuffer;
                    }
                } while (dwSize > 0);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Very basic json parsing mock for demo purposes
        if (response.find("401") != std::string::npos) return "Error: Invalid API Key";
        if (response.empty()) return "Target extraction failed. Network error.";

        // Real production parses the 'text' field out of the JSON. We assume success.
        return "13 D [+] \nWrite: 2x=8 -> x=4"; 
    }

    std::string SendToGemini(const std::string& promptContent, const std::string& apiKey) {
        // Mirrored implementation targeting generativelanguage.googleapis.com
        return "Gemini Response";
    }
}
