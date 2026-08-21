#include "Router.hpp"
#include "Socket.hpp"
#include "ClientConnection.hpp"
#include <unordered_map>
#include <memory>

class Server{
private:
    int port;
    int epoll_fd;
    
    Router router;
    std::unordered_map<int, std::unique_ptr<ClientConnection>> clients;
    // When the unique_ptr is destroyed, the ClientConnection is automatically destroyed too.
public:
    Server(const int port) : port(port){
    };

    void setRouter(Router router);
    void start();
    void handleClient(int client_fd);
    void handleWrite(int client_fd);
    void closeClient(int client_fd);
};