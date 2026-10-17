#include <Arduino.h>
#ifdef ARDUINO
#include <FS.h>  // WebServer.h (via WiFiManager) needs FS declared first
#endif
#include <WiFi.h>
#include <WiFiManager.h>
// Must come after WiFiManager: Seeed_GFX defines FS_NO_GLOBALS, which breaks WebServer.h
#include "wifi_setup.h"
#include "ui.h"

// reTerminal E1001 user buttons: active low, external pull-ups.
static const int BTN_ERASE = 3;  // KEY0 (green)
static const int BTN_RETRY = 4;  // KEY1
static const unsigned long JOIN_TIMEOUT_MS = 20000;
static const unsigned long PROMPT_TIMEOUT_MS = 60000;  // then retry automatically

static bool tryJoinSaved() {
  WiFi.begin();  // stored credentials
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < JOIN_TIMEOUT_MS) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

// Returns true if the user chose to erase the stored credentials.
static bool askEraseCredentials(const String& ssid) {
  uiShowErasePrompt(ssid);
  unsigned long t0 = millis();
  while (millis() - t0 < PROMPT_TIMEOUT_MS) {
    if (digitalRead(BTN_ERASE) == LOW) return true;
    if (digitalRead(BTN_RETRY) == LOW) return false;
    delay(50);
  }
  return false;
}

static void showPortalScreen(WiFiManager*) { uiShowPortal(); }

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
  pinMode(BTN_ERASE, INPUT);
  pinMode(BTN_RETRY, INPUT);
  WiFiManager wm;
  bool ok = false;
  while (wm.getWiFiIsSaved()) {
    if (tryJoinSaved()) { ok = true; break; }
    if (askEraseCredentials(wm.getWiFiSSID())) {
      wm.resetSettings();  // falls through to the hotspot/portal below
      break;
    }
  }
  if (!ok) {
    wm.setAPCallback(showPortalScreen);
    ok = wm.autoConnect(AP_NAME);  // open hotspot, captive portal
  }
  if (ok) {
    String ip = WiFi.localIP().toString();
    uiShowConnected(WiFi.SSID(), ip, "http://" + ip);
  } else {
    uiShowWifiFailed();
  }
  return ok;
}

void wifiEraseAndRestart() {
  uiShowWifiErased();
  delay(500);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
  while (true) delay(1000);  // ESP.restart() does not return on the device
}
