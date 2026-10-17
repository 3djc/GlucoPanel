#include <Arduino.h>
#include <algorithm>
#include <WiFi.h>
#include "config.h"
#include "config_server.h"
#include "display.h"
#include "glucose.h"
#include "ui.h"
#include "lvgl_port.h"
#include "wifi_setup.h"

static bool online = false;
static unsigned long nextFetch = 0;
static Reading lastGood;
static Deltas deltas;
static String lastError;

static void refresh() {
  FetchResult r = fetchGlucose();
  if (r.ok) {
    lastGood = r.reading;
    lastError = "";
  } else {
    lastError = r.error;
  }
  deltas = glucoseDeltas(lastGood);
  drawDashboard(lastGood, deltas, lastError, r.notConfigured, "http://" + WiFi.localIP().toString());

  unsigned long wait = (unsigned long)refreshMinutes() * 60000UL;
  if (!r.ok) {
    wait = std::min(wait, 60000UL);  // retry sooner after an error
  } else if (cfgGet("source") == "dexcom" && lastGood.when) {
    // Dexcom publishes a reading every 5 min: read 5 s after the last one turns 300 s old,
    // then retry every 10 s (60 s once it is over 500 s old) until the new one shows up.
    long age = (long)(time(nullptr) - lastGood.when);
    if (age < 305) wait = (unsigned long)std::max(1L, 305L - age) * 1000UL;
    else wait = age > 500 ? 60000UL : 10000UL;
  }
  nextFetch = millis() + wait;
  if (r.ok) Serial.printf("fetch ok: %d mg/dL, delta last %s%d, delta 15 min %s%d, next fetch in %lu s\n", lastGood.mgdl,
                          deltas.has1 ? "" : "n/a ", deltas.d1, deltas.has15 ? "" : "n/a ", deltas.d15, wait / 1000);
  else Serial.printf("fetch failed: %s (next in %lu s)\n", r.error.c_str(), wait / 1000);
}

void setup() {
  Serial.begin(115200);
  portBegin();
  online = wifiConnect();
  if (online) {
    glucoseInit();
    configServerBegin();
  }
}

void loop() {
  if (online) {
    configServerHandle();
    bool changed = configServerTakeChanged();
    if (changed) lastGood = Reading();  // new source/credentials: drop the old reading
    if (changed || millis() >= nextFetch) refresh();
    dashboardTick();
  }
  delay(2);
}
