#pragma once

#include <vector>
#include <string>

struct ServerConfig
{
    int port = 8080;
    std::size_t MAX_HEADER_SIZE = 8 * 1024;
    std::size_t MAX_BODY_SIZE = 1 * 1024 * 1024;
    int maxThreads = 4;
    int maxEpollEvents = 10;
    std::string serverLogFile = "/var/log/http-server/ServerLogs.log";
    std::string clientLogFile = "/var/log/http-server/ClientLogs.log";
    std::string staticFileDir = "../static";
};

class ServerConfigBuilder
{
private:
    ServerConfig config_;
public:
    ServerConfigBuilder& withPort(int port);
    ServerConfigBuilder& withMaxHeaderSize(std::size_t maxHeaderSize);
    ServerConfigBuilder& withMaxBodySize(std::size_t maxBodySize);
    ServerConfigBuilder& withMaxThreads(int maxThreads);
    ServerConfigBuilder& withMaxEpollEvents(int maxEpollEvents);
    ServerConfigBuilder& withServerLogFile(std::string serverLogFile);
    ServerConfigBuilder& withClientLogFile(std::string clientLogFile);
    ServerConfigBuilder& withStaticFileDir(std::string staticFileDir);

    ServerConfig build();
};
