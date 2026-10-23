#pragma once
#include <Arduino.h>
#include <time.h>

// Over-the-air updates from the project's GitHub releases. tools/make_manifest.py writes the manifest that
// otaCheck() reads. The device implementation is ota.cpp; the simulator has stand-ins (sim/ota_sim.cpp).

struct OtaInfo {
  bool ok = false;     // the manifest was fetched and has an entry for this board
  bool newer = false;  // ... and its version is higher than the running one
  String version, url, sha256;
  size_t size = 0;
  String error;        // set when !ok (English; show it through T())
  time_t checked = 0;  // when the check ran
};

// Fetches the manifest of the latest release. Needs WiFi and the clock (NTP). Blocking, a few seconds.
OtaInfo otaCheck();

// The result of the last otaCheck() (checked == 0: none yet).
const OtaInfo& otaLast();

// Downloads info.url into the idle app slot, checks its SHA-256 and size, and restarts into it. Returns only
// on failure (then `error` says why and the running firmware is untouched). Blocking, 20-40 s; draws the
// progress screen and brings the dashboard back on failure.
bool otaInstall(const OtaInfo& info, String& error);

// Call from loop() once online. Confirms a freshly updated firmware after it has run for a while and, when
// "Install updates automatically" is on, checks for a new release daily and installs it in the chosen hour.
// canInstall: false while the panel should not be interrupted (settings screen open, a fetch error showing).
void otaTick(bool canInstall);
