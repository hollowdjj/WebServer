cmake_minimum_required(VERSION 3.16)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)


include_directories(./include)
aux_source_directory(./src SRC_LIST)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -O0")
  set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/debug)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")

  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -O3")
  set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/release)

else()

  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -O3")
  set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/release)

endif()

add_executable(WebServer ${SRC_LIST})

include(FetchContent)
FetchContent_Declare(	spdlog							#库名字
			GIT_REPOSITORY git@github.com:gabime/spdlog.git	# 仓库地址
			GIT_TAG v1.x 						# 库版本
			SOURCE_DIR ${PROJECT_SOURCE_DIR}/lib/spdlog           # 指定库下载地址
		    )
FetchContent_MakeAvailable(spdlog)
# 项目中使用spdlog
target_link_libraries(WebServer PRIVATE spdlog::spdlog)
