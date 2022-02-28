cmake_minimum_required(VERSION 3.10)

# set the project name
project(wmon2)


add_executable(wmon2 WIN32 wmon2.cpp)
target_link_libraries(wmon2 Comctl32.lib)
