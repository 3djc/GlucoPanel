#pragma once

// Starts the settings web page (http://<ip>/ and http://glucopanel.local/). Call once WiFi is up.
void configServerBegin();
void configServerHandle();  // call from loop()
bool configServerTakeChanged();  // true once after settings were saved (then resets)
