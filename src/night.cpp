#include "night.h"
#include <Arduino.h>
#include <stdio.h>
#include <time.h>
#include "config.h"
#include "display.h"
#include "lvgl_port.h"
#include "ui.h"

static int minutesOfDay(const String& hhmm) {
  int h = 0, m = 0;
  sscanf(hhmm.c_str(), "%d:%d", &h, &m);
  return h * 60 + m;
}

static bool isNight() {
  String mode = cfgGet("night_mode");
  if (mode == "on") return true;
  if (mode != "auto") return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // the clock is not set yet
  uiApplyTimeZone();
  struct tm t;
  localtime_r(&now, &t);
  int cur = t.tm_hour * 60 + t.tm_min, from = minutesOfDay(cfgGet("night_from")), to = minutesOfDay(cfgGet("night_to"));
  if (from == to) return false;
  return from < to ? (cur >= from && cur < to) : (cur >= from || cur < to);  // the night may run over midnight
}

void nightTick() {
  static unsigned long last = 0;
  if (last && millis() - last < 2000) return;
  last = millis();
  static int shownNight = -1;
#ifdef UI_PROFILE_LCD
  static int shownLevel = -1;
#endif
  bool night = isNight();
  // In a dark room change the colours first and the brightness second, in both directions: going dark the
  // screen turns black and then dims; coming back it turns light while still dim and then brightens.
  if ((int)night != shownNight) {
    shownNight = night;
    dashboardSetNight(night);
  }
#ifdef UI_PROFILE_LCD
  int level = atoi(cfgGet(night ? "night_level" : "brightness").c_str());
  if (level != shownLevel) {
    shownLevel = level;
    portSetBacklight(level);
  }
#endif
}
