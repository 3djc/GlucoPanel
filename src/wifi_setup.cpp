#include <Arduino.h>
#ifdef ARDUINO
#include <FS.h>  // WebServer.h (via WiFiManager) needs FS declared first
#endif
#include <WiFi.h>
#include <WiFiManager.h>
// Must come after WiFiManager: Seeed_GFX defines FS_NO_GLOBALS, which breaks WebServer.h
#include "wifi_setup.h"
#include "config.h"
#include "config_server.h"
#include "input.h"
#include "secrets.h"
#include "ui.h"

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
    Input in = inputPoll();
#ifdef UI_PROFILE_LCD
    if (in.gesture == G_TAP) {  // the two on-screen buttons
      int hit = uiErasePromptHit(in.x, in.y);
      if (hit == 1) return true;
      if (hit == 2) return false;
    }
#else
    if (in.gesture == G_TAP && !in.secondary) return true;  // KEY0 (green): erase
    if (in.gesture == G_TAP && in.secondary) return false;  // KEY1: retry
#endif
    delay(20);
  }
  return false;
}

// The setup hotspot is protected: a random WPA2 password, made once and kept in flash, shown on the screen
// as text and inside the "join" QR code. (An open hotspot would let anyone nearby read the home WiFi
// password you type into the setup page.)
static String hotspotPassword() {
  String p = cfgGet("ap_pass");
  if (p.length() < 8) {
    p = randomToken(10, "abcdefghjkmnpqrstuvwxyz23456789");  // no look-alike characters
    cfgSet("ap_pass", p);
  }
  return p;
}

static void showPortalScreen(WiFiManager*) { uiShowPortal(hotspotPassword()); }

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
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
    ok = wm.autoConnect(AP_NAME, hotspotPassword().c_str());  // WPA2 hotspot, captive portal
  }
  if (ok) {
    String ip = WiFi.localIP().toString();
    uiShowConnected(WiFi.SSID(), ip, "http://" + ip, configServerCode());
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
