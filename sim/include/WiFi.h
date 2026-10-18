// WiFi shim (sim only).
#pragma once
#include <Arduino.h>

#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6

extern bool sim_wifi_connected;  // sim_main.cpp

struct IPAddress {
  uint8_t o[4] = {0, 0, 0, 0};
  uint8_t operator[](int i) const { return o[i]; }
  String toString() const { return "localhost:8080"; /* sim web server address */ }
};

struct WiFiShim {
  void mode(int) {}
  void begin() {}
  int status() const { return sim_wifi_connected ? WL_CONNECTED : WL_DISCONNECTED; }
  String SSID() const { return "SimNetwork"; }
  IPAddress localIP() const { return {}; }
};
static WiFiShim WiFi;
