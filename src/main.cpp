#include <Arduino.h>
#include <algorithm>
#include <WiFi.h>
#include "config.h"
#include "config_server.h"
#include "display.h"
#include "glucose.h"
#include "i18n.h"
#include "input.h"
#include "night.h"
#include "ui.h"
#include "lvgl_port.h"
#include "wifi_setup.h"

static bool online = false;

static const unsigned long CONFIG_SCREEN_MS = 60000;  // the settings screen closes by itself after this

static bool configShown = false;
static unsigned long configSince = 0;

static void openConfig() {
  configShown = true;
  configSince = millis();
  Serial.printf("settings code: %s\n", configServerNewCode().c_str());  // a fresh code each time the screen opens
  dashboardShowConfig("http://" + WiFi.localIP().toString(), configServerCode());
}

static void closeConfig() {
  configShown = false;
  dashboardHideConfig();
}

// E-paper: green short press opens the settings screen, any short press closes it, holding green for
// 3 s erases the WiFi. LCD: a tap opens it; on it, holding the erase button for 3 s erases the WiFi and
// a tap anywhere else closes it. (A touch screen is touched by accident far more easily than a button
// on the back, hence the dedicated button.)
static void handleInput() {
  Input in = inputPoll();
  bool timeout = configShown && millis() - configSince > CONFIG_SCREEN_MS;
#ifdef UI_PROFILE_LCD
  if (configShown) {
    static int shownPct = -1;
    bool onErase = in.down && uiConfigEraseHit(in.x, in.y);
    int pct = onErase ? (int)std::min(100UL, in.heldMs * 100 / LONG_PRESS_MS) : 0;
    if (pct / 4 != shownPct / 4) {  // redraw the progress in 4 % steps
      shownPct = pct;
      uiConfigEraseProgress(pct);
    }
    if (in.gesture == G_HOLD && uiConfigEraseHit(in.x, in.y)) wifiEraseAndRestart();
    if ((in.gesture == G_TAP && !uiConfigEraseHit(in.x, in.y)) || timeout) {
      shownPct = -1;
      closeConfig();
    }
  } else if (in.gesture == G_TAP && online) {
    openConfig();
  }
#else
  if (in.gesture == G_HOLD && !in.secondary) wifiEraseAndRestart();
  if (configShown) {
    if (in.gesture == G_TAP || timeout) closeConfig();
  } else if (in.gesture == G_TAP && !in.secondary && online) {
    openConfig();
  }
#endif
}

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
  drawDashboard(lastGood, deltas, lastError, r.notConfigured, "http://" + WiFi.localIP().toString(), configServerCode());

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
  setCpuFrequencyMhz(160);  // plenty for a dashboard that updates once a minute (default 240 MHz)
  Serial.begin(115200);
  i18nReload();  // the saved language applies from the first screen
  portBegin();
  inputBegin();
  online = wifiConnect();
  if (online) {
    WiFi.setSleep(WIFI_PS_MIN_MODEM);  // radio sleeps between beacons; the settings page is a little slower
    glucoseInit();
    configServerBegin();
    Serial.printf("settings code: %s\n", configServerCode().c_str());
  }
}

void loop() {
  handleInput();
  nightTick();
  if (online) {
    configServerHandle();
    bool changed = configServerTakeChanged();
    if (changed) lastGood = Reading();  // new source/credentials: drop the old reading
    if (changed || millis() >= nextFetch) refresh();
    dashboardTick();
  }
  delay(10);  // idle the CPU between polls; touch, buttons and the web server stay responsive
}
