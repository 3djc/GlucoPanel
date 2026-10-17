#include <Arduino.h>
#include <algorithm>
#include <WiFi.h>
#include "config.h"
#include "config_server.h"
#include "display.h"
#include "glucose.h"
#include "i18n.h"
#include "ui.h"
#include "lvgl_port.h"
#include "wifi_setup.h"

static bool online = false;

// reTerminal E1001 user buttons (active low, see wifi_setup.cpp): green = KEY0, the other one = KEY1.
static const int BTN_GREEN = 3, BTN_OTHER = 4;
static const unsigned long LONG_PRESS_MS = 3000;      // green button: hold this long to erase the WiFi
static const unsigned long CONFIG_SCREEN_MS = 60000;  // the settings screen closes by itself after this

enum Press { PRESS_NONE, PRESS_SHORT, PRESS_LONG };
struct Button {
  int pin;
  bool down = false, longFired = false;
  unsigned long since = 0;
};
static Button green{BTN_GREEN}, other{BTN_OTHER};

// Poll every loop. SHORT is reported on release, LONG once, as soon as the button has been held long enough.
static Press pollButton(Button& b) {
  bool low = digitalRead(b.pin) == LOW;
  unsigned long now = millis();
  if (low && !b.down) {
    b.down = true;
    b.longFired = false;
    b.since = now;
  } else if (low && !b.longFired && now - b.since >= LONG_PRESS_MS) {
    b.longFired = true;
    return PRESS_LONG;
  } else if (!low && b.down) {
    b.down = false;
    if (!b.longFired && now - b.since >= 40) return PRESS_SHORT;  // ignore contact bounce
  }
  return PRESS_NONE;
}

static bool configShown = false;
static unsigned long configSince = 0;

static void handleButtons() {
  Press g = pollButton(green), o = pollButton(other);
  if (g == PRESS_LONG) wifiEraseAndRestart();
  if (configShown) {
    if (g == PRESS_SHORT || o == PRESS_SHORT || millis() - configSince > CONFIG_SCREEN_MS) {
      configShown = false;
      dashboardHideConfig();
    }
  } else if (g == PRESS_SHORT && online) {
    configShown = true;
    configSince = millis();
    Serial.printf("settings code: %s\n", configServerNewCode().c_str());  // a fresh code each time the screen opens
    dashboardShowConfig("http://" + WiFi.localIP().toString(), configServerCode());
  }
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
  Serial.begin(115200);
  i18nReload();  // the saved language applies from the first screen
  portBegin();
  online = wifiConnect();
  if (online) {
    glucoseInit();
    configServerBegin();
    Serial.printf("settings code: %s\n", configServerCode().c_str());
  }
}

void loop() {
  handleButtons();
  if (online) {
    configServerHandle();
    bool changed = configServerTakeChanged();
    if (changed) lastGood = Reading();  // new source/credentials: drop the old reading
    if (changed || millis() >= nextFetch) refresh();
    dashboardTick();
  }
  delay(2);
}
