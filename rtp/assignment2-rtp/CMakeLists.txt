cmake_minimum_required(VERSION 2.8)
project(assignment2-rtp)

set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#添加头文件搜索路径
include_directories(/usr/local/include)

#添加库文件搜索路径
link_directories(/usr/local/lib)

add_executable(sender
		# if you use recommended template, you need to cancel the comment of the next line
		src/rtp.c
        src/util.c
        src/sender.c)
target_link_libraries(sender)

add_executable(receiver
		# if you use recommended template, you need to cancel the comment of the next line
		src/rtp.c
		src/util.c
		src/receiver.c)
target_link_libraries(receiver)

find_package(Threads)
target_link_libraries(sender ${CMAKE_THREAD_LIBS_INIT})


add_executable(opt_sender
		src/opt_rtp.c
        src/util.c
        src/opt_sender.c)
target_link_libraries(opt_sender)

add_executable(opt_receiver
		src/opt_rtp.c
		src/util.c
		src/opt_receiver.c)
target_link_libraries(opt_receiver)

target_link_libraries(opt_sender ${CMAKE_THREAD_LIBS_INIT})