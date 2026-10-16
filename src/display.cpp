#include "display.h"
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "wifi_setup.h"  // drawQR

// ePaper policy: a full refresh flashes and wears the panel, so it is only used when the screen
// layout changes and every FULL_REFRESH_MS to clear ghosting. Everything else (new value, age
// text, error line) is a partial refresh of just that window.
#ifndef FULL_REFRESH_MS
#define FULL_REFRESH_MS (30UL * 60UL * 1000UL)
#endif

namespace {

enum Mode { MODE_NONE, MODE_NOT_CONFIGURED, MODE_NO_DATA, MODE_READING };
struct Rect { int x, y, w, h; };
const Rect R_VALUE = {0, 50, 800, 260};  // number, arrow, LOW/HIGH
const Rect R_AGE = {0, 340, 500, 40};
const Rect R_ERROR = {0, 425, 800, 45};

// What is currently on the panel.
struct Shown {
  Mode mode = MODE_NONE;
  String header, url, error, status;
  int mgdl = -1;
  Trend trend = TREND_UNKNOWN;
  int ageMins = -1;
  time_t when = 0;
} shown;
unsigned long lastFullMs = 0;

}  // namespace

static void drawArrow(EPaper& e, int cx, int cy, int len, Trend t) {
  float deg;
  int heads = 1;
  switch (t) {
    case TREND_DOUBLE_UP: heads = 2; deg = -90; break;
    case TREND_SINGLE_UP: deg = -90; break;
    case TREND_45_UP: deg = -45; break;
    case TREND_FLAT: deg = 0; break;
    case TREND_45_DOWN: deg = 45; break;
    case TREND_SINGLE_DOWN: deg = 90; break;
    case TREND_DOUBLE_DOWN: heads = 2; deg = 90; break;
    default: e.drawString("?", cx - 20, cy - 40, 4); return;
  }
  float a = deg * M_PI / 180.0f, dx = cosf(a), dy = sinf(a), px = -dy, py = dx;
  float hl = len * 0.35f, hw = len * 0.28f, sw = len * 0.06f;
  float tx = cx - dx * len / 2, ty = cy - dy * len / 2;  // tail
  float ex = cx + dx * len / 2, ey = cy + dy * len / 2;  // tip
  float bx = ex - dx * hl, by = ey - dy * hl;            // base of the first head
  auto P = [](float v) { return (int)lroundf(v); };
  e.fillTriangle(P(tx + px * sw), P(ty + py * sw), P(tx - px * sw), P(ty - py * sw), P(bx + px * sw), P(by + py * sw), TFT_BLACK);
  e.fillTriangle(P(tx - px * sw), P(ty - py * sw), P(bx + px * sw), P(by + py * sw), P(bx - px * sw), P(by - py * sw), TFT_BLACK);
  for (int h = 0; h < heads; h++) {
    float sx = ex - dx * hl * 0.9f * h, sy = ey - dy * hl * 0.9f * h;  // second head sits behind the first
    e.fillTriangle(P(sx), P(sy), P(sx - dx * hl + px * hw), P(sy - dy * hl + py * hw),
                   P(sx - dx * hl - px * hw), P(sy - dy * hl - py * hw), TFT_BLACK);
  }
}

static int ageMinutes(time_t when) {
  long mins = (long)((time(nullptr) - when) / 60);
  return mins < 0 ? 0 : (int)mins;
}

static String ageText(int mins) {
  if (mins < 1) return "just now";
  if (mins < 60) return String(mins) + " min ago";
  return String(mins / 60) + " h " + String(mins % 60) + " min ago";
}

static void clearRect(EPaper& e, const Rect& r) { e.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE); }

static void drawAge(EPaper& e, int mins) {
  clearRect(e, R_AGE);
  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);
  e.drawString("Updated " + ageText(mins), 40, 350, 4);
}

static void drawError(EPaper& e, const String& error) {
  clearRect(e, R_ERROR);
  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);
  if (error.length()) e.drawString("! " + error, 20, 440, 2);
}

static void drawValue(EPaper& e, const Reading& last, bool mmol, const String& status) {
  clearRect(e, R_VALUE);
  e.setTextColor(TFT_BLACK);
  char buf[16];
  if (mmol) snprintf(buf, sizeof(buf), "%.1f", last.mgdl / 18.016);
  else snprintf(buf, sizeof(buf), "%d", last.mgdl);
  e.setTextSize(2);
  e.drawString(buf, 40, 55, 8);  // font 8: 75 px digits, doubled
  int numberWidth = e.textWidth(buf, 8);
  e.drawString(status, 40, 250, 4);
  e.setTextSize(1);
  drawArrow(e, 40 + numberWidth + 110, 130, 130, last.trend);
}

static void partial(EPaper& e, const Rect& r) { e.updataPartial(r.x, r.y, r.w, r.h); }

void drawDashboard(EPaper& e, const Reading& last, const String& error, bool notConfigured,
                   const String& configUrl) {
  bool mmol = cfgGet("unit") == "mmol";
  Mode mode = notConfigured ? MODE_NOT_CONFIGURED : last.valid ? MODE_READING : MODE_NO_DATA;
  String header = String(sourceName()) + "   " + (mmol ? "mmol/L" : "mg/dL");
  int low = atoi(cfgGet("low").c_str()), high = atoi(cfgGet("high").c_str());
  String status = last.mgdl < low ? "LOW" : last.mgdl > high ? "HIGH" : "IN RANGE";
  int mins = last.valid ? ageMinutes(last.when) : -1;

  bool layoutChanged = shown.mode == MODE_NONE || mode != shown.mode || header != shown.header;
  bool errorChanged = error != shown.error;
  bool valueChanged = mode == MODE_READING &&
                      (last.mgdl != shown.mgdl || last.trend != shown.trend || status != shown.status);
  bool ageChanged = mode == MODE_READING && mins != shown.ageMins;
  bool urlChanged = mode == MODE_NOT_CONFIGURED && configUrl != shown.url;

  if (!layoutChanged && !errorChanged && !valueChanged && !ageChanged && !urlChanged) return;  // panel stays untouched

  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);
  if (mode == MODE_READING && !layoutChanged) {
    // in-place update: only redraw (and refresh) what changed
    if (valueChanged) drawValue(e, last, mmol, status);
    if (ageChanged) drawAge(e, mins);
    if (errorChanged) drawError(e, error);
    if (millis() - lastFullMs >= FULL_REFRESH_MS) {
      e.update();
      lastFullMs = millis();
    } else {
      if (valueChanged && ageChanged) e.updataPartial(0, R_VALUE.y, 800, R_AGE.y + R_AGE.h - R_VALUE.y);  // one refresh, not two
      else if (valueChanged) partial(e, R_VALUE);
      else if (ageChanged) partial(e, R_AGE);
      if (errorChanged) partial(e, R_ERROR);
    }
  } else {
    e.fillScreen(TFT_WHITE);
    e.drawString(header, 20, 20, 4);
    if (mode == MODE_READING) {
      drawValue(e, last, mmol, status);
      drawAge(e, mins);
    } else if (mode == MODE_NOT_CONFIGURED) {
      e.drawString("Not configured", 20, 100, 4);
      e.drawString("Open this page to choose your", 20, 160, 2);
      e.drawString("glucose data source:", 20, 190, 2);
      e.drawString(configUrl, 20, 240, 2);
      drawQR(e, 520, 80, 7, configUrl.c_str());
    } else {
      e.drawString("No data yet", 20, 100, 4);
    }
    if (mode != MODE_NOT_CONFIGURED) drawError(e, error);
    e.update();
    lastFullMs = millis();
  }

  shown.mode = mode;
  shown.header = header;
  shown.url = configUrl;
  shown.error = error;
  shown.status = status;
  shown.mgdl = last.mgdl;
  shown.trend = last.trend;
  shown.ageMins = mins;
  shown.when = last.when;
}

void dashboardTick(EPaper& e) {
  if (shown.mode != MODE_READING) return;
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  int mins = ageMinutes(shown.when);
  if (mins == shown.ageMins) return;
  shown.ageMins = mins;
  drawAge(e, mins);
  if (millis() - lastFullMs >= FULL_REFRESH_MS) {
    e.update();
    lastFullMs = millis();
  } else {
    partial(e, R_AGE);
  }
}
