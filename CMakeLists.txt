cmake_minimum_required(VERSION 3.5)
project(CHROOT_SHELL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c11 -Wall -Wextra")

set(SOURCE_FILES main.c)
add_executable(CHROOT_SHELL ${SOURCE_FILES})