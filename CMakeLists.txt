cmake_minimum_required(VERSION 3.10)

# set the project name
project(wmon2)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PAHO_BUILD_STATIC ON)
set(PAHO_BUILD_WITH_SSL OFF)
set(PAHO_ENABLE_TESTING FALSE)
set(PAHO_BUILD_SAMPLES OFF)
include(pahomqttc.cmake)
include(pahomqttcpp.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/paho.mqtt.c/include ${CMAKE_CURRENT_BINARY_DIR}/paho.mqtt.cpp/include)

add_executable(wmon2 WIN32 wmon2.cpp wmon2.rc)
target_link_libraries(wmon2 Comctl32 Ws2_32 paho-mqttpp3 paho-mqttc3::MQTTAsync)
