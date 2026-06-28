#pragma once

// Minimal HTTP/1.1 server using POSIX sockets.
// Single-threaded accept loop — fine for local development and small user counts.
// Static files served from ../frontend/ without modifying frontend source files.

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace ft {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
    bool noCache = false;

    std::string toString() const {
        std::ostringstream out;
        out << "HTTP/1.1 " << status << " ";
        switch (status) {
            case 200: out << "OK"; break;
            case 201: out << "Created"; break;
            case 400: out << "Bad Request"; break;
            case 401: out << "Unauthorized"; break;
            case 404: out << "Not Found"; break;
            case 409: out << "Conflict"; break;
            case 500: out << "Internal Server Error"; break;
            default:  out << "Unknown"; break;
        }
        out << "\r\n";
        out << "Content-Type: " << contentType << "\r\n";
        out << "Content-Length: " << body.size() << "\r\n";
        out << "Access-Control-Allow-Origin: *\r\n";
        out << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
        out << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
        if (noCache) {
            out << "Cache-Control: no-cache\r\n";
        }
        out << "Connection: close\r\n\r\n";
        out << body;
        return out.str();
    }

    static HttpResponse json(int statusCode, const std::string& jsonBody) {
        HttpResponse r;
        r.status = statusCode;
        r.contentType = "application/json";
        r.body = jsonBody;
        return r;
    }

    static HttpResponse text(int statusCode, const std::string& textBody, const std::string& type) {
        HttpResponse r;
        r.status = statusCode;
        r.contentType = type;
        r.body = textBody;
        return r;
    }
};

using RouteHandler = HttpResponse (*)(const HttpRequest&, void* context);

class HttpServer {
public:
    HttpServer(std::string host, int port, std::string frontendRoot)
        : host_(std::move(host)), port_(port), frontendRoot_(std::move(frontendRoot)) {}

    void addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
        routes_[method + " " + path] = handler;
    }

    void setContext(void* ctx) { context_ = ctx; }

    // Blocking run loop — listens until process is stopped.
    void run() {
        const int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) {
            throw std::runtime_error("socket() failed");
        }

        const int yes = 1;
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(port_));
        address.sin_addr.s_addr = inet_addr(host_.c_str());

        if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close(serverFd);
            throw std::runtime_error("bind() failed on port " + std::to_string(port_));
        }

        if (listen(serverFd, 16) < 0) {
            close(serverFd);
            throw std::runtime_error("listen() failed");
        }

        while (true) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            const int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (clientFd < 0) continue;
            handleClient(clientFd);
            close(clientFd);
        }
    }

private:
    std::string host_;
    int port_;
    std::string frontendRoot_;
    void* context_ = nullptr;
    std::unordered_map<std::string, RouteHandler> routes_;

    static std::string toLower(std::string value) {
        for (char& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    static HttpRequest parseRequest(const std::string& raw) {
        HttpRequest req;
        std::istringstream stream(raw);
        std::string requestLine;
        std::getline(stream, requestLine);
        if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

        std::istringstream rl(requestLine);
        std::string version;
        rl >> req.method >> req.path >> version;

        std::string line;
        int contentLength = 0;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            const size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string key = toLower(line.substr(0, colon));
            std::string value = line.substr(colon + 1);
            while (!value.empty() && value.front() == ' ') value.erase(value.begin());
            req.headers[key] = value;
            if (key == "content-length") {
                contentLength = std::stoi(value);
            }
        }

        if (contentLength > 0) {
            req.body.resize(static_cast<size_t>(contentLength));
            stream.read(&req.body[0], contentLength);
        }
        return req;
    }

    static std::string readFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return "";
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static std::string mimeType(const std::string& path) {
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css; charset=utf-8";
        if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript; charset=utf-8";
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") return "image/png";
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
        return "application/octet-stream";
    }

    // Prevent directory traversal attacks in static file paths.
    static bool isSafeStaticPath(const std::string& path) {
        return path.find("..") == std::string::npos;
    }

    HttpResponse serveStatic(const std::string& path) const {
        std::string filePath = path;
        if (filePath == "/" || filePath == "/index.html") {
            filePath = "/index.html";
        }
        if (!isSafeStaticPath(filePath)) {
            return HttpResponse::text(400, "Invalid path", "text/plain");
        }

        const std::string fullPath = frontendRoot_ + filePath;
        const std::string content = readFile(fullPath);
        if (content.empty()) {
            return HttpResponse::text(404, "Not Found", "text/plain");
        }

        HttpResponse resp = HttpResponse::text(200, content, mimeType(filePath));
        resp.noCache = (filePath.find(".js") != std::string::npos
                     || filePath.find(".html") != std::string::npos);
        return resp;
    }

    HttpResponse dispatch(const HttpRequest& req) {
        if (req.method == "OPTIONS") {
            return HttpResponse::text(204, "", "text/plain");
        }

        const std::string key = req.method + " " + req.path;
        auto it = routes_.find(key);
        if (it != routes_.end()) {
            return it->second(req, context_);
        }

        // Prefix routes for API with path parameters are handled separately in back.cpp
        if (req.path.rfind("/api/", 0) == 0) {
            return HttpResponse::json(404, "{\"success\":false,\"error\":\"API route not found\"}");
        }

        return serveStatic(req.path);
    }

    void handleClient(int clientFd) {
        std::string raw;
        char buffer[4096];
        while (raw.find("\r\n\r\n") == std::string::npos) {
            const ssize_t n = recv(clientFd, buffer, sizeof(buffer), 0);
            if (n <= 0) return;
            raw.append(buffer, static_cast<size_t>(n));
            if (raw.size() > 1024 * 1024) return; // 1 MB guard
        }

        HttpRequest req = parseRequest(raw);
        HttpResponse resp = dispatch(req);
        const std::string out = resp.toString();
        send(clientFd, out.c_str(), out.size(), 0);
    }
};

} // namespace ft
