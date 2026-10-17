#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "glucose.h"
#include "http.h"
#include "i18n.h"

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

// printf-style translated phrase with one number
static String tf(const char* en, int n) {
  char b[160];
  snprintf(b, sizeof(b), T(en), n);
  return b;
}

// ---------- reading history (for the deltas and the graph) ----------

// Libre only reports the latest value each minute, so ~1 point/min can pile up over the graph window.
static const int HIST_MAX = STATS_HOURS * 60 + 60;
static Reading hist[HIST_MAX];  // ascending by time
static int histN = 0;

static void historyClear() { histN = 0; }

static void historyAdd(int mgdl, time_t when, Trend trend) {
  if (mgdl <= 0 || when <= 0) return;
  for (int i = 0; i < histN; i++)
    if (hist[i].when == when) return;  // already known
  if (histN == HIST_MAX) {             // full: drop the oldest
    for (int i = 1; i < histN; i++) hist[i - 1] = hist[i];
    histN--;
  }
  int i = histN++;
  while (i > 0 && hist[i - 1].when > when) { hist[i] = hist[i - 1]; i--; }
  hist[i].valid = true;
  hist[i].mgdl = mgdl;
  hist[i].trend = trend;
  hist[i].when = when;
}

int glucoseHistory(Reading* out, int max) {
  int n = histN < max ? histN : max;
  for (int i = 0; i < n; i++) out[i] = hist[histN - n + i];  // the newest n, ascending
  return n;
}

Stats glucoseStats(int low, int high) {
  Stats st;
  if (histN < 2) return st;
  const time_t end = hist[histN - 1].when, from = end - (time_t)STATS_HOURS * 3600;
  int first = 0;
  while (first < histN && hist[first].when < from) first++;
  if (end - hist[first].when < 30 * 60) return st;
  double wsum = 0, mean = 0, m2 = 0, below = 0, above = 0;
  st.minMgdl = 1000;
  for (int i = first; i < histN; i++) {
    int v = hist[i].mgdl;
    time_t dt = i + 1 < histN ? hist[i + 1].when - hist[i].when : 300;  // each reading holds until the next
    if (dt > 900) dt = 900;                                              // but never across a long gap
    if (dt <= 0) continue;
    double w = (double)dt, nw = wsum + w, delta = v - mean;
    mean += delta * w / nw;  // weighted running mean / variance
    m2 += w * delta * (v - mean);
    wsum = nw;
    if (v < low) below += w;
    if (v > high) above += w;
    st.minMgdl = v < st.minMgdl ? v : st.minMgdl;
    st.maxMgdl = v > st.maxMgdl ? v : st.maxMgdl;
  }
  if (wsum <= 0) return st;
  st.ok = true;
  st.hours = (int)((end - hist[first].when + 1800) / 3600);
  st.avg = (float)mean;
  st.sd = (float)sqrt(m2 / wsum);
  st.belowPct = (float)(100.0 * below / wsum);
  st.abovePct = (float)(100.0 * above / wsum);
  st.inRangePct = 100.0f - st.belowPct - st.abovePct;
  return st;
}

// How far back the next fetch has to reach: everything on the first one (or while the clock is not set),
// afterwards only what is newer than the newest reading we have, so a fetch stays small.
static int fetchWindowMinutes() {
  time_t now = time(nullptr);
  if (histN == 0 || now < 1600000000) return STATS_HOURS * 60;
  int mins = (int)((now - hist[histN - 1].when) / 60) + 10;
  return mins < 30 ? 30 : mins > STATS_HOURS * 60 ? STATS_HOURS * 60 : mins;
}

static time_t historySpan() { return histN ? hist[histN - 1].when - hist[0].when : 0; }

Deltas glucoseDeltas(const Reading& latest) {
  Deltas d;
  if (!latest.valid) return d;
  const time_t TOLERANCE = 450;  // 7.5 min: graph points can be 15 min apart
  time_t target = latest.when - 15 * 60;
  const Reading* prev = nullptr;
  const Reading* ago = nullptr;
  for (int i = 0; i < histN; i++) {
    const Reading& h = hist[i];
    if (h.when >= latest.when) continue;
    if (latest.when - h.when < 120) continue;  // Dexcom often returns the same measurement twice, seconds apart
    prev = &h;  // ascending, so the last one older than that wins
    time_t diff = h.when > target ? h.when - target : target - h.when;
    if (diff <= TOLERANCE && (!ago || diff < (ago->when > target ? ago->when - target : target - ago->when))) ago = &h;
  }
  if (prev && latest.when - prev->when <= 30 * 60) { d.has1 = true; d.d1 = latest.mgdl - prev->mgdl; }
  if (ago) { d.has15 = true; d.d15 = latest.mgdl - ago->mgdl; }
  return d;
}

// ---------- long-term daily totals (for the HbA1c estimate) ----------

static const int DAYS_MAX = 30;
struct DayAcc {
  uint32_t day;   // UTC days since the epoch
  uint32_t secs;  // seconds of readings
  uint64_t sum;   // mg/dL x seconds
};
static DayAcc days[DAYS_MAX];  // ascending by day
static int daysN = 0;
static time_t accLastWhen = 0;  // newest reading already counted ...
static int accLastVal = 0;      // ... and its value (its interval is counted when the next one arrives)
static String accAccount;       // hash of the credentials the totals belong to
static bool accDirty = false;
static unsigned long accSavedMs = 0;

static void accClear() {
  daysN = 0;
  accLastWhen = 0;
  accLastVal = 0;
  accDirty = true;
}

static void accCredit(time_t when, int mgdl, time_t secs) {
  uint32_t day = (uint32_t)(when / 86400);
  int i = daysN - 1;
  while (i >= 0 && days[i].day > day) i--;
  if (i < 0 || days[i].day != day) {  // new day: insert after i
    if (daysN == DAYS_MAX) {          // full: drop the oldest
      for (int k = 1; k < daysN; k++) days[k - 1] = days[k];
      daysN--;
      i--;
    }
    for (int k = daysN; k > i + 1; k--) days[k] = days[k - 1];
    i++;
    daysN++;
    days[i] = {day, 0, 0};
  }
  days[i].secs += (uint32_t)secs;
  days[i].sum += (uint64_t)mgdl * (uint64_t)secs;
}

// "1|<account>|<last when>|<last value>|<day>:<secs>:<sum>;..."
static String accSerialize() {
  char head[32];
  snprintf(head, sizeof(head), "|%ld|%d|", (long)accLastWhen, accLastVal);
  String s = "1|" + accAccount + head;
  for (int i = 0; i < daysN; i++) {
    char b[48];
    snprintf(b, sizeof(b), "%lu:%lu:%llu;", (unsigned long)days[i].day, (unsigned long)days[i].secs, (unsigned long long)days[i].sum);
    s += b;
  }
  return s;
}

// Loads the saved totals if they belong to `account`; otherwise starts empty.
static void accLoad(const String& account) {
  accClear();
  accAccount = account;
  String s = cfgGet("hba1c");
  const char* p = s.c_str();
  char acct[80];
  long lw = 0;
  int lv = 0, n = 0;
  if (sscanf(p, "1|%79[^|]|%ld|%d|%n", acct, &lw, &lv, &n) < 3 || n == 0 || account != acct) return;
  accLastWhen = (time_t)lw;
  accLastVal = lv;
  p += n;
  unsigned long d, sc;
  unsigned long long sm;
  int used;
  while (daysN < DAYS_MAX && sscanf(p, "%lu:%lu:%llu;%n", &d, &sc, &sm, &used) == 3) {
    days[daysN++] = {(uint32_t)d, (uint32_t)sc, (uint64_t)sm};
    p += used;
  }
  accDirty = false;
}

// Counts the readings that arrived since the last call.
static void accIngest() {
  for (int i = 0; i < histN; i++) {
    const Reading& r = hist[i];
    if (r.when <= accLastWhen) continue;
    if (accLastWhen > 0) {
      time_t dt = r.when - accLastWhen;
      accCredit(accLastWhen, accLastVal, dt > 900 ? 900 : dt);  // never across a long gap
    }
    accLastWhen = r.when;
    accLastVal = r.mgdl;
    accDirty = true;
  }
  if (accDirty && (!accSavedMs || millis() - accSavedMs > 30UL * 60UL * 1000UL)) {
    cfgSet("hba1c", accSerialize());
    accSavedMs = millis() ? millis() : 1;
    accDirty = false;
  }
}

Hba1c glucoseHba1c() {
  Hba1c h;
  if (daysN == 0) return h;
  const uint32_t newest = days[daysN - 1].day;
  uint64_t secs = 0, sum = 0;
  for (int i = 0; i < daysN; i++) {
    if (days[i].day + HBA1C_DAYS <= newest) continue;  // older than the window
    secs += days[i].secs;
    sum += days[i].sum;
  }
  h.coveredDays = (float)secs / 86400.0f;
  if (secs < (uint64_t)(HBA1C_MIN_COVERAGE * HBA1C_DAYS * 86400)) return h;
  float mean = (float)((double)sum / (double)secs);
  h.ok = true;
  h.percent = 3.31f + 0.02392f * mean;
  h.mmolMol = (h.percent - 2.15f) * 10.929f;
  return h;
}

// ---------- Nightscout ----------

static FetchResult fetchNightscout() {
  String base = cfgGet("ns_url");
  if (!base.length()) { FetchResult r; r.notConfigured = true; r.error = T("Nightscout URL not set"); return r; }
  String url = base + "/api/v1/entries/sgv.json?count=" + String(fetchWindowMinutes() / 5 + 6);  // one reading per 5 min
  String tok = cfgGet("ns_token");
  if (tok.length()) url += "&token=" + urlEncode(tok);

  HttpResponse r = httpRequest("GET", url, "", {{"Accept", "application/json"}});
  if (r.status <= 0) return fail("Nightscout: " + r.error);
  if (r.status == 401 || r.status == 403) return fail(String("Nightscout: ") + T("access denied (check token)"));
  if (r.status != 200) return fail("Nightscout: HTTP " + String(r.status));

  JsonDocument filter;
  filter[0]["sgv"] = true;
  filter[0]["date"] = true;
  filter[0]["direction"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, r.body.c_str(), DeserializationOption::Filter(filter))) return fail(String("Nightscout: ") + T("unreadable response"));
  JsonArray a = doc.as<JsonArray>();
  if (a.isNull() || a.size() == 0) return fail(String("Nightscout: ") + T("no readings"));

  Reading newest;
  for (JsonObject o : a) {
    int sgv = (int)lround(o["sgv"] | 0.0);
    time_t when = (time_t)((o["date"] | 0.0) / 1000.0);
    Trend t = trendFromName(o["direction"] | "");
    if (sgv <= 0 || when <= 0) continue;
    historyAdd(sgv, when, t);
    if (when > newest.when) { newest.valid = true; newest.mgdl = sgv; newest.when = when; newest.trend = t; }
  }
  if (!newest.valid) return fail(String("Nightscout: ") + T("reading has no value"));
  return succeed(newest.mgdl, newest.trend, newest.when);
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
  error = code.length() ? "Dexcom: " + code : "Dexcom: " + tf("login failed (HTTP %d)", r.status);
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
    FetchResult r; r.notConfigured = true; r.error = T("Dexcom username/password not set"); return r;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    String error;
    if (!dexSession.length() && !dexLogin(error)) return fail(error);
    HttpResponse r = httpRequest("POST",
        dexBase() + "/Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + dexSession + "&minutes=" + String(fetchWindowMinutes()) + "&maxCount=288",  // 288 is the most Share returns (~16 h)
        "", {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
    if (r.status <= 0) return fail("Dexcom: " + r.error);
    String code = dexErrorCode(r.body);
    if (code == "SessionNotValid" || code == "SessionIdNotFound") { dexSession = ""; continue; }
    if (r.status != 200) return fail(code.length() ? "Dexcom: " + code : "Dexcom: HTTP " + String(r.status));

    JsonDocument doc;
    if (deserializeJson(doc, r.body.c_str())) return fail(String("Dexcom: ") + T("unreadable response"));
    JsonArray a = doc.as<JsonArray>();
    if (a.isNull() || a.size() == 0) return fail(String("Dexcom: ") + T("no recent readings"));
    Reading newest;
    for (JsonObject o : a) {
      int val = o["Value"] | 0;
      Trend t = o["Trend"].is<int>() ? trendFromDexcomNumber(o["Trend"].as<int>()) : trendFromName(o["Trend"] | "");
      const char* ts = o["ST"] | (o["DT"] | (o["WT"] | ""));  // "Date(1700000000000)" or "/Date(...-0500)/"
      const char* p = strchr(ts, '(');
      time_t when = p ? (time_t)(strtoll(p + 1, nullptr, 10) / 1000) : 0;
      if (val <= 0 || when <= 0) continue;
      historyAdd(val, when, t);
      if (when > newest.when) { newest.valid = true; newest.mgdl = val; newest.when = when; newest.trend = t; }
    }
    if (!newest.valid) return fail(String("Dexcom: ") + T("reading has no value"));
    return succeed(newest.mgdl, newest.trend, newest.when);
  }
  return fail(String("Dexcom: ") + T("session expired"));
}

// ---------- LibreLinkUp ----------

static String lluToken, lluAccountHash, lluRegion, lluPatient;

// "M/D/YYYY h:mm:ss AM" (UTC) -> epoch seconds, 0 if unreadable
static time_t parseLibreStamp(const char* str) {
  int mo, d, y, H, M, S;
  char ap[3] = {};
  if (sscanf(str, "%d/%d/%d %d:%d:%d %2s", &mo, &d, &y, &H, &M, &S, ap) != 7) return 0;
  if (!strcmp(ap, "PM") && H < 12) H += 12;
  if (!strcmp(ap, "AM") && H == 12) H = 0;
  return civilToEpoch(y, mo, d, H, M, S);
}

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
    if (deserializeJson(d, r.body.c_str())) { error = "LibreLinkUp: " + tf("unreadable login response (HTTP %d)", r.status); return false; }
    int status = d["status"] | -1;
    if (status == 2) { error = String("LibreLinkUp: ") + T("wrong email or password"); return false; }
    if (status == 4) { error = String("LibreLinkUp: ") + T("accept the new terms in the LibreLinkUp app"); return false; }
    if (status != 0) { error = "LibreLinkUp: " + tf("login status %d", status); return false; }
    if (d["data"]["redirect"] | false) {
      String region = String(d["data"]["region"] | "");
      if (!region.length() || region == lluRegion) { error = String("LibreLinkUp: ") + T("region redirect failed"); return false; }
      lluRegion = region;  // e.g. "de"
      continue;
    }
    const char* token = d["data"]["authTicket"]["token"] | "";
    const char* uid = d["data"]["user"]["id"] | "";
    if (!*token || !*uid) { error = String("LibreLinkUp: ") + T("login needs an extra step (open the LibreLinkUp app)"); return false; }
    lluToken = token;
    lluAccountHash = sha256Hex(uid);
    return true;
  }
  error = String("LibreLinkUp: ") + T("region redirect loop");
  return false;
}

static FetchResult fetchLibre() {
  if (!cfgGet("libre_email").length() || !cfgGet("libre_pass").length()) {
    FetchResult r; r.notConfigured = true; r.error = T("LibreLinkUp email/password not set"); return r;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    String error;
    if (!lluToken.length() && !lluLogin(error)) return fail(error);
    HttpResponse r = httpRequest("GET", lluBase(lluRegion) + "/llu/connections", "", lluHeaders(true));
    if (r.status <= 0) return fail("LibreLinkUp: " + r.error);
    if (r.status == 401) { lluToken = ""; continue; }  // token expired: log in again
    if (r.status != 200) return fail("LibreLinkUp: HTTP " + String(r.status));

    JsonDocument doc;
    if (deserializeJson(doc, r.body.c_str())) return fail(String("LibreLinkUp: ") + T("unreadable response"));
    JsonArray conns = doc["data"].as<JsonArray>();
    if (conns.isNull() || conns.size() == 0) return fail(String("LibreLinkUp: ") + T("no connections (share your data in LibreLink)"));
    JsonObject gm = conns[0]["glucoseMeasurement"];
    int mgdl = (int)lround(gm["ValueInMgPerDl"] | 0.0);
    if (mgdl <= 0) return fail(String("LibreLinkUp: ") + T("no glucose measurement"));
    time_t when = parseLibreStamp(gm["FactoryTimestamp"] | "");  // UTC
    Trend trend = trendFromLibreArrow(gm["TrendArrow"] | 0);
    lluPatient = String(conns[0]["patientId"] | "");
    historyAdd(mgdl, when, trend);

    // The connections call only has the latest value: fetch the graph when ~15 min of history
    // (deltas) or part of the graph window is missing.
    static unsigned long lastBackfill = 0;
    Reading latest;
    latest.valid = true; latest.mgdl = mgdl; latest.when = when;
    if (lluPatient.length() && (!glucoseDeltas(latest).has15 || historySpan() < GRAPH_HOURS * 3600 - 900) && (!lastBackfill || millis() - lastBackfill > 300000UL)) {
      lastBackfill = millis();
      HttpResponse g = httpRequest("GET", lluBase(lluRegion) + "/llu/connections/" + lluPatient + "/graph", "", lluHeaders(true));
      if (g.status == 200) {
        JsonDocument filter;
        filter["data"]["graphData"][0]["ValueInMgPerDl"] = true;
        filter["data"]["graphData"][0]["FactoryTimestamp"] = true;
        JsonDocument gd;
        if (!deserializeJson(gd, g.body.c_str(), DeserializationOption::Filter(filter)))
          for (JsonObject o : gd["data"]["graphData"].as<JsonArray>())
            historyAdd((int)lround(o["ValueInMgPerDl"] | 0.0), parseLibreStamp(o["FactoryTimestamp"] | ""), TREND_UNKNOWN);
      }
    }
    return succeed(mgdl, trend, when);
  }
  return fail(String("LibreLinkUp: ") + T("session expired"));
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
    dexSession = lluToken = lluAccountHash = lluRegion = lluPatient = "";
    historyClear();
    accLoad(sha256Hex(key));  // the saved daily totals only stay if they belong to this account
  }

  if (WiFi.status() != WL_CONNECTED) return fail(T("WiFi disconnected"));
  if (time(nullptr) < 1700000000) return fail(T("Waiting for the clock (NTP)"));

  String src = cfgGet("source");
  FetchResult r = src == "dexcom" ? fetchDexcom() : src == "libre" ? fetchLibre() : fetchNightscout();
  if (r.ok) accIngest();
  return r;
}
