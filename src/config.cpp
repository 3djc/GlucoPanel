#include "config.h"
#include <Preferences.h>

// What a dashboard block can show (see cells.h).
#define BLOCK_OPTIONS \
  "none:Empty|value:Glucose value|arrow:Trend arrow|delta1:Change vs last reading|delta15:Change vs 15 min ago|delta30:Change vs 30 min ago|" \
  "clock:Date and time|readtime:Time of the reading|age:Age of the reading|fresh:Freshness bar|" \
  "avg:Average glucose|tir:Time in range|range:Lowest and highest|cv:Variability (CV)|hba1c:Estimated HbA1c"

const ConfigField CONFIG_FIELDS[] = {
  {"lang", "Language", "select", "", "en:English|fr:Français|de:Deutsch|it:Italiano|es:Español", "en"},

  {"source", "Data source", "select", "", "nightscout:Nightscout|libre:LibreLinkUp|dexcom:Dexcom Share", "nightscout"},

  {"ns_url", "Nightscout URL", "url", "nightscout", "", ""},
  {"ns_token", "Access token (optional)", "password", "nightscout", "", ""},

  {"libre_email", "Email", "text", "libre", "", ""},
  {"libre_pass", "Password", "password", "libre", "", ""},
  {"libre_region", "Region", "select", "libre",
   "eu:Europe|eu2:Europe 2|us:United States|de:Germany|fr:France|jp:Japan|ap:Asia-Pacific|au:Australia|ae:UAE|ca:Canada", "eu"},

  {"dex_user", "Username", "text", "dexcom", "", ""},
  {"dex_pass", "Password", "password", "dexcom", "", ""},
  {"dex_region", "Server", "select", "dexcom", "us:United States|ous:Outside US|jp:Japan", "us"},

  {"unit", "Display unit", "select", "", "mgdl:mg/dL|mmol:mmol/L", "mgdl"},
  {"hba1c_unit", "HbA1c unit", "select", "", "percent:% (NGSP)|mmolmol:mmol/mol (IFCC)", "percent"},
  {"trusted_ips", "Trusted IP addresses (no code needed)", "text", "", "", ""},
  {"tls_verify", "Check server certificates", "select", "", "on:Yes (recommended)|off:No (insecure)", "on"},
  {"auto_update", "Install updates automatically", "select", "", "on:On|off:Off", "on"},
  {"update_hour", "Install updates at (hour, 0-23)", "number", "", "0:23", "3"},
  {"tz", "Time zone", "select", "",
   "UTC0:UTC|GMT0BST,M3.5.0/1,M10.5.0:London|CET-1CEST,M3.5.0,M10.5.0/3:Paris / Berlin / Madrid|EET-2EEST,M3.5.0/3,M10.5.0/4:Helsinki / Athens|"
   "EST5EDT,M3.2.0,M11.1.0:New York|CST6CDT,M3.2.0,M11.1.0:Chicago|MST7MDT,M3.2.0,M11.1.0:Denver|PST8PDT,M3.2.0,M11.1.0:Los Angeles|"
   "AEST-10AEDT,M10.1.0,M4.1.0/3:Sydney", "CET-1CEST,M3.5.0,M10.5.0/3"},
  // Night mode (white on black); the backlight levels only exist where there is a backlight (the colour LCD)
#ifdef UI_PROFILE_LCD
  {"brightness", "Brightness (%)", "number", "", "10:100", "100"},
#endif
  {"night_mode", "Night mode (dark theme)", "select", "", "off:Off|on:On|auto:Scheduled", "off"},
  {"night_from", "Night starts at", "time", "", "", "22:00"},
  {"night_to", "Night ends at", "time", "", "", "07:00"},
#ifdef UI_PROFILE_LCD
  {"night_level", "Night brightness (%)", "number", "", "5:100", "10"},
#endif
  {"low", "Low threshold (mg/dL)", "number", "", "", "70"},
  {"high", "High threshold (mg/dL)", "number", "", "", "180"},

  {"blk_tl", "Top row, left", "select", "", BLOCK_OPTIONS, "arrow"},
  {"blk_tm", "Top row, middle", "select", "", BLOCK_OPTIONS, "delta1"},
  {"blk_tr", "Top row, right", "select", "", BLOCK_OPTIONS, "delta15"},
  {"blk_ml", "Middle row, left", "select", "", BLOCK_OPTIONS, "clock"},
  {"blk_mc", "Middle row, centre", "select", "", BLOCK_OPTIONS, "value"},
  {"blk_mr", "Middle row, right", "select", "", BLOCK_OPTIONS, "fresh"},
  {"blk_bottom", "Bottom row", "select", "", "graph:Glucose graph (3 h)|none:Empty", "graph"},
};
const int CONFIG_FIELD_COUNT = sizeof(CONFIG_FIELDS) / sizeof(CONFIG_FIELDS[0]);

static const char* NS = "gluco";

String cfgGet(const char* key) {
  String def;
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++)
    if (!strcmp(CONFIG_FIELDS[i].key, key)) def = CONFIG_FIELDS[i].def;
  Preferences prefs;
  prefs.begin(NS, false);
  String v = prefs.getString(key, def);
  prefs.end();
  return v;
}

void cfgSet(const char* key, const String& value) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putString(key, value);
  prefs.end();
}

int refreshMinutes() { return cfgGet("source") == "dexcom" ? 5 : 1; }
