cmake_minimum_required(VERSION 2.8)

project(extract_certain)

aux_source_directory(. DIR_SRCS)

add_executable(extract ${DIR_SRCS})
