#include "routing/StaticFileHandler.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

StaticFileHandler::StaticFileHandler(std::string rootDir) {
    // canonical() resolves symlinks and ".." up front, and throws if the
    // path doesn't exist — fail loudly at startup rather than silently later
    this->rootDir = fs::canonical(rootDir).string();
}

std::string StaticFileHandler::getContentType(const std::string& path) const {
    static const std::unordered_map<std::string, std::string> types = {
        {".html", "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".txt",  "text/plain"},
        {".pdf",  "application/pdf"},
        {".woff", "font/woff"},
        {".woff2","font/woff2"},
    };

    std::string ext = fs::path(path).extension().string();
    auto it = types.find(ext);
    if (it != types.end()) return it->second;
    return "application/octet-stream";   // safe default for unknown types
}

HTTPResponse StaticFileHandler::serve(const HTTPRequest& request) const {
    HTTPResponse response;

    std::string requestPath = request.path;
    if (requestPath == "/") requestPath = "/index.html";  // root → index.html

    // build the list of candidate paths to try, in order
    std::vector<std::string> candidates = {
        requestPath,                    // exact match: /about.html
        requestPath + ".html",          // extension fallback: /about → about.html
        requestPath + "/index.html"     // directory fallback: /blog → blog/index.html
    };

    for (const auto& candidatePath : candidates) {
        fs::path requested = fs::path(rootDir) / candidatePath.substr(1); // drop leading '/'

        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(requested, ec);
        if (ec) continue;

        // the same path-traversal guard as before — check every candidate,
        // not just the first, since an attacker could target any of them
        fs::path root = fs::path(rootDir);
        auto rootIt = root.begin();
        auto resIt = resolved.begin();
        bool inside = true;
        for (; rootIt != root.end(); ++rootIt, ++resIt) {
            if (resIt == resolved.end() || *resIt != *rootIt) {
                inside = false;
                break;
            }
        }
        if (!inside) continue;   // never leak this — just try the next candidate

        if (fs::exists(resolved) && fs::is_regular_file(resolved)) {
            std::ifstream file(resolved, std::ios::binary);
            if (!file) continue;

            std::ostringstream contents;
            contents << file.rdbuf();

            response.setStatus(200, "OK");
            response.setHeader("Content-Type", getContentType(resolved.string()));
            response.setBody(contents.str());
            return response;
        }
    }

    // nothing matched any candidate
    response.setStatus(404, "Not Found");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("404 Not Found");
    return response;
}