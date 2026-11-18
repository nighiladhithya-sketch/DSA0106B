// HttpServer.h - minimal single-threaded HTTP/1.1 server on raw sockets (Windows + Linux/macOS)
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>

struct HttpRequest {
    std::string method;
    std::string path;                               // without the query string
    std::map<std::string, std::string> query;       // URL-decoded parameters

    std::string param(const std::string &key, const std::string &fallback = "") const {
        auto it = query.find(key);
        return it == query.end() ? fallback : it->second;
    }
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest &)>;

    HttpServer();
    ~HttpServer();
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;

    int listen(int preferredPort, int attempts = 10);   // returns the port actually bound
    void serve(const Handler &handler);                 // runs until the process is stopped

    static std::string urlDecode(const std::string &text);
    static HttpRequest parseRequest(const std::string &raw);

private:
    void handleClient(std::intptr_t client, const Handler &handler);
    std::intptr_t listenSocket_;
};
