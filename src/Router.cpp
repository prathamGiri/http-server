#include "HTTPResponse.hpp"
#include "HTTPRequest.hpp"
#include "Router.hpp"

#include <string>

std::string createKey(const std::string& method, const std::string& path){
    std::string key = method + path;
    return key;
}

void Router::get(const std::string& path , Handler handler){
    getRoutes[createKey("GET", path)] = handler;
}

void Router::post(const std::string& path , Handler handler){
    getRoutes[createKey("POST", path)] = handler;
}

HTTPResponse Router::route(const HTTPRequest& request){
    auto it = getRoutes.find(createKey(request.method, request.path));
    if(it != getRoutes.end()){
        return it->second(request);
    }
    
    HTTPResponse response;
    response.setStatus(404, "Not Found!");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("404 Not Found!");

    return response;
}