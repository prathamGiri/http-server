#include "Server.hpp"
#include "Socket.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>

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

    // epoll asks the kernel to notify/wake our process when 
    // one of the registered file descriptors becomes ready 
    // for an event, such as having data available to read.
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll\n";
        return;
    }

    epoll_event serverEvent{};

    serverEvent.events = EPOLLIN;
    serverEvent.data.fd = server.getFD();

    // register the listining client
    if (epoll_ctl(
            epoll_fd,
            EPOLL_CTL_ADD,
            server.getFD(),
            &serverEvent
        ) == -1)
    {
        std::cerr << "Failed to add server socket to epoll\n";
        return;
    }

    epoll_event events[10];

    while(true){
        // wait indefinitely(-1) until something happens
        int event_count = epoll_wait(
            epoll_fd,
            events,
            10,
            -1
        );

        if (event_count == -1)
        {
            std::cerr << "epoll_wait failed\n";
            return;
        }

        for (int i = 0; i < event_count; i++)
        {   
            int fd = events[i].data.fd;
            if (fd == socket.getFD())
            {
                std::cout << "New connection!\n";

                // STEP 4 : create client
                sockaddr_in clientAddress;
                socklen_t clientAddrSize = sizeof(clientAddress);

                int client_fd = accept(socket.getFD(), (sockaddr*)&clientAddress, &clientAddrSize);
                if(client_fd < 0){
                    std::cerr << "Client Connection failed!" << std::endl;
                    continue;
                }
                std::cout << "Accepted client: " << client_fd << std::endl;
            }
            
        }

        // std::thread(
        //     FUNCTION,
        //     OBJECT,
        //     ARGUMENTS...
        // );

        // std::thread(
        //     &Server::handleClient,
        //     this,
        //     client_fd
        // ).detach();
    }
}