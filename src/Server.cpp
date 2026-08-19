#include "Server.hpp"
#include "Socket.hpp"
#include "ClientConnection.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>

#include <iostream>
#include <string>
#include <thread>

void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        std::cerr << "fcntl F_GETFL failed\n";
        return;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "fcntl F_SETFL failed\n";
    }
}

Server::Server(const int port) : port(port) {
}

void Server::setRouter(Router router){
    this->router = router;
}

void Server::handleWrite(int client_fd){

    auto& client = *clients.at(client_fd);

    if (client.writeBuffer.empty())
    {
        return;
    }
    
    ssize_t bytesSent = send(
        client_fd,
        client.writeBuffer.c_str(),
        client.writeBuffer.size(),
        0
    );

    if (bytesSent < 0)
    {   
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        std::cerr << "Send failed!" << std::endl;

        epoll_ctl(
            epoll_fd,
            EPOLL_CTL_DEL,
            client_fd,
            nullptr
        );

        close(client_fd);
        clients.erase(client_fd);
    }else if (bytesSent > 0)
    {
        client.writeBuffer.erase(0,bytesSent);
        if (client.writeBuffer.empty())
        {
            epoll_event event{};
            event.events = EPOLLIN;
            event.data.fd = client_fd;

            if(epoll_ctl(
                epoll_fd,
                EPOLL_CTL_MOD,
                client_fd,
                &event
            )){
                std::cerr << "Failed to disable EPOLLOUT\n";
            };
        }
        
    }
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
    if (bytesReceived > 0)
    {
        std::cout << "Received: " << 
        std::cout.write(buffer, bytesReceived); << 
        std::endl;

        // append the buffer to the client connection storage
        auto& client = *clients.at(client_fd);

        client.readBuffer.append(
            buffer,
            bytesReceived
        );

        // only parse when complete data received
        if (client.readBuffer.find("\r\n\r\n") != std::string::npos)
        {
            HTTPRequest request = HTTPRequest::parse(client.readBuffer);

            HTTPResponse resObj = router.route(request);
            client.writeBuffer = resObj.toString();

            epoll_event event{};
            event.events = EPOLLIN | EPOLLOUT;
            event.data.fd = client_fd;

            if (epoll_ctl(
                    epoll_fd,
                    EPOLL_CTL_MOD,
                    client_fd,
                    &event
                ) == -1)
            {
                std::cerr << "Failed to modify client event\n";
            }
        }
    }else if (bytesReceived == 0)
    {
        std::cout << "Client disconnected\n";

        epoll_ctl(
            epoll_fd,
            EPOLL_CTL_DEL,
            client_fd,
            nullptr
        );

        close(client_fd);

        // Remove the ClientConnection from our map
        clients.erase(client_fd);
    }else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        else
        {
            std::cerr << "recv failed\n";

            epoll_ctl(
                epoll_fd,
                EPOLL_CTL_DEL,
                client_fd,
                nullptr
            );

            close(client_fd);
            // Remove the ClientConnection from clients map
            clients.erase(client_fd);
        }
    }
}

void Server::start(){
    // STEP 1 : Create the server socket
    Socket socket;
    setNonBlocking(socket.getFD());

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
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll\n";
        return;
    }

    epoll_event serverEvent{};

    serverEvent.events = EPOLLIN;
    serverEvent.data.fd = socket.getFD();

    // register the listining client
    if (epoll_ctl(
            epoll_fd,
            EPOLL_CTL_ADD,
            socket.getFD(),
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
                setNonBlocking(client_fd);
                std::cout << "Accepted client: " << client_fd << std::endl;

                // create a client connection object to store the segmented buffer
                auto client = std::make_unique<ClientConnection>(client_fd);
                clients[client_fd] = std::move(client); // to transfer the ownership of a unique pointer, use move
                
                epoll_event clientEvent{};

                clientEvent.events = EPOLLIN;
                clientEvent.data.fd = client_fd;

                if (epoll_ctl(
                        epoll_fd,
                        EPOLL_CTL_ADD,
                        client_fd,
                        &clientEvent
                    ) == -1)
                {
                    std::cerr << "Failed to add client socket to epoll\n";
                    close(client_fd);
                    continue;
                }
            }else
            {
                if (events[i].events & EPOLLIN)
                {
                    handleClient(fd);
                }
                if (events[i].events & EPOLLOUT)
                {
                    handleWrite(fd);
                }
            }
        }
    }
}