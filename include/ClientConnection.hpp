#pragma once

#include <string>

class ClientConnection
{
public:
    int fd;
    std::string readBuffer;
    std::string writeBuffer;
    ClientConnection(const int fd) : fd(fd){
    };
};