#ifdef ARDUINO  // device only; the simulator uses sim/http_sim.cpp
#include "http.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// The SDK embeds the Mozilla root certificate list (~68 KB, already part of the firmware); HTTPS
// connections are checked against it unless "Check server certificates" is switched off in the settings.
extern const uint8_t rootca_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_end[] asm("_binary_x509_crt_bundle_end");

HttpResponse httpRequest(const char* method, const String& url, const String& body,
                         const std::vector<HttpHeader>& headers) {
  HttpResponse r;
  HTTPClient http;
  WiFiClientSecure tls;
  WiFiClient plain;
  bool ok;
  bool secure = url.startsWith("https://");
  if (secure) {
    // Needs the clock to be right (certificates have validity dates): fetchGlucose() waits for NTP first.
    if (cfgGet("tls_verify") == "off") tls.setInsecure();
    else tls.setCACertBundle(rootca_start, rootca_end - rootca_start);
    ok = http.begin(tls, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) {
    r.error = "bad URL";
    return r;
  }
  http.setTimeout(15000);
  for (const HttpHeader& h : headers) {
    if (h.name == "User-Agent") http.setUserAgent(h.value);
    else http.addHeader(h.name, h.value);
  }
  int code = http.sendRequest(method, body);
  r.status = code;
  if (code > 0) r.body = http.getString();
  else {
    char why[96] = {};
    if (secure) tls.lastError(why, sizeof(why));  // e.g. "X509 - Certificate verification failed, ..."
    r.error = why[0] ? String(why) : HTTPClient::errorToString(code);
  }
  http.end();
  return r;
}
#endif
