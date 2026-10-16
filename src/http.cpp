#ifdef ARDUINO  // device only; the simulator uses sim/http_sim.cpp
#include "http.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

HttpResponse httpRequest(const char* method, const String& url, const String& body,
                         const std::vector<HttpHeader>& headers) {
  HttpResponse r;
  HTTPClient http;
  WiFiClientSecure tls;
  WiFiClient plain;
  bool ok;
  if (url.startsWith("https://")) {
    tls.setInsecure();  // TLS is encrypted but the server certificate is NOT verified (no CA bundle embedded)
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
  else r.error = HTTPClient::errorToString(code);
  http.end();
  return r;
}
#endif
