#include "core/HTTPRequest.hpp"
#include "core/HTTPResponse.hpp"
#include "routing/Router.hpp"
#include "server/Server.hpp"
#include "server/ServerConfig.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ResultQueue.hpp"
#include "net/EpollEventLoop.hpp"
#include "logger/Logger.hpp"
#include "routing/StaticFileHandler.hpp"

int main(){
    ServerConfig config = ServerConfigBuilder()
        .withPort(8080)
        .withMaxThreads(4)
        .withMaxEpollEvents(10)
        .withServerLogFile("/var/log/http-server/ServerLogs.log")
        .withClientLogFile("/var/log/http-server/ClientLogs.log")
        .withStaticFileDir("../static")
        .build();
        
    auto router = std::make_shared<Router>();

    router->get(
        "/",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the home page");

            return response;
        }
    );

    router->get(
        "/about",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the about page");

            return response;
        }
    );

    router->get(
        "/help",
        [](const HTTPRequest&){
            HTTPResponse response;
            response.setStatus(202, "OK");
            response.setHeader("Content-Type", "text/plain");
            response.setBody("This is the help page");

            return response;
        }
    );

    router->post(
        "/login",
        [](const HTTPRequest& request)
        {
            HTTPResponse response;
            response.setBody("Login Successful using:" + request.body);

            return response;
        }
    );

    auto threadPool = std::make_shared<ThreadPool>(config.maxThreads);
    auto resultQueue = std::make_shared<ResultQueue>();
    auto epollInstance = std::make_shared<EpollEventLoop>(config.maxEpollEvents);
    auto serverLogger = std::make_shared<Logger>(config.serverLogFile);
    auto clientLogger = std::make_shared<Logger>(config.clientLogFile);
    auto staticFileHandler = std::make_shared<StaticFileHandler>(config.staticFileDir);

    Server server(config, router, threadPool, resultQueue, epollInstance, serverLogger, clientLogger, staticFileHandler);

    // server.setRouter(router);

    server.start();

    return 0;
}