#include "Server.hpp"
#include "Socket.hpp"
#include "ClientConnection.hpp"
#include "Logger.hpp"

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

constexpr std::size_t MAX_HEADER_SIZE = 8 * 1024;
constexpr std::size_t MAX_BODY_SIZE = 1 * 1024 * 1024;

Logger serverLogger;
Logger clientLogger("/var/log/ClientLogs.log");

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

HTTPResponse sendErrorResponse(int statusCode, std::string statusText){
    HTTPResponse errRes;
    errRes.setStatus(statusCode, statusText);
    errRes.setHeader("Content-Type", "text/plain");
    errRes.setBody(std::to_string(statusCode) + " " + statusText);
    return errRes;
}

void Server::closeClient(int client_fd){
    epoll_ctl(
        epoll_fd,
        EPOLL_CTL_DEL,
        client_fd,
        nullptr
    );

    close(client_fd);
    clients.erase(client_fd);
}

void Server::setRouter(Router router){
    this->router = router;
}

void Server::handleWrite(int client_fd){

    auto& client = *clients.at(client_fd);

    if (client.writeBuffer.empty())
    {
        if (client.closeAfterWrite)
        {
            closeClient(client_fd);
        }
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

        closeClient(client_fd);
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
        sizeof(buffer),
        0
    );
    if (bytesReceived > 0)
    {
        std::cout << "Received: ";
        std::cout.write(buffer, bytesReceived);
        std::cout << "\n";

        // append the buffer to the client connection storage
        auto& client = *clients.at(client_fd);

        client.readBuffer.append(
            buffer,
            bytesReceived
        );

        // if multiple requests at once
        while (true)
        {
            // only parse when complete data received
            std::size_t requestEnd = client.readBuffer.find("\r\n\r\n");
            if (requestEnd == std::string::npos)
            {
                if (client.readBuffer.size() > MAX_HEADER_SIZE)
                {
                    clientLogger.log(3, "MAX_HEADER_SIZE", "/", 431, "Request Header Fields Too Large");
                    client.writeBuffer += sendErrorResponse(431, "Request Header Fields Too Large").toString();
                    client.closeAfterWrite = true;
                    client.readBuffer.clear();   // stop growing it further

                    epoll_event event{};
                    event.events = EPOLLOUT;     // no more EPOLLIN — ignore further input
                    event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event);
                }
                break;
            }
            try
            {
                std::size_t requestSize = requestEnd+4;
                std::string headers = client.readBuffer.substr(0, requestSize);
                std::size_t contentLength = 0;
                std::size_t contentPos = headers.find("Content-Length:");
                if (contentPos != std::string::npos)
                {
                    contentPos+=std::string("Content-Length:").length();
                    while (contentPos < requestSize && headers[contentPos] == ' ')
                    {
                        contentPos++;
                    }
                    contentLength = std::stoul(headers.substr(contentPos));
                    if(contentLength > MAX_BODY_SIZE){
                        clientLogger.log(3, "MAX_BODY_SIZE", "/", 413, "Payload Too Large");
                        client.writeBuffer += sendErrorResponse(413, "Payload Too Large").toString();
                        client.closeAfterWrite = true;
                        client.readBuffer.clear();   // stop growing it further

                        epoll_event event{};
                        event.events = EPOLLOUT;     // no more EPOLLIN — ignore further input
                        event.data.fd = client_fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event);
                        break;
                    }
                    if(client.readBuffer.length() - requestSize < contentLength){
                        break;
                    }
                    requestSize+=contentLength;
                }
                std::string requestData = client.readBuffer.substr(0, requestSize);
                client.readBuffer.erase(0, requestSize);

                HTTPRequest request = HTTPRequest::parse(requestData);
                clientLogger.log(1, request.method, request.path, 200, request.body);
                HTTPResponse resObj = router.route(request);
                client.writeBuffer += resObj.toString();
            }
            catch(const std::invalid_argument& e)
            {
                clientLogger.log(3, "invalid_argument", "/", 400, std::string("Bad Request (Invalid Argument): ")+e.what());
                std::cerr << "Bad Request (Invalid Argument):" << e.what() << "\n";
                client.writeBuffer += sendErrorResponse(400, "Bad Request!").toString();
            }
            catch(const std::out_of_range& e){
                clientLogger.log(3, "out_of_range", "/", 400, std::string("Bad Request (out of range):") + e.what());
                std::cerr << "Bad Request (out of range):" << e.what() << "\n";
                client.writeBuffer += sendErrorResponse(400, "Bad Request!").toString();
            }
            catch(const std::exception& e){
                clientLogger.log(3, "exception", "/", 500, std::string("Internal Server Error:") + e.what());
                std::cerr << "Internal Server Error:" << e.what() << "\n";
                client.writeBuffer += sendErrorResponse(500, "Internal Server Error!").toString();
            }
            catch(...)
            {
                clientLogger.log(3, "exception", "/", 500, "Unexpected Error Ocured:");
                std::cerr << "Unexpected Error Ocured:" << "\n";
                client.writeBuffer += sendErrorResponse(500, "Internal Server Error!").toString();
            }
        }
        if(!client.writeBuffer.empty()){
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
        }else if (client.closeAfterWrite)
        {
            closeClient(client_fd);
        }
        
    }else if (bytesReceived == 0)
    {
        std::cout << "Client disconnected\n";
        closeClient(client_fd);
    }else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        else
        {
            std::cerr << "recv failed\n";
            closeClient(client_fd);
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
        serverLogger.log(3, "bind", "/", 500, "Socket Binding Failed");
        std::cerr << "Socket Binding Failed" << std::endl;
        return;
    }
    serverLogger.log(1, "bind", "/", 200, "Socket Successfully Binded");
    std::cout << "Socket Successfully Binded" << std::endl;

    // STEP 3 : start listening 
    if(listen(socket.getFD(), 5) < 0){
        serverLogger.log(3, "listen", "/", 500, "Failed to listen on port: "+ std::to_string(port));
        std::cerr << "Failed to listen!" << std::endl;
        return;
    }
    serverLogger.log(1, "listen", "/", 200, "Listening on port: "+std::to_string(port));
    std::cout << "Listening on port" << port << "..." << std::endl;

    // epoll asks the kernel to notify/wake our process when 
    // one of the registered file descriptors becomes ready 
    // for an event, such as having data available to read.
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        serverLogger.log(3, "epoll_create", "/", 500, "Failed to create epoll");
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
        serverLogger.log(3, "epoll_ctl", "/", 500, "Failed to add server socket to epoll");
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
            serverLogger.log(3, "epoll_wait", "/", 500, "epoll_wait failed");
            std::cerr << "epoll_wait failed\n";
            return;
        }

        for (int i = 0; i < event_count; i++)
        {   
            int fd = events[i].data.fd;
            if (fd == socket.getFD())
            {
                serverLogger.log(1, "connect", "/", 200, "New connection!");
                std::cout << "New connection!\n";

                // STEP 4 : create client
                sockaddr_in clientAddress;
                socklen_t clientAddrSize = sizeof(clientAddress);

                int client_fd = accept(socket.getFD(), (sockaddr*)&clientAddress, &clientAddrSize);
                if(client_fd < 0){
                    serverLogger.log(3, "accept", "/", 500, "Client Connection Failed!");
                    clientLogger.log(3, "accept", "/", 500, "Client Connection Failed!");
                    std::cerr << "Client Connection failed!" << std::endl;
                    continue;
                }
                setNonBlocking(client_fd);
                serverLogger.log(1, "accept", "/", 200, "Client Accepted");
                clientLogger.log(1, "accept", "/", 200, "Client Accepted");
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
                    serverLogger.log(3, "epoll_ctl", "/", 500, "Failed to add client socket to epoll");
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