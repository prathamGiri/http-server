// GET / HTTP/1.1
// Host: 192.168.1.3:8080
// Connection: keep-alive
// Cache-Control: max-age=0
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
// Accept-Encoding: gzip, deflate
// Accept-Language: en-US,en;q=0.9

// convert above request to class

#include "HTTPRequest.hpp"

#include <string>
#include <sstream>

HTTPRequest HTTPRequest::parse(const std::string& request){
    HTTPRequest reqObj;
    std::stringstream ss(request);
    ss >> reqObj.method >> reqObj.path >> reqObj.version;

    std::string line;

    while(std::getline(ss, line)){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) break;

        auto pos = line.find(':');
        if(pos == std::string::npos){
            continue;
        }

        std::string key = line.substr(0,pos);
        std::string value = line.substr(pos+1);
        if(!value.empty() && value.front() == ' ') value.erase(0,1);

        reqObj.headers[key] = value;
    }

    std::string remaining;
    std::string body;

    while(std::getline(ss, remaining)){
        body+=remaining;
        if(!ss.eof()){
            body+='\n';
        }
    }
    reqObj.body = body;

    return reqObj;
}