cd paho.mqtt.c
mkdir build
cmake -GNinja -Bbuild -H. -DPAHO_BUILD_STATIC=ON -DPAHO_ENABLE_TESTING=FALSE -DCMAKE_INSTALL_PREFIX="F:\Projects\C++\wmon_mqtt\paho.mqtt.c\build"
cmake --build build/ --target install
cd ../paho.mqtt.cpp
mkdir build
cmake -Bbuild -H. -DCMAKE_INSTALL_PREFIX="..\paho.mqtt.c\build" -DPAHO_BUILD_SAMPLES=OFF -DPAHO_WITH_SSL=OFF -DCMAKE_PREFIX_PATH="..\paho.mqtt.c\build" -DPAHO_BUILD_STATIC=ON
cmake --build build/ --target install
