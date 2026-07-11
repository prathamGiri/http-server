cmake_minimum_required(VERSION 3.15)

project(HttpServer)

add_executable(server
    src/main.cpp
    src/Socket.cpp
    src/HTTPResponse.cpp
    src/HTTPRequest.cpp
    src/Router.cpp
    src/Server.cpp
)

target_include_directories(server PRIVATE include)