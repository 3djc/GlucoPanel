#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "glucose.h"
#include "http.h"

// ---------- helpers ----------

static FetchResult fail(const String& msg) {
  FetchResult r;
  r.error = msg;
  return r;
}

static FetchResult succeed(int mgdl, Trend t, time_t when) {
  FetchResult r;
  r.ok = true;
  r.reading.valid = true;
  r.reading.mgdl = mgdl;
  r.reading.trend = t;
  r.reading.when = when;
  return r;
}

static String jsonStr(const String& s) {
  String o = "\"";
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((unsigned char)c >= 32) o += c;
  }
  return o + "\"";
}

static String urlEncode(const String& s) {
  String o;
  char buf[4];
  for (int i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { snprintf(buf, sizeof(buf), "%%%02X", c); o += buf; }
  }
  return o;
}

static time_t civilToEpoch(int y, int m, int d, int H, int M, int S) {
  y -= m <= 2;  // days-from-civil (Howard Hinnant)
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468;
  return (time_t)(days * 86400L + H * 3600L + M * 60L + S);
}

static String sha256Hex(const String& in) {
  static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::string msg(in.c_str());
  uint64_t bits = (uint64_t)msg.size() * 8;
  msg += (char)0x80;
  while (msg.size() % 64 != 56) msg += (char)0;
  for (int i = 7; i >= 0; i--) msg += (char)(bits >> (i * 8));
  auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
      w[i] = ((uint32_t)(uint8_t)msg[off + i * 4] << 24) | ((uint32_t)(uint8_t)msg[off + i * 4 + 1] << 16) |
             ((uint32_t)(uint8_t)msg[off + i * 4 + 2] << 8) | (uint32_t)(uint8_t)msg[off + i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + S1 + ch + K[i] + w[i];
      uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + mj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  String out;
  char buf[9];
  for (int i = 0; i < 8; i++) { snprintf(buf, sizeof(buf), "%08x", (unsigned)h[i]); out += buf; }
  return out;
}

// Nightscout "direction" and Dexcom string trends share the same names.
static Trend trendFromName(const char* n) {
  if (!n) return TREND_UNKNOWN;
  if (!strcmp(n, "DoubleUp")) return TREND_DOUBLE_UP;
  if (!strcmp(n, "SingleUp")) return TREND_SINGLE_UP;
  if (!strcmp(n, "FortyFiveUp")) return TREND_45_UP;
  if (!strcmp(n, "Flat")) return TREND_FLAT;
  if (!strcmp(n, "FortyFiveDown")) return TREND_45_DOWN;
  if (!strcmp(n, "SingleDown")) return TREND_SINGLE_DOWN;
  if (!strcmp(n, "DoubleDown")) return TREND_DOUBLE_DOWN;
  return TREND_UNKNOWN;
}

static Trend trendFromDexcomNumber(int n) {  // 1 = DoubleUp ... 7 = DoubleDown
  static const Trend t[] = {TREND_UNKNOWN, TREND_DOUBLE_UP, TREND_SINGLE_UP, TREND_45_UP, TREND_FLAT,
                            TREND_45_DOWN, TREND_SINGLE_DOWN, TREND_DOUBLE_DOWN};
  return n >= 1 && n <= 7 ? t[n] : TREND_UNKNOWN;
}

static Trend trendFromLibreArrow(int n) {  // 1 = SingleDown ... 5 = SingleUp
  static const Trend t[] = {TREND_UNKNOWN, TREND_SINGLE_DOWN, TREND_45_DOWN, TREND_FLAT, TREND_45_UP, TREND_SINGLE_UP};
  return n >= 1 && n <= 5 ? t[n] : TREND_UNKNOWN;
}

// ---------- Nightscout ----------

static FetchResult fetchNightscout() {
  String base = cfgGet("ns_url");
  if (!base.length()) { FetchResult r; r.notConfigured = true; r.error = "Nightscout URL not set"; return r; }
  String url = base + "/api/v1/entries/sgv.json?count=1";
  String tok = cfgGet("ns_token");
  if (tok.length()) url += "&token=" + urlEncode(tok);

  HttpResponse r = httpRequest("GET", url, "", {{"Accept", "application/json"}});
  if (r.status <= 0) return fail("Nightscout: " + r.error);
  if (r.status == 401 || r.status == 403) return fail("Nightscout: access denied (check token)");
  if (r.status != 200) return fail("Nightscout: HTTP " + String(r.status));

  JsonDocument doc;
  if (deserializeJson(doc, r.body.c_str())) return fail("Nightscout: unreadable response");
  JsonArray a = doc.as<JsonArray>();
  if (a.isNull() || a.size() == 0) return fail("Nightscout: no readings");
  JsonObject o = a[0];
  int sgv = (int)lround(o["sgv"] | 0.0);
  if (sgv <= 0) return fail("Nightscout: reading has no value");
  double ms = o["date"] | 0.0;
  return succeed(sgv, trendFromName(o["direction"] | ""), (time_t)(ms / 1000.0));
}

// ---------- Dexcom Share ----------

static String dexSession;

static String dexBase() {
  String reg = cfgGet("dex_region");
  String host = reg == "ous" ? "shareous1.dexcom.com" : reg == "jp" ? "share.dexcom.jp" : "share2.dexcom.com";
  return "https://" + host + "/ShareWebServices/Services";
}

static String dexAppId() {
  return cfgGet("dex_region") == "jp" ? "d8665ade-9673-4e27-9ff6-92db4ce13d13"
                                      : "d89443d2-327c-4a6f-89e5-496bbb0317db";
}

// Dexcom errors are JSON objects like {"Code":"AccountPasswordInvalid","Message":"..."}.
static String dexErrorCode(const String& body) {
  JsonDocument d;
  if (deserializeJson(d, body.c_str()) || !d.is<JsonObject>()) return "";
  return String(d["Code"] | "");
}

// Both login steps answer with a bare JSON string (a GUID).
static bool dexPost(const String& path, const String& body, String& out, String& error) {
  HttpResponse r = httpRequest("POST", dexBase() + path, body,
                               {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
  if (r.status <= 0) { error = "Dexcom: " + r.error; return false; }
  JsonDocument d;
  if (r.status == 200 && !deserializeJson(d, r.body.c_str()) && d.is<const char*>()) {
    out = String(d.as<const char*>());
    if (out != "00000000-0000-0000-0000-000000000000") return true;
  }
  String code = dexErrorCode(r.body);
  error = code.length() ? "Dexcom: " + code : "Dexcom: login failed (HTTP " + String(r.status) + ")";
  return false;
}

static bool dexLogin(String& error) {
  String user = cfgGet("dex_user"), pass = cfgGet("dex_pass"), app = dexAppId(), account;
  if (!dexPost("/General/AuthenticatePublisherAccount",
               "{\"accountName\":" + jsonStr(user) + ",\"password\":" + jsonStr(pass) +
                   ",\"applicationId\":\"" + app + "\"}", account, error))
    return false;
  return dexPost("/General/LoginPublisherAccountById",
                 "{\"accountId\":" + jsonStr(account) + ",\"password\":" + jsonStr(pass) +
                     ",\"applicationId\":\"" + app + "\"}", dexSession, error);
}

static FetchResult fetchDexcom() {
  if (!cfgGet("dex_user").length() || !cfgGet("dex_pass").length()) {
    FetchResult r; r.notConfigured = true; r.error = "Dexcom username/password not set"; return r;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    String error;
    if (!dexSession.length() && !dexLogin(error)) return fail(error);
    HttpResponse r = httpRequest("POST",
        dexBase() + "/Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + dexSession + "&minutes=1440&maxCount=1",
        "", {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (r.status <= 0) return fail("Dexcom: " + r.error);
    String code = dexErrorCode(r.body);
    if (code == "SessionNotValid" || code == "SessionIdNotFound") { dexSession = ""; continue; }
    if (r.status != 200) return fail(code.length() ? "Dexcom: " + code : "Dexcom: HTTP " + String(r.status));

    JsonDocument doc;
    if (deserializeJson(doc, r.body.c_str())) return fail("Dexcom: unreadable response");
    JsonArray a = doc.as<JsonArray>();
    if (a.isNull() || a.size() == 0) return fail("Dexcom: no readings in the last 24 h");
    JsonObject o = a[0];
    int val = o["Value"] | 0;
    if (val <= 0) return fail("Dexcom: reading has no value");
    Trend t = o["Trend"].is<int>() ? trendFromDexcomNumber(o["Trend"].as<int>()) : trendFromName(o["Trend"] | "");
    const char* ts = o["ST"] | (o["DT"] | (o["WT"] | ""));  // "Date(1700000000000)" or "/Date(...-0500)/"
    const char* p = strchr(ts, '(');
    time_t when = p ? (time_t)(strtoll(p + 1, nullptr, 10) / 1000) : 0;
    return succeed(val, t, when);
  }
  return fail("Dexcom: session expired");
}

// ---------- LibreLinkUp ----------

static String lluToken, lluAccountHash, lluRegion;

static String lluBase(const String& region) { return "https://api-" + region + ".libreview.io"; }

static std::vector<HttpHeader> lluHeaders(bool auth) {
  std::vector<HttpHeader> h = {
    {"User-Agent", "Mozilla/5.0 (iPhone; CPU OS 17_4.1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4.1 Mobile/15E148 Safari/604.1"},
    {"Content-Type", "application/json;charset=UTF-8"},
    {"Accept", "application/json"},
    {"product", "llu.android"},
    {"version", "4.16.0"},
  };
  if (auth) {
    h.push_back({"Authorization", "Bearer " + lluToken});
    h.push_back({"account-id", lluAccountHash});
  }
  return h;
}

static bool lluLogin(String& error) {
  if (!lluRegion.length()) lluRegion = cfgGet("libre_region");
  for (int attempt = 0; attempt < 2; attempt++) {  // second try follows a region redirect
    HttpResponse r = httpRequest("POST", lluBase(lluRegion) + "/llu/auth/login",
        "{\"email\":" + jsonStr(cfgGet("libre_email")) + ",\"password\":" + jsonStr(cfgGet("libre_pass")) + "}",
        lluHeaders(false));
    if (r.status <= 0) { error = "LibreLinkUp: " + r.error; return false; }
    JsonDocument d;
    if (deserializeJson(d, r.body.c_str())) { error = "LibreLinkUp: unreadable login response (HTTP " + String(r.status) + ")"; return false; }
    int status = d["status"] | -1;
    if (status == 2) { error = "LibreLinkUp: wrong email or password"; return false; }
    if (status == 4) { error = "LibreLinkUp: accept the new terms in the LibreLinkUp app"; return false; }
    if (status != 0) { error = "LibreLinkUp: login status " + String(status); return false; }
    if (d["data"]["redirect"] | false) {
      String region = String(d["data"]["region"] | "");
      if (!region.length() || region == lluRegion) { error = "LibreLinkUp: region redirect failed"; return false; }
      lluRegion = region;  // e.g. "de"
      continue;
    }
    const char* token = d["data"]["authTicket"]["token"] | "";
    const char* uid = d["data"]["user"]["id"] | "";
    if (!*token || !*uid) { error = "LibreLinkUp: login needs an extra step (open the LibreLinkUp app)"; return false; }
    lluToken = token;
    lluAccountHash = sha256Hex(uid);
    return true;
  }
  error = "LibreLinkUp: region redirect loop";
  return false;
}

static FetchResult fetchLibre() {
  if (!cfgGet("libre_email").length() || !cfgGet("libre_pass").length()) {
    FetchResult r; r.notConfigured = true; r.error = "LibreLinkUp email/password not set"; return r;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    String error;
    if (!lluToken.length() && !lluLogin(error)) return fail(error);
    HttpResponse r = httpRequest("GET", lluBase(lluRegion) + "/llu/connections", "", lluHeaders(true));
    if (r.status <= 0) return fail("LibreLinkUp: " + r.error);
    if (r.status == 401) { lluToken = ""; continue; }  // token expired: log in again
    if (r.status != 200) return fail("LibreLinkUp: HTTP " + String(r.status));

    JsonDocument doc;
    if (deserializeJson(doc, r.body.c_str())) return fail("LibreLinkUp: unreadable response");
    JsonArray conns = doc["data"].as<JsonArray>();
    if (conns.isNull() || conns.size() == 0) return fail("LibreLinkUp: no connections (share your data in LibreLink)");
    JsonObject gm = conns[0]["glucoseMeasurement"];
    int mgdl = (int)lround(gm["ValueInMgPerDl"] | 0.0);
    if (mgdl <= 0) return fail("LibreLinkUp: no glucose measurement");
    int mo, d, y, H, M, S;
    char ap[3] = {};
    time_t when = 0;  // FactoryTimestamp is UTC, "M/D/YYYY h:mm:ss AM"
    if (sscanf(gm["FactoryTimestamp"] | "", "%d/%d/%d %d:%d:%d %2s", &mo, &d, &y, &H, &M, &S, ap) == 7) {
      if (!strcmp(ap, "PM") && H < 12) H += 12;
      if (!strcmp(ap, "AM") && H == 12) H = 0;
      when = civilToEpoch(y, mo, d, H, M, S);
    }
    return succeed(mgdl, trendFromLibreArrow(gm["TrendArrow"] | 0), when);
  }
  return fail("LibreLinkUp: session expired");
}

// ---------- public ----------

void glucoseInit() {
#ifdef ARDUINO
  configTime(0, 0, "pool.ntp.org", "time.google.com");
#endif
}

const char* sourceName() {
  String s = cfgGet("source");
  return s == "dexcom" ? "Dexcom Share" : s == "libre" ? "LibreLinkUp" : "Nightscout";
}

FetchResult fetchGlucose() {
  // Drop cached sessions when the credentials change.
  static String cacheKey;
  String key = cfgGet("source") + "|" + cfgGet("libre_email") + cfgGet("libre_pass") + cfgGet("libre_region") +
               "|" + cfgGet("dex_user") + cfgGet("dex_pass") + cfgGet("dex_region");
  if (key != cacheKey) {
    cacheKey = key;
    dexSession = lluToken = lluAccountHash = lluRegion = "";
  }

  if (WiFi.status() != WL_CONNECTED) return fail("WiFi disconnected");
  if (time(nullptr) < 1700000000) return fail("Waiting for the clock (NTP)");

  String src = cfgGet("source");
  if (src == "dexcom") return fetchDexcom();
  if (src == "libre") return fetchLibre();
  return fetchNightscout();
}
