#pragma once

#include <string>
#include <functional>
#include <unordered_map>

#include "HTTPResponse.hpp"
#include "HTTPRequest.hpp"

class Router{
public:
    using Handler = std::function<HTTPResponse(const HTTPRequest&)>;
private:
    std::unordered_map<std::string, Handler> getRoutes;
public:
    void get(const std::string& path , Handler handler);
    void post(const std::string& path , Handler handler);
    HTTPResponse route(const HTTPRequest& request);
};