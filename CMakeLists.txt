cmake_minimum_required(VERSION 3.15)

project(HttpServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server
    src/main.cpp
    src/Socket.cpp
    src/HTTPResponse.cpp
    src/HTTPRequest.cpp
    src/Router.cpp
    src/Server.cpp
    src/Logger.cpp
)

target_include_directories(server PRIVATE include)