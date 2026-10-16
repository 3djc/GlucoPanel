#pragma once
#include <Arduino.h>
#include <time.h>

enum Trend {
  TREND_UNKNOWN, TREND_DOUBLE_DOWN, TREND_SINGLE_DOWN, TREND_45_DOWN, TREND_FLAT,
  TREND_45_UP, TREND_SINGLE_UP, TREND_DOUBLE_UP
};

struct Reading {
  bool valid = false;
  int mgdl = 0;
  Trend trend = TREND_UNKNOWN;
  time_t when = 0;  // UTC epoch seconds of the measurement
};

struct FetchResult {
  bool ok = false;
  bool notConfigured = false;
  Reading reading;
  String error;
};

void glucoseInit();       // starts NTP (timestamps need a correct clock)
FetchResult fetchGlucose();  // fetches from the data source chosen in the settings page
const char* sourceName();    // display name of the configured source
