cmake_minimum_required(VERSION 3.10)

# set the project name
project(wmon2)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(paho.mqtt.c/build/include)
add_executable(wmon2 WIN32 wmon2.cpp)
target_link_libraries(wmon2 Comctl32 Ws2_32 ../paho.mqtt.c/build/lib/paho-mqttpp3-static ../paho.mqtt.c/build/lib/paho-mqtt3a-static)
