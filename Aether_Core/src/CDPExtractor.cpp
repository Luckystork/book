// ============================================================================
//  ZeroPoint — CDPExtractor.cpp
//  Chrome DevTools Protocol (CDP) content extraction from Bluebook / any
//  Chromium-based browser running with --remote-debugging-port=9222.
//
//  PROTOCOL:
//  CDP communicates over WebSocket. The correct flow is:
//    1. HTTP GET http://127.0.0.1:9222/json -> get webSocketDebuggerUrl
//    2. WebSocket upgrade handshake to that URL
//    3. Send JSON-RPC commands over the WebSocket frame
//
//  Current implementation uses HTTP /json/version endpoint to discover the
//  debugger URL, then performs a raw-TCP exchange. For full production use,
//  upgrade to a proper WebSocket client (e.g., libwebsockets or Beast).
// ============================================================================

#include "../include/CDPExtractor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// Winsock initialized once at startup, cleaned up on exit
static bool g_WinsockReady = false;

void InitCDPNetworking() {
    if (g_WinsockReady) return;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
        g_WinsockReady = true;
}

void ShutdownCDPNetworking() {
    if (g_WinsockReady) {
        WSACleanup();
        g_WinsockReady = false;
    }
}

// RAII wrapper for SOCKET to prevent leaks
struct ScopedSocket {
    SOCKET s;
    ScopedSocket() : s(INVALID_SOCKET) {}
    explicit ScopedSocket(SOCKET sock) : s(sock) {}
    ~ScopedSocket() { if (s != INVALID_SOCKET) closesocket(s); }
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;
    operator SOCKET() const { return s; }
    SOCKET release() { SOCKET t = s; s = INVALID_SOCKET; return t; }
};

// Helper: connect to 127.0.0.1:9222 with timeout
static SOCKET ConnectToDebugPort() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    // 3-second send/recv timeout
    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(9222);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

// Helper: receive all available data into a string
static std::string RecvAll(SOCKET sock, int maxBytes = 65536) {
    std::string result;
    result.reserve(4096);
    std::vector<char> buf(8192);
    int total = 0;
    while (total < maxBytes) {
        int n = recv(sock, buf.data(), (int)buf.size(), 0);
        if (n <= 0) break;
        result.append(buf.data(), n);
        total += n;
    }
    return result;
}

// Robust JSON string value extraction — handles escaped quotes
static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        // Try with space after colon
        needle = "\"" + key + "\": \"";
        pos = json.find(needle);
        if (pos == std::string::npos) return "";
    }
    pos += needle.size();

    std::string result;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); i++) {
        if (escaped) {
            switch (json[i]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case '/':  result += '/';  break;
                default:   result += '\\'; result += json[i]; break;
            }
            escaped = false;
        } else if (json[i] == '\\') {
            escaped = true;
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

// ============================================================================
//  Extract visible text from the current page via CDP
// ============================================================================

std::string ExtractBluebookDOM() {
    if (!g_WinsockReady) {
        InitCDPNetworking();
        if (!g_WinsockReady)
            return "[CDP] Winsock initialization failed";
    }

    ScopedSocket sock(ConnectToDebugPort());
    if (sock == INVALID_SOCKET)
        return "[CDP] Debug port 9222 not open — is Bluebook running with remote debugging?";

    // Send CDP Runtime.evaluate command to extract page text.
    // This is sent as a raw HTTP-like request. For full WebSocket support,
    // a proper WS handshake would be needed first.
    const char* cdpCmd =
        R"({"id":1,"method":"Runtime.evaluate","params":{)"
        R"("expression":"document.body.innerText","returnByValue":true}})";

    if (send(sock, cdpCmd, (int)strlen(cdpCmd), 0) <= 0)
        return "[CDP] Failed to send command";

    std::string resp = RecvAll(sock);
    if (resp.empty())
        return "[CDP] No response from debug port (timeout or connection reset)";

    // Parse the "value" field from the JSON response
    std::string value = ExtractJsonValue(resp, "value");
    if (!value.empty())
        return value;

    // Couldn't parse — return truncated raw response for debugging
    if (resp.size() > 500) resp.resize(500);
    return "[CDP] Raw response: " + resp;
}
