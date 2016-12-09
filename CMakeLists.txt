cmake_minimum_required(VERSION 3.6)
project(DFT)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -o")

set(SOURCE_FILES client.c mainServer.c server.c dataStructures.h dataStructures.c)



set(SOURCE_FILES2
        client.c
        dataStructures.c
        dataStructures.h
)

set(SOURCE_FILES3
        mainServer.c
        dataStructures.c
        dataStructures.h server.h)



add_executable(client ${SOURCE_FILES2})
add_executable(server ${SOURCE_FILES3} server.h)


target_link_libraries(client m)
target_link_libraries(server m)