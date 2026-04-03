#include "CDPExtractor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

std::string ExtractBluebookDOM() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return "WSA failed";

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9222);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        return "Bluebook CDP port not open";
    }

    const char* cdpCmd = R"({"id":1,"method":"Runtime.evaluate","params":{"expression":"document.body.innerText","returnByValue":true}})";
    send(sock, cdpCmd, (int)strlen(cdpCmd), 0);

    char buf[16384] = {0};
    recv(sock, buf, sizeof(buf)-1, 0);

    closesocket(sock);
    WSACleanup();

    std::string resp(buf);
    size_t start = resp.find("\"value\":\"");
    if (start != std::string::npos) {
        start += 9;
        size_t end = resp.find("\"", start);
        return resp.substr(start, end - start);
    }
    return resp;
}
