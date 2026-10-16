#pragma once
#include <Arduino.h>

// Settings editable from the web page (config_server.cpp), stored in NVS.
struct ConfigField {
  const char* key;      // NVS key (max 15 chars)
  const char* label;
  const char* type;     // text | url | password | number | select
  const char* group;    // "" = always shown, else only for that data source
  const char* options;  // select only: "value:Label|value:Label"
  const char* def;
};

extern const ConfigField CONFIG_FIELDS[];
extern const int CONFIG_FIELD_COUNT;

String cfgGet(const char* key);  // stored value, or the field's default
void cfgSet(const char* key, const String& value);

// Poll period is fixed by the data source: Dexcom updates every 5 min, others every 1 min.
int refreshMinutes();
