#include "server/ServerConfig.hpp"

ServerConfigBuilder& ServerConfigBuilder::withPort(int port){
    config_.port = port;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withMaxHeaderSize(std::size_t maxHeaderSize){
    config_.MAX_HEADER_SIZE = maxHeaderSize;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withMaxBodySize(std::size_t maxBodySize){
    config_.MAX_BODY_SIZE = maxBodySize;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withMaxThreads(int maxThreads){
    config_.maxThreads = maxThreads;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withMaxEpollEvents(int maxEpollEvents){
    config_.maxEpollEvents = maxEpollEvents;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withServerLogFile(std::string serverLogFile){
    config_.serverLogFile = serverLogFile;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withClientLogFile(std::string clientLogFile){
    config_.clientLogFile = clientLogFile;
    return *this;
}
ServerConfigBuilder& ServerConfigBuilder::withStaticFileDir(std::string staticFileDir){
    config_.staticFileDir = staticFileDir;
    return *this;
}
ServerConfig ServerConfigBuilder::build(){
    return config_;
}