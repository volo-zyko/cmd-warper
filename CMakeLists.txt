cmake_minimum_required(VERSION 3.5)

project(cmd-warper)

add_executable(cmd-warper main.c)

install(TARGETS cmd-warper RUNTIME DESTINATION .)
