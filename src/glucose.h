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

// Changes in mg/dL of the latest reading vs the previous one and vs ~15 min earlier,
// computed from the readings collected by recent fetches (cleared when the source changes).
struct Deltas {
  bool has1 = false, has15 = false;
  int d1 = 0, d15 = 0;
};
Deltas glucoseDeltas(const Reading& latest);

// Width of the dashboard graph. Fetchers ask their source for at least this much history.
#ifndef GRAPH_HOURS
#define GRAPH_HOURS 3
#endif

// Copies the newest `max` collected readings (ascending by time) into `out`; returns how many.
int glucoseHistory(Reading* out, int max);

void glucoseInit();       // starts NTP (timestamps need a correct clock)
FetchResult fetchGlucose();  // fetches from the data source chosen in the settings page
const char* sourceName();    // display name of the configured source
