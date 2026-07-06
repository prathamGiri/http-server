#include "Socket.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

Socket::Socket(){
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) std::cerr << "Error creating socket!!" << std::endl;
    else std::cout << "Socket Created."  << std::endl;
}

Socket::~Socket(){
    if(fd != -1){
        std::cout << "Closing Socket.." << std::endl;
        close(fd);
    }
}

int Socket::getFD() const{
    return fd;
}

