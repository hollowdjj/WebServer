cmake_minimum_required(VERSION 3.12)
project(WebServer)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(./include)
include_directories(./third_party)
aux_source_directory(./src SRC_LIST)

add_executable(WebServer ${SRC_LIST})
add_executable(client ./test/HttpClient.cpp)
add_executable(Test ./test/Test.cpp)