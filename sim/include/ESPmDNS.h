// mDNS shim (sim only).
#pragma once
struct MDNSShim {
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, int) {}
};
static MDNSShim MDNS;
