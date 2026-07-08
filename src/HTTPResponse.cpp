#include "HTTPResponse.hpp"

#include <string>
#include <sstream>

HTTPResponse::HTTPResponse(){
    statusCode = 200;
    statusText = "OK";
}

void HTTPResponse::setStatus(int code, const std::string& msg){
    statusCode = code;
    statusText = msg;
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value){
    responseHeaders[key] = value;
}

void HTTPResponse::setBody(const std::string& body){
    responseBody = body;
}

std::string HTTPResponse::toString() const{
    std::ostringstream response;

    response
        << "HTTP/1.1 "
        << statusCode
        << " "
        << statusText
        << "\r\n";

    for(const auto& [key, value]: responseHeaders){
        response
            << key
            << ": "
            << value
            << "\r\n";
    }

    response
        << "Content-Length: "
        << responseBody.size();

    response
        << "\r\n";

    response
        << "\r\n"
        << responseBody;

    return response.str();
}