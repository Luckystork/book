// ============================================================================
//  ZeroPoint — CDPExtractor.cpp  (v4.3.0 Final)
//  Chrome DevTools Protocol (CDP) content extraction from Bluebook / any
//  Chromium-based browser running with --remote-debugging-port=9222.
//
//  PROTOCOL (full production implementation):
//    1. HTTP GET http://127.0.0.1:9222/json -> get webSocketDebuggerUrl
//    2. WebSocket upgrade handshake to that URL
//    3. Send JSON-RPC command as a WebSocket text frame
//    4. Receive WebSocket text frame with the result
//    5. Parse the value from the JSON response
// ============================================================================

#include "../include/CDPExtractor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

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

// ============================================================================
//  TCP Helpers
// ============================================================================

static SOCKET ConnectToDebugPort() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    DWORD timeout = 5000;
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

static bool SendAll(SOCKET sock, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

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
        // Stop at end of HTTP response (double CRLF for headers, then body)
        if (result.find("\r\n\r\n") != std::string::npos) {
            // Check for Content-Length or just read what's available
            size_t headerEnd = result.find("\r\n\r\n") + 4;
            // Try to read a bit more for the body
            if (total < maxBytes) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(sock, &readfds);
                timeval tv = { 0, 200000 }; // 200ms
                while (select((int)sock + 1, &readfds, NULL, NULL, &tv) > 0) {
                    n = recv(sock, buf.data(), (int)buf.size(), 0);
                    if (n <= 0) break;
                    result.append(buf.data(), n);
                    total += n;
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);
                    tv = { 0, 100000 };
                }
            }
            break;
        }
    }
    return result;
}

// ============================================================================
//  JSON Helpers
// ============================================================================

static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
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
//  WebSocket Handshake + Framing (RFC 6455)
// ============================================================================
//  Minimal but correct implementation for CDP communication over localhost.

// Generate a random 16-byte base64 key for Sec-WebSocket-Key
static std::string GenerateWSKey() {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char raw[16];
    // Use system entropy: tick count XOR'd with address of local var
    srand((unsigned)(GetTickCount() ^ (uintptr_t)&raw ^ (unsigned)time(NULL)));
    for (int i = 0; i < 16; i++) raw[i] = (unsigned char)(rand() & 0xFF);

    std::string out;
    out.reserve(24);
    for (int i = 0; i < 16; i += 3) {
        unsigned int n = ((unsigned int)raw[i]) << 16;
        if (i + 1 < 16) n |= ((unsigned int)raw[i + 1]) << 8;
        if (i + 2 < 16) n |= raw[i + 2];
        out += b64[(n >> 18) & 0x3F];
        out += b64[(n >> 12) & 0x3F];
        out += (i + 1 < 16) ? b64[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < 16) ? b64[n & 0x3F] : '=';
    }
    return out;
}

// Parse the host and path from a ws:// or wss:// URL
static bool ParseWSUrl(const std::string& url, std::string& host, int& port, std::string& path) {
    size_t start = 0;
    if (url.rfind("ws://", 0) == 0) start = 5;
    else if (url.rfind("wss://", 0) == 0) start = 6;
    else return false;

    size_t pathStart = url.find('/', start);
    std::string authority = (pathStart != std::string::npos)
        ? url.substr(start, pathStart - start)
        : url.substr(start);
    path = (pathStart != std::string::npos) ? url.substr(pathStart) : "/";

    size_t colonPos = authority.find(':');
    if (colonPos != std::string::npos) {
        host = authority.substr(0, colonPos);
        port = atoi(authority.substr(colonPos + 1).c_str());
    } else {
        host = authority;
        port = 9222;
    }
    return true;
}

// Perform WebSocket upgrade handshake. Returns true on success.
static bool WSHandshake(SOCKET sock, const std::string& host, int port, const std::string& path) {
    std::string wsKey = GenerateWSKey();

    std::string req = "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + std::to_string(port) + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + wsKey + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    if (!SendAll(sock, req.c_str(), (int)req.size())) return false;

    // Read the HTTP response (101 Switching Protocols expected)
    std::string resp;
    resp.reserve(1024);
    char buf[1024];
    while (true) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        resp.append(buf, n);
        if (resp.find("\r\n\r\n") != std::string::npos) break;
        if (resp.size() > 4096) return false; // Headers too large
    }

    // Verify 101 status
    return (resp.find("HTTP/1.1 101") != std::string::npos);
}

// Send a WebSocket text frame (client must mask per RFC 6455)
static bool WSSendText(SOCKET sock, const std::string& payload) {
    size_t len = payload.size();

    // Frame header: FIN=1, opcode=0x1 (text), MASK=1
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + text opcode

    if (len < 126) {
        frame.push_back((unsigned char)(len | 0x80)); // length + MASK bit
    } else if (len < 65536) {
        frame.push_back(0xFE); // 126 + MASK bit
        frame.push_back((unsigned char)((len >> 8) & 0xFF));
        frame.push_back((unsigned char)(len & 0xFF));
    } else {
        frame.push_back(0xFF); // 127 + MASK bit
        for (int i = 7; i >= 0; i--)
            frame.push_back((unsigned char)((len >> (8 * i)) & 0xFF));
    }

    // 4-byte masking key (client frames must be masked)
    unsigned char mask[4];
    unsigned int seed = GetTickCount() ^ (unsigned int)(uintptr_t)&mask;
    mask[0] = (unsigned char)(seed & 0xFF);
    mask[1] = (unsigned char)((seed >> 8) & 0xFF);
    mask[2] = (unsigned char)((seed >> 16) & 0xFF);
    mask[3] = (unsigned char)((seed >> 24) & 0xFF);
    frame.insert(frame.end(), mask, mask + 4);

    // Masked payload
    for (size_t i = 0; i < len; i++)
        frame.push_back((unsigned char)(payload[i] ^ mask[i % 4]));

    return SendAll(sock, (const char*)frame.data(), (int)frame.size());
}

// Receive a WebSocket text frame. Returns the payload, or empty on error.
static std::string WSRecvText(SOCKET sock, int maxBytes = 262144) {
    // Read first 2 bytes of frame header
    unsigned char hdr[2];
    int n = recv(sock, (char*)hdr, 2, MSG_WAITALL);
    if (n != 2) return "";

    bool fin = (hdr[0] & 0x80) != 0;
    int opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t payloadLen = hdr[1] & 0x7F;

    if (payloadLen == 126) {
        unsigned char ext[2];
        if (recv(sock, (char*)ext, 2, MSG_WAITALL) != 2) return "";
        payloadLen = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payloadLen == 127) {
        unsigned char ext[8];
        if (recv(sock, (char*)ext, 8, MSG_WAITALL) != 8) return "";
        payloadLen = 0;
        for (int i = 0; i < 8; i++)
            payloadLen = (payloadLen << 8) | ext[i];
    }

    if ((int64_t)payloadLen > maxBytes || payloadLen == 0) return "";

    // Read mask key if present (server frames are usually unmasked)
    unsigned char mask[4] = {};
    if (masked) {
        if (recv(sock, (char*)mask, 4, MSG_WAITALL) != 4) return "";
    }

    // Read payload
    std::string payload;
    payload.resize((size_t)payloadLen);
    size_t totalRead = 0;
    while (totalRead < (size_t)payloadLen) {
        int toRead = (int)((size_t)payloadLen - totalRead);
        if (toRead > 8192) toRead = 8192;
        n = recv(sock, &payload[totalRead], toRead, 0);
        if (n <= 0) return "";
        totalRead += n;
    }

    // Unmask if needed
    if (masked) {
        for (size_t i = 0; i < payload.size(); i++)
            payload[i] ^= mask[i % 4];
    }

    // Handle continuation frames if FIN is not set
    (void)fin; // For CDP responses, messages are typically single-frame
    (void)opcode;

    return payload;
}

// ============================================================================
//  Step 1: Discover webSocketDebuggerUrl via HTTP GET /json
// ============================================================================

static std::string DiscoverWSDebuggerUrl() {
    ScopedSocket sock(ConnectToDebugPort());
    if (sock == INVALID_SOCKET) return "";

    const char* req =
        "GET /json HTTP/1.1\r\n"
        "Host: 127.0.0.1:9222\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (!SendAll(sock, req, (int)strlen(req))) return "";

    std::string resp = RecvAll(sock);
    if (resp.empty()) return "";

    // Extract body (after \r\n\r\n)
    size_t bodyStart = resp.find("\r\n\r\n");
    if (bodyStart == std::string::npos) return "";
    std::string body = resp.substr(bodyStart + 4);

    // Find webSocketDebuggerUrl in the first page entry
    return ExtractJsonValue(body, "webSocketDebuggerUrl");
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

    // Step 1: Discover the WebSocket debugger URL
    std::string wsUrl = DiscoverWSDebuggerUrl();
    if (wsUrl.empty())
        return "[CDP] Debug port 9222 not open or no pages found — "
               "is Bluebook running with --remote-debugging-port=9222?";

    // Step 2: Parse the WS URL and connect
    std::string host;
    int port = 9222;
    std::string path;
    if (!ParseWSUrl(wsUrl, host, port, path))
        return "[CDP] Invalid WebSocket URL from debug port";

    ScopedSocket sock(ConnectToDebugPort());
    if (sock == INVALID_SOCKET)
        return "[CDP] Could not reconnect to debug port for WebSocket";

    // Step 3: WebSocket upgrade handshake
    if (!WSHandshake(sock, host, port, path))
        return "[CDP] WebSocket handshake failed (HTTP 101 not received)";

    // Step 4: Send CDP command — extract visible page text
    const char* cdpCmd =
        R"({"id":1,"method":"Runtime.evaluate","params":{)"
        R"("expression":"document.body.innerText","returnByValue":true}})";

    if (!WSSendText(sock, cdpCmd))
        return "[CDP] Failed to send WebSocket frame";

    // Step 5: Receive the response frame
    std::string wsResp = WSRecvText(sock);
    if (wsResp.empty())
        return "[CDP] No response from WebSocket (timeout or connection reset)";

    // Step 6: Parse the "value" field from the CDP JSON response
    std::string value = ExtractJsonValue(wsResp, "value");
    if (!value.empty())
        return value;

    // Couldn't parse — return truncated raw response for debugging
    if (wsResp.size() > 500) wsResp.resize(500);
    return "[CDP] Raw response: " + wsResp;
}
