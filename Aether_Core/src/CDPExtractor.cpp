// ============================================================================
//  ZeroPoint — CDPExtractor.cpp
//  Chrome DevTools Protocol (CDP) content extraction from Bluebook / any
//  Chromium-based browser running with --remote-debugging-port=9222.
//
//  PROTOCOL NOTE:
//  CDP communicates over WebSocket, not raw TCP. The correct flow is:
//    1. HTTP GET http://127.0.0.1:9222/json → get webSocketDebuggerUrl
//    2. WebSocket upgrade handshake to that URL
//    3. Send JSON-RPC commands over the WebSocket frame
//
//  The current implementation uses a simplified raw-TCP approach which
//  works for basic local testing. For production Bluebook extraction,
//  upgrade to a proper WebSocket client (e.g. libwebsockets or Beast).
//
//  TODO: Implement full WebSocket handshake for robust CDP communication.
// ============================================================================

#include "CDPExtractor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

// ============================================================================
//  Extract visible text from the current page via CDP
// ============================================================================

std::string ExtractBluebookDOM() {
    // Initialize Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return "[CDP] Winsock initialization failed";
    }

    // Connect to Chromium debug port
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return "[CDP] Socket creation failed";
    }

    // Set a 3-second timeout so we don't hang forever
    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
               (const char*)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(9222);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        return "[CDP] Debug port 9222 not open — is Bluebook running with remote debugging?";
    }

    // Send CDP Runtime.evaluate command to extract page text
    const char* cdpCmd =
        R"({"id":1,"method":"Runtime.evaluate","params":{)"
        R"("expression":"document.body.innerText","returnByValue":true}})";

    int sent = send(sock, cdpCmd, (int)strlen(cdpCmd), 0);
    if (sent <= 0) {
        closesocket(sock);
        WSACleanup();
        return "[CDP] Failed to send command";
    }

    // Read the response
    char buf[16384] = {};
    int received = recv(sock, buf, sizeof(buf) - 1, 0);

    closesocket(sock);
    WSACleanup();

    if (received <= 0) {
        return "[CDP] No response from debug port (timeout or connection reset)";
    }

    // Parse the "value":"..." field from the JSON response
    std::string resp(buf, received);
    size_t start = resp.find("\"value\":\"");
    if (start != std::string::npos) {
        start += 9;  // skip past "value":"
        size_t end = resp.find("\"", start);
        if (end != std::string::npos && end > start) {
            return resp.substr(start, end - start);
        }
    }

    // Couldn't parse — return raw response for debugging
    return "[CDP] Raw response: " + resp.substr(0, 500);
}
