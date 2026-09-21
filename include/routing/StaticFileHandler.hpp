#pragma once

#include <string>

#include "core/HTTPRequest.hpp"
#include "core/HTTPResponse.hpp"

class StaticFileHandler {
public:
    explicit StaticFileHandler(std::string rootDir);
    HTTPResponse serve(const HTTPRequest& request) const;

private:
    std::string rootDir;   // absolute, canonical path to the static root
    std::string getContentType(const std::string& path) const;
};