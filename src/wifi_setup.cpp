#include <Arduino.h>
#ifdef ARDUINO
#include <FS.h>  // WebServer.h (via WiFiManager) needs FS declared first
#endif
#include <WiFi.h>
#include <WiFiManager.h>
#include <qrcode.h>
// Must come after WiFiManager: Seeed_GFX defines FS_NO_GLOBALS, which breaks WebServer.h
#include "wifi_setup.h"

static EPaper* gEpaper;

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
  EPaper& e = *gEpaper;
  e.fillScreen(TFT_WHITE);
  e.setTextColor(TFT_BLACK);
  e.drawString("Cannot join WiFi", 20, 20, 4);
  e.drawString("Saved network: " + ssid, 20, 80, 2);
  e.drawString("Erase it and set up a new WiFi?", 20, 130, 2);
  e.drawString("KEY0 (green): ERASE and set up new WiFi", 20, 200, 2);
  e.drawString("KEY1: retry saved WiFi", 20, 240, 2);
  e.drawString("(retries automatically in 60 s)", 20, 300, 2);
  e.update();
  unsigned long t0 = millis();
  while (millis() - t0 < PROMPT_TIMEOUT_MS) {
    if (digitalRead(BTN_ERASE) == LOW) return true;
    if (digitalRead(BTN_RETRY) == LOW) return false;
    delay(50);
  }
  return false;
}

static void drawQR(int x, int y, int scale, const char* text) {
  QRCode qr;
  uint8_t buf[qrcode_getBufferSize(4)];
  qrcode_initText(&qr, buf, 4, ECC_MEDIUM, text);
  int quiet = 2 * scale;
  gEpaper->fillRect(x - quiet, y - quiet, qr.size * scale + 2 * quiet, qr.size * scale + 2 * quiet, TFT_WHITE);
  for (int j = 0; j < qr.size; j++)
    for (int i = 0; i < qr.size; i++)
      if (qrcode_getModule(&qr, i, j))
        gEpaper->fillRect(x + i * scale, y + j * scale, scale, scale, TFT_BLACK);
}

static void showPortalScreen(WiFiManager*) {
  EPaper& e = *gEpaper;
  e.fillScreen(TFT_WHITE);
  e.setTextColor(TFT_BLACK);
  e.drawString("WiFi setup", 20, 20, 4);
  e.drawString("1. Join WiFi " AP_NAME, 20, 70, 2);
  drawQR(80, 130, 7, "WIFI:T:nopass;S:" AP_NAME ";;");
  e.drawString("2. Open setup page", 420, 70, 2);
  drawQR(480, 130, 7, "http://192.168.4.1");
  e.drawString("(or browse to 192.168.4.1)", 420, 400, 2);
  e.update();
}

bool wifiConnect(EPaper& epaper) {
  gEpaper = &epaper;
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
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK);
  if (ok) {
    epaper.drawString("WiFi connected", 20, 20, 4);
    epaper.drawString("SSID: " + WiFi.SSID(), 20, 70, 2);
    epaper.drawString("IP: " + WiFi.localIP().toString(), 20, 100, 2);
    String url = "http://" + WiFi.localIP().toString();
    epaper.drawString("Configure the panel at:", 20, 170, 2);
    epaper.drawString(url, 20, 200, 2);
    epaper.drawString("or http://glucopanel.local", 20, 230, 2);
    drawQR(520, 80, 7, url.c_str());
  } else {
    epaper.drawString("WiFi setup failed", 20, 20, 4);
  }
  epaper.update();
  return ok;
}
