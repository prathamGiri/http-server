#include "Socket.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(){
    // STEP 1 : Create the server socket
    Socket server;

    // STEP 2 : bind the socket to a port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if(bind(server.getFD(), (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        std::cerr << "Binding failed" << std::endl;
        return 1;
    }
    std::cout << "Successfully binded" << std::endl;

    // STEP 3 : start listening 
    if(listen(server.getFD(), 5) < 0){
        std::cerr << "Failed to listen!" << std::endl;
        return 1;
    }
    std::cout << "Listening on port 8080..." << std::endl;

    // STEP 4 : create client
    sockaddr_in clientAddress;
    socklen_t clientAddrSize = sizeof(clientAddress);

    int client_fd = accept(server.getFD(), (sockaddr*)&clientAddress, &clientAddrSize);

    if(client_fd < 0){
        std::cerr << "Client Connection failed!" << std::endl;
        return 1;
    }
    std::cout << "Client Connected!!" << std::endl;

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
        return 1;
    }

    std::cout << "Received: " << buffer << std::endl;

    HTTPRequest request = HTTPRequest::parse(buffer);

    HTTPResponse resObj;

    resObj.setHeader("Content-Type", "text/plain");
    resObj.setHeader("Connection", "close");
    resObj.setBody("Hello from my C++ HTTP Server!");

    std::string response = resObj.toString();
    
    // Step 6 : echo back the data
    // ssize_t bytesSent = send(
    //     client_fd,
    //     buffer,
    //     bytesReceived,
    //     0
    // );

    // std::string body = "Hello from my C++ HTTP Server!";

    // std::string response =
    //     "HTTP/1.1 200 OK\r\n"
    //     "Content-Type: text/plain\r\n"
    //     "Content-Length: " + std::to_string(body.size()) + "\r\n"
    //     "Connection: close\r\n"
    //     "\r\n" +
    //     body;

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

    return 0;
}