cmake_minimum_required(VERSION 3.6)
project(DFT)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -o")

set(SOURCE_FILES client.c mainServer.c server.c dataStructures.h)
add_executable(DFT ${SOURCE_FILES} dataStructures.h)