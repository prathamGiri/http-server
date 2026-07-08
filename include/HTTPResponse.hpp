#include <string>
#include <unordered_map>

class HTTPResponse{
private:
    int statusCode;
    std::string statusText;
    std::unordered_map<std::string, std::string> responseHeaders;
    std::string responseBody;

public:
    HTTPResponse();
    void setStatus(int code, const std::string& msg);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    std::string toString() const;
};