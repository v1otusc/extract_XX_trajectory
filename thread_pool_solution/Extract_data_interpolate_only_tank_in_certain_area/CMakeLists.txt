cmake_minimum_required(VERSION 3.0)

project(extract)

set(CMAKE_CXX_STANDARD 11)

aux_source_directory(. DIR_SRCS)

find_package(Threads) 

add_executable(extract ${DIR_SRCS})

target_link_libraries(extract ${CMAKE_THREAD_LIBS_INIT})
