#include "server/Server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <fcntl.h>
#include <cerrno>
#include <sys/eventfd.h>

#include <iostream>
#include <string>
#include <thread>

Server::Server(
        ServerConfig config,
        std::shared_ptr<Router> router,
        std::shared_ptr<ThreadPool> threadPool,
        std::shared_ptr<ResultQueue> resultQueue,
        std::shared_ptr<EpollEventLoop> epollInstance,
        std::shared_ptr<Logger> serverLogger,
        std::shared_ptr<Logger> clientLogger,
        std::shared_ptr<StaticFileHandler> staticFileHandler
    ) : config_(std::move(config)),
        router_(std::move(router)),
        threadPool_(std::move(threadPool)),
        resultQueue_(std::move(resultQueue)),
        epollInstance_(std::move(epollInstance)),
        serverLogger_(std::move(serverLogger)),
        clientLogger_(std::move(clientLogger)),
        staticFileHandler_(std::move(staticFileHandler)){};

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
    epollInstance_->removeFD(client_fd);

    close(client_fd);
    clients.erase(client_fd);
}

void Server::handleWorkerResults() {
    std::vector<ResultItem> results;
    resultQueue_->drainInto(results);

    for (auto& result : results) {
        // the client may have disconnected while the worker was busy — check first
        auto it = clients.find(result.client_fd);
        if (it == clients.end()) continue;   // gone, discard the result

        auto& client = *it->second;
        client.writeBuffer += result.responseString;

        epollInstance_->changeFdMode(result.client_fd, IOEventMode::ReadWrite);
    }
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
        serverLogger_->log(LogLevels::ERROR, "send", "/", 500, "Send failed");

        closeClient(client_fd);
    }else if (bytesSent > 0)
    {
        client.writeBuffer.erase(0,bytesSent);
        if (client.writeBuffer.empty())
        {
            epollInstance_->changeFdMode(client_fd, IOEventMode::Read);
        }
        
    }
}

void Server::handleClient(int client_fd){
    char buffer[4096];

    ssize_t bytesReceived = recv(
        client_fd,
        buffer,
        sizeof(buffer),
        0
    );
    if (bytesReceived > 0)
    {
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
                if (client.readBuffer.size() > config_.MAX_HEADER_SIZE)
                {
                    clientLogger_->log(LogLevels::ERROR, "MAX_HEADER_SIZE", "/", 431, "Request Header Fields Too Large");
                    client.writeBuffer += sendErrorResponse(431, "Request Header Fields Too Large").toString();
                    client.closeAfterWrite = true;
                    client.readBuffer.clear();   // stop growing it further

                    epollInstance_->changeFdMode(client_fd, IOEventMode::Write);
                }
                break;
            }
            std::size_t requestSize = requestEnd+4;
            std::string headers = client.readBuffer.substr(0, requestSize);
            std::size_t contentLength = 0;
            try
            {
                std::size_t contentPos = headers.find("Content-Length:");
                if (contentPos != std::string::npos)
                {
                    contentPos+=std::string("Content-Length:").length();
                    while (contentPos < requestSize && headers[contentPos] == ' ')
                    {
                        contentPos++;
                    }                
                    contentLength = std::stoul(headers.substr(contentPos));
                }
            }
            catch(const std::exception& e)
            {
                clientLogger_->log(LogLevels::ERROR, "invalid_content_length", "/", 400, std::string("Bad Request: ") + e.what());
                client.writeBuffer += sendErrorResponse(400, "Bad Request!").toString();
                client.closeAfterWrite = true;

                epollInstance_->changeFdMode(client_fd, IOEventMode::Write);
                break;
            }
            if(contentLength > config_.MAX_BODY_SIZE){
                clientLogger_->log(LogLevels::ERROR, "MAX_BODY_SIZE", "/", 413, "Payload Too Large");
                client.writeBuffer += sendErrorResponse(413, "Payload Too Large").toString();
                client.closeAfterWrite = true;
                client.readBuffer.clear();   // stop growing it further

                epollInstance_->changeFdMode(client_fd, IOEventMode::Write);
                break;
            }
            if(client.readBuffer.length() - requestSize < contentLength){
                break;
            }
            requestSize+=contentLength;
            std::string requestData = client.readBuffer.substr(0, requestSize);
            client.readBuffer.erase(0, requestSize);

            threadPool_->enqueue([this, requestData, client_fd](){
                std::string responseStr;
                try
                {
                    HTTPRequest request = HTTPRequest::parse(requestData);
                    clientLogger_->log(LogLevels::INFO, request.method, request.path, 200, request.body);
                    HTTPResponse resObj;

                    bool handled = router_->tryRoute(request, resObj); 

                    if (!handled && request.method == "GET")
                    {
                        resObj = staticFileHandler_->serve(request);
                        handled = true;
                    }
                    if (handled)
                    {
                        responseStr = resObj.toString();
                    }else
                    {
                        responseStr = sendErrorResponse(404, "Not Found").toString();
                    }             
                }
                catch(const std::invalid_argument& e)
                {
                    clientLogger_->log(LogLevels::ERROR, "invalid_argument", "/", 400, std::string("Bad Request (Invalid Argument): ")+e.what());
                    responseStr = sendErrorResponse(400, "Bad Request!").toString();
                }
                catch(const std::out_of_range& e){
                    clientLogger_->log(LogLevels::ERROR, "out_of_range", "/", 400, std::string("Bad Request (out of range):") + e.what());
                    responseStr = sendErrorResponse(400, "Bad Request!").toString();
                }
                catch(const std::exception& e){
                    clientLogger_->log(LogLevels::ERROR, "exception", "/", 500, std::string("Internal Server Error:") + e.what());
                    responseStr = sendErrorResponse(500, "Internal Server Error!").toString();
                }
                catch(...)
                {
                    clientLogger_->log(LogLevels::ERROR, "exception", "/", 500, "Unexpected Error Ocured:");
                    responseStr = sendErrorResponse(500, "Internal Server Error!").toString();
                }

                resultQueue_->push({client_fd, std::move(responseStr)});

                uint64_t  one = 1;
                write(notify_fd, &one, sizeof(one));
            });
        }
        if (client.closeAfterWrite)
        {
            clientLogger_->log(LogLevels::INFO, "handleClient", "/", 500, "Client disconnected!");
            closeClient(client_fd);
        } // if client sends multiple requests, one request is with worker, 
        // second request is flagged as content-length-error, this will set
        // the closeAfterWrite flag to true and close client even before the 
        // first worker returns result.
        
    }else if (bytesReceived == 0)
    {
        clientLogger_->log(LogLevels::INFO, "handleClient", "/", 500, "Client disconnected!");
        closeClient(client_fd);
    }else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        else
        {
            serverLogger_->log(LogLevels::ERROR, "recv", "/", 500, "recv() failed");
            closeClient(client_fd);
        }
    }
}

void Server::start(){
    Socket socket;
    int listenfd = socket.getFD();
    setNonBlocking(listenfd);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config_.port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if(bind(listenfd, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        serverLogger_->log(LogLevels::ERROR, "bind", "/", 500, "Socket Binding Failed");
        return;
    }
    serverLogger_->log(LogLevels::INFO, "bind", "/", 200, "Socket Successfully Binded");

    if(listen(listenfd, 5) < 0){
        serverLogger_->log(LogLevels::ERROR, "listen", "/", 500, "Failed to listen on port: "+ std::to_string(config_.port));
        return;
    }
    serverLogger_->log(LogLevels::INFO, "listen", "/", 200, "Listening on port: "+std::to_string(config_.port));
    if(epollInstance_->init()) {
        serverLogger_->log(LogLevels::ERROR, "epoll_create", "/", 500, "Failed to create epoll");
        return;
    }

    if(epollInstance_->registerFD(listenfd, IOEventMode::Read)) {
        serverLogger_->log(LogLevels::ERROR, "epoll_ctl", "/", 500, "Failed to add socket fd to epoll");
        return;
    }

    notify_fd = eventfd(0, EFD_NONBLOCK);
    if (notify_fd == -1) {
        serverLogger_->log(LogLevels::ERROR, "eventfd", "/", 500, "Failed to create notify_fd");
        return;
    }
    if(epollInstance_->registerFD(notify_fd, IOEventMode::Read)) {
        serverLogger_->log(LogLevels::ERROR, "epoll_ctl", "/", 500, "Failed to add notify fd to epoll");
        return;
    };

    while(true){
        std::vector<IOEvent> eventsList;
        try
        {
            eventsList = epollInstance_->getEvents();
        }
        catch(const std::exception& e)
        {
            serverLogger_->log(LogLevels::ERROR, "epoll_wait", "/", 500, e.what());
            return;
        }
        
        for (auto event : eventsList)
        {   
            if (event.fd == listenfd)
            {
                serverLogger_->log(LogLevels::INFO, "connect", "/", 200, "New connection!");

                sockaddr_in clientAddress;
                socklen_t clientAddrSize = sizeof(clientAddress);

                int client_fd = accept(listenfd, (sockaddr*)&clientAddress, &clientAddrSize);
                if(client_fd < 0){
                    serverLogger_->log(LogLevels::ERROR, "accept", "/", 500, "Client Connection Failed!");
                    clientLogger_->log(LogLevels::ERROR, "accept", "/", 500, "Client Connection Failed!");
                    continue;
                }
                setNonBlocking(client_fd);
                serverLogger_->log(LogLevels::INFO, "accept", "/", 200, "Client Accepted");
                clientLogger_->log(LogLevels::INFO, "accept", "/", 200, "Client Accepted");

                auto client = std::make_unique<ClientConnection>(client_fd);
                clients[client_fd] = std::move(client); // to transfer the ownership of a unique pointer, use move

                if(epollInstance_->registerFD(client_fd, IOEventMode::Read)) {
                    serverLogger_->log(LogLevels::ERROR, "epoll_ctl", "/", 500, "Failed to add client fd to epoll");
                    close(client_fd);
                    continue;
                }
            }
            else if (event.fd == notify_fd)
            {
                uint64_t val;
                read(notify_fd, &val, sizeof(val));
                handleWorkerResults();
            }
            else
            {
                if (event.readable)
                {
                    handleClient(event.fd);
                }
                if (event.writable)
                {
                    handleWrite(event.fd);
                }
            }
        }
    }
}
