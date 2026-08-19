#pragma once

#include <string>

class ClientConnection
{
public:
    int fd;
    std::string readBuffer;
    ClientConnection(const int fd) : fd(fd){
    };
};