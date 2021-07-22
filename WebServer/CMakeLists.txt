cmake_minimum_required(VERSION 3.19)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(./src)
aux_source_directory(./src SRC_LIST)
#set(SRC_LIST ./src main.cpp Channel.cpp HttpServer.cpp Utility.cpp EventLoop.cpp HttpData.cpp Timer.cpp)

add_executable(WebServer ${SRC_LIST})
add_executable(client ./test/HttpClient.cpp)
add_executable(Test ./test/Test.cpp)