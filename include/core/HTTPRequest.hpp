#pragma once

#include <string>
#include <unordered_map>

class HTTPRequest
{
public:
    std::string method;
    std::string path;
    std::string version;

    std::unordered_map<std::string, std::string> headers;

    std::string body;

    static HTTPRequest parse(const std::string& request);
};