cmake_minimum_required(VERSION 2.8)
project(network3-rip)

set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#添加头文件搜索路径
include_directories(/usr/local/include)

#添加库文件搜索路径
link_directories(/usr/local/lib)

add_executable(router  weblab/rip.c  weblab/router.c)
target_link_libraries(router)

add_executable(agent  weblab/rip.c	weblab/agent.c)
target_link_libraries(agent)

find_package(Threads)
target_link_libraries(router ${CMAKE_THREAD_LIBS_INIT})
