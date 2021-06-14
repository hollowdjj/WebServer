cmake_minimum_required(VERSION 3.19)
project(WebServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(./)
set(SRC_LIST main.cpp Acceptor.cpp Channel.cpp EventLoop.cpp HttpServer.cpp Utility.cpp Epoller.cpp Epoller.h)

add_executable(WebServer ${SRC_LIST})