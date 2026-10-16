#include "display.h"
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "wifi_setup.h"  // drawQR

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

static String ageText(time_t when) {
  long mins = (long)((time(nullptr) - when) / 60);
  if (mins < 0) mins = 0;
  if (mins < 1) return "just now";
  if (mins < 60) return String((int)mins) + " min ago";
  return String((int)(mins / 60)) + " h " + String((int)(mins % 60)) + " min ago";
}

void drawDashboard(EPaper& e, const Reading& last, const String& error, bool notConfigured,
                   const String& configUrl) {
  e.fillScreen(TFT_WHITE);
  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);

  bool mmol = cfgGet("unit") == "mmol";
  e.drawString(String(sourceName()) + "   " + (mmol ? "mmol/L" : "mg/dL"), 20, 20, 4);

  if (!last.valid) {
    if (notConfigured) {
      e.drawString("Not configured", 20, 100, 4);
      e.drawString("Open this page to choose your", 20, 160, 2);
      e.drawString("glucose data source:", 20, 190, 2);
      e.drawString(configUrl, 20, 240, 2);
      drawQR(e, 520, 80, 7, configUrl.c_str());
    } else {
      e.drawString("No data yet", 20, 100, 4);
    }
  } else {
    char buf[16];
    if (mmol) snprintf(buf, sizeof(buf), "%.1f", last.mgdl / 18.016);
    else snprintf(buf, sizeof(buf), "%d", last.mgdl);
    e.setTextSize(6);
    e.drawString(buf, 40, 90, 4);
    e.setTextSize(1);
    drawArrow(e, 600, 220, 150, last.trend);

    int low = atoi(cfgGet("low").c_str()), high = atoi(cfgGet("high").c_str());
    const char* status = last.mgdl < low ? "LOW" : last.mgdl > high ? "HIGH" : "IN RANGE";
    e.setTextSize(2);
    e.drawString(status, 40, 290, 4);
    e.setTextSize(1);
    e.drawString("Updated " + ageText(last.when), 40, 380, 4);
  }

  if (error.length() && !notConfigured) {
    String msg = "! " + error;
    e.drawString(msg, 20, 440, 2);
  }
  e.update();
}
