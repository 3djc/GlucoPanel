// Note: no Seeed_GFX header may be included before WebServer.h (FS_NO_GLOBALS breaks it).
#include <Arduino.h>
#ifdef ARDUINO
#include <FS.h>
#endif
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include "config.h"

static WebServer server(80);
static bool settingsChanged = false;

static String esc(const String& s) {
  String o;
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

// Iterates "value:Label|value:Label".
static bool nextOption(const char*& p, String& val, String& label) {
  if (!*p) return false;
  val = "";
  label = "";
  while (*p && *p != ':') val += *p++;
  if (*p == ':') p++;
  while (*p && *p != '|') label += *p++;
  if (*p == '|') p++;
  return true;
}

static const char* PAGE_HEAD =
  "<!doctype html><html><head><meta charset=utf-8>"
  "<meta name=viewport content='width=device-width,initial-scale=1'><title>GlucoPanel</title>"
  "<style>body{font:16px system-ui,sans-serif;max-width:30rem;margin:0 auto;padding:1rem}"
  "label{display:block;margin:.8rem 0 .2rem;font-weight:600}"
  "input,select{width:100%;padding:.5rem;font:inherit;box-sizing:border-box}"
  "button{margin-top:1rem;padding:.6rem 1rem;font:inherit}"
  ".ok{background:#d8f5d8;padding:.5rem}.err{background:#fbd8d8;padding:.5rem}"
  "hr{margin:2rem 0}.danger{color:#b00020}</style></head><body><h1>GlucoPanel</h1>";

static void handleRoot() {
  String h = PAGE_HEAD;
  if (server.hasArg("saved")) h += "<p class=ok>Saved.</p>";
  h += "<form method=post action=/save>";
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    const ConfigField& f = CONFIG_FIELDS[i];
    String cur = cfgGet(f.key);
    bool grouped = f.group[0];
    if (grouped) h += String("<div class='grp' data-g='") + f.group + "'>";
    h += String("<label>") + f.label + "</label>";
    if (!strcmp(f.type, "select")) {
      h += String("<select name='") + f.key + "'" + (i == 0 ? " id=source" : "") + ">";
      const char* p = f.options;
      String v, l;
      while (nextOption(p, v, l))
        h += String("<option value='") + v + "'" + (v == cur ? " selected" : "") + ">" + l + "</option>";
      h += "</select>";
    } else if (!strcmp(f.type, "password")) {
      // never echo stored secrets; blank keeps the current value
      h += String("<input type=password name='") + f.key + "' autocomplete=off placeholder='" +
           (cur.length() ? "(saved - leave blank to keep)" : "") + "'>";
    } else {
      h += String("<input type='") + f.type + "' name='" + f.key + "' value='" + esc(cur) + "'>";
    }
    if (grouped) h += "</div>";
  }
  h += "<button>Save</button></form>";
  h += String("<p>Refresh: every ") + String(refreshMinutes()) + " min (set by the data source).</p>";
  h += "<hr><form method=post action=/reset-wifi "
       "onsubmit=\"return confirm('Erase saved WiFi and reboot into setup mode?')\">"
       "<button class=danger>Erase WiFi and reboot into setup</button></form>"
       "<script>const s=document.getElementById('source');"
       "function sync(){document.querySelectorAll('.grp').forEach(d=>"
       "d.style.display=d.dataset.g===s.value?'':'none')}"
       "s.onchange=sync;sync();</script></body></html>";
  server.send(200, "text/html", h);
}

static void fail(const String& msg) {
  server.send(400, "text/html", String(PAGE_HEAD) + "<p class=err>" + esc(msg) +
                                  "</p><p><a href=/>Back</a></p></body></html>");
}

static void handleSave() {
  String vals[CONFIG_FIELD_COUNT];
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    const ConfigField& f = CONFIG_FIELDS[i];
    if (!server.hasArg(f.key)) { vals[i] = cfgGet(f.key); continue; }  // field not submitted: keep it
    String v = server.arg(f.key);
    String t;  // trim, and drop control characters
    for (int k = 0; k < v.length(); k++)
      if ((unsigned char)v[k] >= 32 && !(v[k] == ' ' && t.length() == 0)) t += v[k];
    while (t.length() && t[t.length() - 1] == ' ') t.remove(t.length() - 1);
    v = t;

    if (!strcmp(f.type, "password") && v.length() == 0) { vals[i] = cfgGet(f.key); continue; }
    if (!strcmp(f.type, "select")) {
      const char* p = f.options;
      String ov, ol;
      bool found = false;
      while (nextOption(p, ov, ol)) found |= (ov == v);
      if (!found) return fail(String("Invalid value for ") + f.label);
    } else if (!strcmp(f.type, "url") && v.length()) {
      if (!v.startsWith("http://") && !v.startsWith("https://"))
        return fail(String(f.label) + " must start with http:// or https://");
      while (v.endsWith("/")) v.remove(v.length() - 1);
    } else if (!strcmp(f.type, "number")) {
      if (atoi(v.c_str()) < 20 || atoi(v.c_str()) > 400)
        return fail(String(f.label) + " must be between 20 and 400");
      v = String(atoi(v.c_str()));
    }
    vals[i] = v;
  }
  int low = 0, high = 0;
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    if (!strcmp(CONFIG_FIELDS[i].key, "low")) low = atoi(vals[i].c_str());
    if (!strcmp(CONFIG_FIELDS[i].key, "high")) high = atoi(vals[i].c_str());
  }
  if (low >= high) return fail("Low threshold must be below the high threshold");

  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) cfgSet(CONFIG_FIELDS[i].key, vals[i]);
  settingsChanged = true;
  server.sendHeader("Location", "/?saved=1");
  server.send(303, "text/plain", "");
}

static void handleResetWifi() {
  server.send(200, "text/html", String(PAGE_HEAD) +
              "<p class=ok>WiFi erased. Rebooting: join the <b>GlucoPanel</b> hotspot to set it up again.</p>"
              "</body></html>");
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void configServerBegin() {
  MDNS.begin("glucopanel");
  MDNS.addService("http", "tcp", 80);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset-wifi", HTTP_POST, handleResetWifi);
  server.begin();
}

void configServerHandle() { server.handleClient(); }

bool configServerTakeChanged() {
  bool c = settingsChanged;
  settingsChanged = false;
  return c;
}
