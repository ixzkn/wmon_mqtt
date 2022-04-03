Simple program for monitoring windows in Windows as Home Assistant binary_sensor entities via MQTT.
Does not poll, uses SetWinEventHook to ensure instant response.  Only monitors windows that you specify in the configuration.

I use it to trigger an automation to put the livingroom into movie mode when MPC-HC is opened.

### Configuration format
INI with the following structure:
    [default]
    server=(mqtt server address)
    logfile=(optional log file)
    topic=(optional topic name, defaults to hostname)
    client_id=(optional mqtt client id)
    password=(optional mqtt password)
    homeassistant=(true/false send MQTT registration to HA)
    [name of subtopic]
    class=(window classname to search for)
    title=(window title to search for)
    process=(window process name to search for)
    fullscreen=(true/false only send on when window is fullscreen)
    exact=(true/false only match exactly)

Every section besides [default] will be used to register a binary_sensor entity in HA.  The sensor will be named
based on a combination of topic & the section name.  For example: topic=test, section: [section] will create a sensor test_section.
The sensor will be on when the window is detected.  Windows can be deteced via 3 mechanisms:
1. classname - search based on window class name
2. title - search based on window title
3. process - search based on window process name (exe)

Only using methods #1 and 2 will result in the best performance.  Every single top-level window creation will
be checked against every entry until a match is found.  So it is best to limit list size and search methods.

All searches match any substring in the full title/classname/process; to only match the whole string set exact=true.

### Dependencies
 - paho.mqtt.c
 - paho.mqtt.cpp
 - inipp

### Building
1. get submodules (will pull dependencies)
3. build with cmake:
   1. cmake build
   2. cmake --build build

