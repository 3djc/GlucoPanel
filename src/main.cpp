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
#include "ota.h"
#include "ui.h"
#include "version.h"
#include "lvgl_port.h"
#include "power.h"
#include "wifi_setup.h"
#if POWER_SAVE
#include <esp_sleep.h>
#endif

static bool online = false;

#if POWER_SAVE
static bool radioOn = true;  // the WiFi radio is off while the panel sleeps (powerTick)

// Brings the WiFi back after a sleep. False when the network cannot be joined (the fetch then says so).
static bool radioUp() {
  if (radioOn) return true;
  unsigned long t0 = millis();
  radioOn = wifiResume(15000);
  Serial.printf("wifi %s after %lu ms\n", radioOn ? "back" : "did not come back", millis() - t0);
  if (radioOn) {
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    glucoseInit();  // ask NTP again: the clock drifts a little while asleep
    configServerResume();
  }
  return radioOn;
}
#else
static const bool radioOn = true;
static bool radioUp() { return true; }
#endif

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
    if (radioUp()) openConfig();  // the settings page needs the WiFi, which is off while asleep
  }
#endif
}

static unsigned long nextFetch = 0;
static Reading lastGood;
static Deltas deltas;
static String lastError;

static void refresh() {
  // Right after boot the clock needs a few seconds (NTP). Retry quickly instead of drawing an error and waiting a
  // minute; if it still is not set after 30 s, fall through so the error shows.
  static unsigned long clockWaitSince = 0;
  if (time(nullptr) < 1700000000) {
    if (!clockWaitSince) clockWaitSince = millis();
    if (millis() - clockWaitSince < 30000UL) {
      nextFetch = millis() + 500;
      return;
    }
  }

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

#if POWER_SAVE
static const unsigned long AWAKE_AFTER_BOOT_MS = 120000;  // the first fetches, the first update check, the firmware confirmation
static const long MIN_SLEEP_MS = 30000;                   // a shorter wait is not worth switching the radio off for

// Between two events the chip light-sleeps with the radio off. It wakes at the next clock minute (the clock, the
// age and the freshness bar move then), at the next fetch, or when a button is pressed.
static void powerTick() {
  if (!online || configShown || millis() < AWAKE_AFTER_BOOT_MS) return;
  if (otaWantsNetwork() || !inputIdle()) return;
  long toFetch = (long)(nextFetch - millis());
  if (toFetch < MIN_SLEEP_MS) return;  // retrying soon (Dexcom waits 10 s for a new reading): stay connected
  time_t now = time(nullptr);
  long toMinute = now > 1700000000 ? (60 - (long)(now % 60)) * 1000L + 300 : 60000L;
  long sleepMs = std::min(toFetch, toMinute);
  if (sleepMs < 1000) sleepMs += 60000;  // just before a minute change: skip to the next one

  if (radioOn) configServerSuspend();  // before the WiFi goes off (see config_server.cpp)
  wifiSuspend();
  radioOn = false;
  Serial.printf("sleep %ld ms\n", sleepMs);
  Serial.flush();
  inputArmWake();
  esp_sleep_enable_timer_wakeup((uint64_t)sleepMs * 1000ULL);
  esp_light_sleep_start();
  Serial.printf("awake (%s)\n", esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO ? "button" : "timer");
}
#endif

void setup() {
  setCpuFrequencyMhz(240);  // full speed: faster start and redraws; the power is saved by the light sleep between minutes (powerTick)
  Serial.begin(115200);
  Serial.printf("GlucoPanel %s (%s)\n", FW_VERSION, BOARD_ID);
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
    if (radioOn) configServerHandle();
    bool changed = radioOn && configServerTakeChanged();
    if (changed) lastGood = Reading();  // new source/credentials: drop the old reading
    bool due = changed || millis() >= nextFetch;
#if POWER_SAVE
    if (due || otaWantsNetwork()) radioUp();
#endif
    if (due) refresh();
    dashboardTick();
    if (radioOn) otaTick(!configShown && lastError.length() == 0);
#if POWER_SAVE
    powerTick();
#endif
  }
  delay(10);  // idle the CPU between polls; touch, buttons and the web server stay responsive
}
