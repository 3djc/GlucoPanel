// WiFiManager shim (sim only): autoConnect() runs the AP callback, then waits for
// Enter in the SDL window (standing in for the user finishing the portal).
// SIM_NOWAIT=1 returns immediately.
#pragma once
#include <Arduino.h>

bool sim_wait_enter();  // defined in sim_main.cpp
extern bool sim_wifi_saved;      // SIM_WIFI=fail: stored credentials that cannot be joined
extern bool sim_wifi_connected;

class WiFiManager {
 public:
  void setAPCallback(void (*cb)(WiFiManager*)) { cb_ = cb; }
  bool getWiFiIsSaved() { return sim_wifi_saved; }
  String getWiFiSSID() { return "HomeNetwork"; }
  void resetSettings() { sim_wifi_saved = false; }
  bool autoConnect(const char*) {
    if (cb_) cb_(this);
    return sim_wifi_connected = sim_wait_enter();
  }
 private:
  void (*cb_)(WiFiManager*) = nullptr;
};
