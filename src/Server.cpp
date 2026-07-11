#include "Server.hpp"
#include "Socket.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>

Server::Server(const int port) : port(port) {
}

void Server::setRouter(Router router){
    this->router = router;
}

void Server::handleClient(int client_fd){
    //Step 5 : Receive date from client
    char buffer[4096];

    ssize_t bytesReceived = recv(
        client_fd,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytesReceived < 0)
    {
        std::cerr << "Receive failed!" << std::endl;
        close(client_fd);
        return;
    }

    std::cout << "Received: " << buffer << std::endl;

    HTTPRequest request = HTTPRequest::parse(buffer);

    HTTPResponse resObj = router.route(request);
    std::string response = resObj.toString();

    ssize_t bytesSent = send(
        client_fd,
        response.c_str(),
        response.size(),
        0
    );

    if (bytesSent < 0)
    {
        std::cerr << "Send failed!" << std::endl;
    }

    close(client_fd);
}

void Server::start(){
    // STEP 1 : Create the server socket
    Socket socket;

    // STEP 2 : bind the socket to a port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if(bind(socket.getFD(), (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        std::cerr << "Binding failed" << std::endl;
        return;
    }
    std::cout << "Successfully binded" << std::endl;

    // STEP 3 : start listening 
    if(listen(socket.getFD(), 5) < 0){
        std::cerr << "Failed to listen!" << std::endl;
        return;
    }
    std::cout << "Listening on port 8080..." << std::endl;

    while(true){
        // STEP 4 : create client
        sockaddr_in clientAddress;
        socklen_t clientAddrSize = sizeof(clientAddress);

        int client_fd = accept(socket.getFD(), (sockaddr*)&clientAddress, &clientAddrSize);

        if(client_fd < 0){
            std::cerr << "Client Connection failed!" << std::endl;
            return;
        }
        std::cout << "Client Connected!!" << std::endl;

        std::thread(
            &Server::handleClient,
            this,
            client_fd
        ).detach();
    }
}