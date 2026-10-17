#pragma once
#include <Arduino.h>

// Starts the settings web page (http://<ip>/ and http://glucopanel.local/). Call once WiFi is up.
void configServerBegin();
void configServerHandle();  // call from loop()
bool configServerTakeChanged();  // true once after settings were saved (then resets)

// The code that opens the settings page (see config_server.cpp). It is shown on the panel's screens and
// in their QR codes; configServerNewCode() replaces it with a fresh one.
String configServerCode();
String configServerNewCode();
