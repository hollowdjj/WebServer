cmake_minimum_required(VERSION 3.19)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(./include)
aux_source_directory(./src SRC_LIST)

add_executable(WebServer ${SRC_LIST})
add_executable(client ./test/HttpClient.cpp)
add_executable(Test ./test/Test.cpp)



include(FetchContent)      # 照写，不需要修改
FetchContent_Declare(	spdlog	#库名字
						GIT_REPOSITORY https://gitee.com/liguo_1993/spdlog.git	# 仓库地址
						GIT_TAG v1.x # 库版本
						SOURCE_DIR ${PROJECT_SOURCE_DIR}/lib/spdlog # 指定库下载地址
					)
FetchContent_MakeAvailable(spdlog)
# 项目中使用spdlog
target_link_libraries(WebServer PRIVATE spdlog::spdlog)