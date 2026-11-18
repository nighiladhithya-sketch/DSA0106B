// HttpServer.cpp
#include "HttpServer.h"
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  static void closeSocket(SocketHandle s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <unistd.h>
  using SocketHandle = int;
  static void closeSocket(SocketHandle s) { close(s); }
#endif

static const std::intptr_t NO_SOCKET = -1;

HttpServer::HttpServer() : listenSocket_(NO_SOCKET) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
#endif
}

HttpServer::~HttpServer() {
    if (listenSocket_ != NO_SOCKET) closeSocket(static_cast<SocketHandle>(listenSocket_));
#ifdef _WIN32
    WSACleanup();
#endif
}

int HttpServer::listen(int preferredPort, int attempts) {
    for (int port = preferredPort; port < preferredPort + attempts; ++port) {
        SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (static_cast<std::intptr_t>(s) == NO_SOCKET) throw std::runtime_error("Could not create socket");
#ifndef _WIN32
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof yes);
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);         // only this computer can connect
        addr.sin_port = htons(static_cast<unsigned short>(port));
        if (bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof addr) == 0 && ::listen(s, 16) == 0) {
            listenSocket_ = static_cast<std::intptr_t>(s);
            return port;
        }
        closeSocket(s);                                        // port busy: try the next one
    }
    throw std::runtime_error("No free port found");
}

std::string HttpServer::urlDecode(const std::string &text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') out += ' ';
        else if (text[i] == '%' && i + 2 < text.size() &&
                 std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else out += text[i];
    }
    return out;
}

HttpRequest HttpServer::parseRequest(const std::string &raw) {
    HttpRequest req;
    std::istringstream firstLine(raw.substr(0, raw.find("\r\n")));
    std::string target;
    firstLine >> req.method >> target;
    const std::size_t q = target.find('?');
    req.path = urlDecode(target.substr(0, q));
    if (q != std::string::npos) {
        std::stringstream params(target.substr(q + 1));
        std::string pair;
        while (std::getline(params, pair, '&')) {
            const std::size_t eq = pair.find('=');
            if (eq == std::string::npos) req.query[urlDecode(pair)] = "";
            else req.query[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        }
    }
    return req;
}

static const char *statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default:  return "Internal Server Error";
    }
}

void HttpServer::handleClient(std::intptr_t clientId, const Handler &handler) {
    SocketHandle client = static_cast<SocketHandle>(clientId);

    // Time out idle connections (browsers open spare "pre-connect" sockets).
#ifdef _WIN32
    DWORD timeout = 2000;
#else
    timeval timeout{2, 0};
#endif
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof timeout);

    std::string raw;
    char buffer[4096];
    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < 65536) {
        const int n = static_cast<int>(recv(client, buffer, sizeof buffer, 0));
        if (n <= 0) break;
        raw.append(buffer, static_cast<std::size_t>(n));
    }

    if (!raw.empty()) {
        HttpResponse res;
        try {
            res = handler(parseRequest(raw));
        } catch (const std::exception &e) {
            res.status = 500;
            res.body = std::string("{\"error\":\"") + e.what() + "\"}";
        }
        std::ostringstream head;
        head << "HTTP/1.1 " << res.status << ' ' << statusText(res.status) << "\r\n"
             << "Content-Type: " << res.contentType << "\r\n"
             << "Content-Length: " << res.body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n";
        const std::string out = head.str() + res.body;
        std::size_t sent = 0;
        while (sent < out.size()) {
            const int n = static_cast<int>(send(client, out.data() + sent, static_cast<int>(out.size() - sent), 0));
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
    }
    closeSocket(client);
}

void HttpServer::serve(const Handler &handler) {
    if (listenSocket_ == NO_SOCKET) throw std::runtime_error("Call listen() before serve()");
    for (;;) {
        SocketHandle client = accept(static_cast<SocketHandle>(listenSocket_), nullptr, nullptr);
        if (static_cast<std::intptr_t>(client) == NO_SOCKET) continue;
        handleClient(static_cast<std::intptr_t>(client), handler);
    }
}
