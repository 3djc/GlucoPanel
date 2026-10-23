#ifdef ARDUINO  // device only
#include "ota.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include "config.h"
#include "display.h"
#include "version.h"

#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL "https://github.com/3djc/GlucoPanel/releases/latest/download/manifest.json"
#endif
#define OTA_URL_PREFIX "https://github.com/"  // the manifest may only point at the project's releases

static const unsigned long CONFIRM_AFTER_MS = 60000;  // a new firmware that has run this long is kept

// The root certificate list embedded by the SDK (see http.cpp). Updates are always verified, whatever the
// "Check server certificates" setting says.
extern const uint8_t rootca_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_end[] asm("_binary_x509_crt_bundle_end");

// By default the Arduino core confirms the running firmware as soon as it boots. Say that we do it
// ourselves (otaTick), so an update that cannot run properly is rolled back at the next restart.
extern "C" bool verifyRollbackLater() { return true; }  // C linkage: it replaces a weak C function of the core

// GitHub answers with a redirect to its download servers.
static bool beginHttps(HTTPClient& http, WiFiClientSecure& tls, const String& url) {
  tls.setCACertBundle(rootca_start, rootca_end - rootca_start);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setUserAgent("GlucoPanel/" FW_VERSION);
  return http.begin(tls, url);
}

static String httpFailure(int code, WiFiClientSecure& tls) {
  if (code > 0) return "HTTP " + String(code);
  char why[96] = {};
  tls.lastError(why, sizeof(why));
  return why[0] ? String(why) : HTTPClient::errorToString(code);
}

static bool isHex64(const String& s) {
  if (s.length() != 64) return false;
  for (size_t i = 0; i < s.length(); i++)
    if (!isxdigit((unsigned char)s[i])) return false;
  return true;
}

static OtaInfo lastCheck;

const OtaInfo& otaLast() { return lastCheck; }

static OtaInfo fetchManifest() {
  OtaInfo info;
  if (WiFi.status() != WL_CONNECTED) { info.error = "WiFi disconnected"; return info; }
  if (time(nullptr) < 1700000000) { info.error = "Waiting for the clock (NTP)"; return info; }

  HTTPClient http;
  WiFiClientSecure tls;
  if (!beginHttps(http, tls, OTA_MANIFEST_URL)) { info.error = "bad URL"; return info; }
  int code = http.GET();
  if (code != 200) {
    info.error = httpFailure(code, tls);
    http.end();
    return info;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) { info.error = "bad manifest"; return info; }
  info.version = doc["version"] | "";
  JsonObject entry = doc[BOARD_ID];
  info.url = entry["url"] | "";
  info.sha256 = entry["sha256"] | "";
  info.size = entry["size"] | 0;
  if (info.version.isEmpty() || entry.isNull()) { info.error = "no firmware for " BOARD_ID; return info; }
  if (!info.url.startsWith(OTA_URL_PREFIX) || !isHex64(info.sha256) || info.size == 0) {
    info.error = "bad manifest";
    return info;
  }
  info.ok = true;
  info.newer = versionNewer(info.version.c_str(), FW_VERSION);
  return info;
}

OtaInfo otaCheck() {
  lastCheck = fetchManifest();
  lastCheck.checked = time(nullptr) > 1700000000 ? time(nullptr) : 1;  // 1: checked, the clock was not set
  return lastCheck;
}

static bool fail(String& error, const String& why) {
  error = why;
  Update.abort();
  dashboardHideConfig();  // the update screen is up: bring the dashboard back
  return false;
}

bool otaInstall(const OtaInfo& info, String& error) {
  HTTPClient http;
  WiFiClientSecure tls;
  if (!beginHttps(http, tls, info.url)) { error = "bad URL"; return false; }
  int code = http.GET();
  if (code != 200) {
    error = httpFailure(code, tls);
    http.end();
    return false;
  }
  int len = http.getSize();
  if (len > 0 && (size_t)len != info.size) {
    error = "size mismatch";
    http.end();
    return false;
  }
  if (!Update.begin(info.size)) {
    error = Update.errorString();
    http.end();
    return false;
  }

  dashboardShowUpdate(0);
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1460];
  size_t done = 0;
  int shownPct = 0;
  unsigned long lastData = millis();
  while (done < info.size) {
    size_t avail = stream->available();
    if (!avail) {
      if (!http.connected() || millis() - lastData > 15000) break;  // dropped or stalled
      delay(2);
      continue;
    }
    size_t n = stream->readBytes(buf, std::min(avail, sizeof(buf)));
    n = std::min(n, info.size - done);  // never past the announced size
    if (Update.write(buf, n) != n) {
      mbedtls_sha256_free(&sha);
      http.end();
      return fail(error, Update.errorString());
    }
    mbedtls_sha256_update(&sha, buf, n);
    done += n;
    lastData = millis();
    int pct = (int)(done * 100 / info.size);
    if (pct >= shownPct + 10) {  // the panel is slow: a redraw every 10 %
      shownPct = pct - pct % 10;
      dashboardShowUpdate(shownPct);
    }
  }
  http.end();
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);

  if (done != info.size) return fail(error, "download incomplete");
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  if (!info.sha256.equalsIgnoreCase(hex)) return fail(error, "hash mismatch");
  if (!Update.end(true)) return fail(error, Update.errorString());

  dashboardShowUpdate(100);
  Serial.printf("update to %s installed, restarting\n", info.version.c_str());
  delay(500);
  ESP.restart();
  return true;  // not reached
}

// ---- automatic updates ----

static const unsigned long CHECK_EVERY_MS = 24UL * 3600UL * 1000UL;
static const unsigned long RETRY_MS = 15UL * 60UL * 1000UL;  // after a failed check; also the least time between checks
static const unsigned long FIRST_CHECK_MS = 90000;           // let the first glucose fetches finish first
static const int MAX_ATTEMPTS = 3;                           // automatic installs of one version, one per night

static bool reached(unsigned long now, unsigned long t) { return (long)(now - t) >= 0; }  // millis() wraps

// A version whose automatic install failed MAX_ATTEMPTS times is left alone until a newer release appears
// (the "Install now" button on the settings page still works).
static bool givenUp(const String& version) { return cfgGet("ota_fv") == version && cfgGet("ota_fn").toInt() >= MAX_ATTEMPTS; }

static void noteFailure(const String& version) {
  int n = cfgGet("ota_fv") == version ? cfgGet("ota_fn").toInt() : 0;
  cfgSet("ota_fv", version);
  cfgSet("ota_fn", String(n + 1));
}

// Looks for a new release once a day and, if there is one, installs it during the hour chosen in the
// settings. `canInstall` is false while the panel is busy or in trouble (settings screen open, fetch error).
static void autoUpdate(bool canInstall) {
  if (cfgGet("auto_update") != "on") return;
  time_t now = time(nullptr);
  if (now < 1700000000) return;  // no clock yet
  struct tm t;
  localtime_r(&now, &t);
  bool inWindow = t.tm_hour == cfgGet("update_hour").toInt();
  int today = t.tm_year * 366 + t.tm_yday;

  static unsigned long nextCheck = FIRST_CHECK_MS, holdUntil = 0;
  static int checkedDay = -1, triedDay = -1;
  unsigned long ms = millis();
  static bool primed = false;  // the timers start when automatic updates are first seen on (millis() compares only work within ~24 days)
  if (!primed) {
    primed = true;
    nextCheck = ms < FIRST_CHECK_MS ? FIRST_CHECK_MS : ms;
    holdUntil = ms;
  }
  // Daily, and once more when the window opens (a release may have appeared since the last check).
  bool due = reached(ms, nextCheck) || (inWindow && checkedDay != today);
  if (due && reached(ms, holdUntil)) {
    OtaInfo o = otaCheck();
    holdUntil = ms + RETRY_MS;
    nextCheck = ms + (o.ok ? CHECK_EVERY_MS : RETRY_MS);
    if (o.ok && inWindow) checkedDay = today;
    Serial.printf("update check: %s\n", o.ok ? (o.newer ? ("new version " + o.version).c_str() : "up to date") : o.error.c_str());
  }

  const OtaInfo& o = otaLast();
  if (!inWindow || !canInstall || !o.ok || !o.newer || checkedDay != today || triedDay == today || givenUp(o.version)) return;
  triedDay = today;  // one attempt per night
  Serial.printf("installing %s\n", o.version.c_str());
  String error;
  if (!otaInstall(o, error)) {  // only returns on failure
    noteFailure(o.version);
    Serial.printf("update failed: %s\n", error.c_str());
  }
}

void otaTick(bool canInstall) {
  static unsigned long lastRun = 0;
  if (millis() - lastRun < 1000) return;  // the settings are read from flash: once a second is plenty
  lastRun = millis();

  static bool confirmed = false;
  if (!confirmed && millis() >= CONFIRM_AFTER_MS) {
    confirmed = true;
    // A no-op unless this run is a freshly installed update still waiting to be confirmed.
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) Serial.println("firmware confirmed");
  }
  autoUpdate(canInstall);
}
#endif
