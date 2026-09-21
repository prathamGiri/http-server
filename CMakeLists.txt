cmake_minimum_required(VERSION 3.15)

project(HttpServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server
    src/main.cpp
    src/net/Socket.cpp
    src/core/HTTPResponse.cpp
    src/core/HTTPRequest.cpp
    src/routing/Router.cpp
    src/server/Server.cpp
    src/server/ServerConfig.cpp
    src/logging/Logger.cpp
    src/concurrency/ThreadPool.cpp
    src/routing/StaticFileHandler.cpp
    src/net/EpollEventLoop.cpp
)

target_include_directories(server PRIVATE include)