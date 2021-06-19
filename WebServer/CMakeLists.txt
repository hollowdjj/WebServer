cmake_minimum_required(VERSION 3.19)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(./)
set(SRC_LIST main.cpp Channel.cpp EventLoop.cpp HttpServer.cpp Utility.cpp Epoller.cpp HttpData.cpp)

add_executable(WebServer ${SRC_LIST})
add_executable(client ./test/HttpClient.cpp)