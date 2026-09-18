#include "Router.hpp"
#include "Socket.hpp"
#include "ClientConnection.hpp"
#include "ThreadPool.hpp"
#include "ResultQueue.hpp"
#include "StaticFileHandler.hpp"
#include "logger/Logger.hpp"
#include "net/EpollEventLoop.hpp"

#include <unordered_map>
#include <memory>

class Server{
private:
    int port;
    int epoll_fd;
    int notify_fd;

    std::unordered_map<int, std::unique_ptr<ClientConnection>> clients;
    
    Router router;
    ThreadPool threadPool{4};
    ResultQueue resultQueue;
    EpollEventLoop epollInstance;
    Logger serverLogger{"/var/log/http-server/ServerLogs.log"};
    Logger clientLogger{"/var/log/http-server/ClientLogs.log"};
    StaticFileHandler staticFileHandler{"../static"};
public:
    Server(const int port) : port(port){
    };

    void handleWorkerResults();
    void setRouter(Router router);
    void start();
    void handleClient(int client_fd);
    void handleWrite(int client_fd);
    void closeClient(int client_fd);
};