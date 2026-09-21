#include "net/Socket.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

Socket::Socket(){
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) std::cerr << "Error creating socket!!" << std::endl;
}

Socket::~Socket(){
    if(fd != -1){
        close(fd);
    }
}

int Socket::getFD() const{
    return fd;
}

