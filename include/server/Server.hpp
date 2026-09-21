#include "routing/Router.hpp"
#include "net/Socket.hpp"
#include "net/ClientConnection.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ResultQueue.hpp"
#include "routing/StaticFileHandler.hpp"
#include "logger/Logger.hpp"
#include "net/EpollEventLoop.hpp"
#include "server/ServerConfig.hpp"

#include <unordered_map>
#include <memory>

class Server{
private:
    ServerConfig config_;
    int epoll_fd;
    int notify_fd;

    std::unordered_map<int, std::unique_ptr<ClientConnection>> clients;
    
    std::shared_ptr<Router> router_;
    std::shared_ptr<ThreadPool> threadPool_;
    std::shared_ptr<ResultQueue> resultQueue_;
    std::shared_ptr<EpollEventLoop> epollInstance_;
    std::shared_ptr<Logger> serverLogger_;
    std::shared_ptr<Logger> clientLogger_;
    std::shared_ptr<StaticFileHandler> staticFileHandler_;
public:
    Server(
        ServerConfig config,
        std::shared_ptr<Router> router,
        std::shared_ptr<ThreadPool> threadPool,
        std::shared_ptr<ResultQueue> resultQueue,
        std::shared_ptr<EpollEventLoop> epollInstance,
        std::shared_ptr<Logger> serverLogger,
        std::shared_ptr<Logger> clientLogger,
        std::shared_ptr<StaticFileHandler> staticFileHandler
    );

    void handleWorkerResults();
    void setRouter(Router router);
    void start();
    void handleClient(int client_fd);
    void handleWrite(int client_fd);
    void closeClient(int client_fd);
};