#include "Router.hpp"
#include "Socket.hpp"

class Server{
private:
    int port;
    int epoll_fd;
    
    Router router;

public:
    Server(const int port);

    void setRouter(Router router);

    void start();

    void handleClient(int client_fd);
};