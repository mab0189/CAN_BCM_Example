cmake_minimum_required(VERSION 3.20)
project(CAN_BCM_Example C)

set(CMAKE_C_STANDARD 99)

include_directories(include)
add_executable(CAN_BCM_Example src/CANFD_BCM_Example.c src/CANFD_BCM_Socket.c)
