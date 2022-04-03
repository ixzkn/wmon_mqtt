# based on: https://gitlab.com/diplomathesis/i40barman
#
# Builds the paho.mqtt.c library
#
# This step is necessary to provide the libraries and
# headers for paho.mqtt.cpp project
#
include(ExternalProject)
set(PAHOMQTTC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/paho.mqtt.c)
set(PAHOMQTTC_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/paho.mqtt.c)

ExternalProject_Add(
    pahomqttc
    PREFIX ${PAHOMQTTC_TARGET_DIR}
    SOURCE_DIR ${PAHOMQTTC_DIR}
     CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PAHOMQTTC_TARGET_DIR}
     CMAKE_ARGS -DPAHO_WITH_SSL=${PAHO_BUILD_WITH_SSL}
     CMAKE_ARGS -DPAHO_BUILD_STATIC=${PAHO_BUILD_STATIC}
)

# The paho.mqtt.cpp project needs to know where the paho.mqtt.c libraries
# and headers are installed, so we provide this information in an additional
# cmake file which is passed to the paho.mqtt.cpp project build
set(PAHOMQTTC_INCLUDE_DIR ${PAHOMQTTC_TARGET_DIR}/include)
set(PAHOMQTTC_LIBRARY_DIR ${PAHOMQTTC_TARGET_DIR}/lib)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fix_pahomqttcpp.cmake.in 
               ${CMAKE_CURRENT_BINARY_DIR}/fix_pahomqttcpp.cmake @ONLY)


add_library(paho-mqttc3::MQTTClient STATIC IMPORTED GLOBAL)
set_target_properties(paho-mqttc3::MQTTClient PROPERTIES
     IMPORTED_LOCATION ${PAHOMQTTC_TARGET_DIR}/lib/paho-mqtt3c-static.lib
     INTERFACE_INCLUDE_DIRECTORIES ${PAHOMQTTC_TARGET_DIR}/include)
add_library(paho-mqttc3::MQTTAsync STATIC IMPORTED GLOBAL)
set_target_properties(paho-mqttc3::MQTTAsync PROPERTIES
     IMPORTED_LOCATION ${PAHOMQTTC_TARGET_DIR}/lib/paho-mqtt3a-static.lib
     INTERFACE_INCLUDE_DIRECTORIES ${PAHOMQTTC_TARGET_DIR}/include)