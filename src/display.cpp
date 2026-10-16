#include "display.h"
#include <Arduino.h>
#include <math.h>
#include <algorithm>
#include "config.h"
#include "wifi_setup.h"  // drawQR

// ePaper policy: a full refresh flashes and wears the panel, so it is only used when the screen
// layout changes and every FULL_REFRESH_MS to clear ghosting. Everything else (new reading, age
// text, error text) is a single partial refresh of the smallest window covering what changed.
#ifndef FULL_REFRESH_MS
#define FULL_REFRESH_MS (30UL * 60UL * 1000UL)
#endif

namespace {

const int SCREEN_W = 800, SCREEN_H = 480;
const int COLS = 3, ROWS = 3;

enum Mode { MODE_NONE, MODE_NOT_CONFIGURED, MODE_NO_DATA, MODE_READING };
struct Rect { int x, y, w, h; };

Rect block(int col, int row) {
  int x0 = col * SCREEN_W / COLS, x1 = (col + 1) * SCREEN_W / COLS;
  int y0 = row * SCREEN_H / ROWS, y1 = (row + 1) * SCREEN_H / ROWS;
  return {x0, y0, x1 - x0, y1 - y0};
}

const Rect STRIP = {0, 452, SCREEN_W, 28};  // age + error text along the bottom edge

Rect unite(const Rect& a, const Rect& b) {
  if (a.w == 0) return b;
  int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
  int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
  return {x0, y0, x1 - x0, y1 - y0};
}

// What is currently on the panel.
struct Shown {
  Mode mode = MODE_NONE;
  String name, url, error;
  bool mmol = false;
  int mgdl = -1;
  Trend trend = TREND_UNKNOWN;
  Deltas deltas;
  int ageMins = -1;
  time_t when = 0;
} shown;
unsigned long lastFullMs = 0;

}  // namespace

// ---------- drawing primitives ----------

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
    default:
      e.setTextSize(2);
      e.drawString("?", cx - 14, cy - 26, 4);
      e.setTextSize(1);
      return;
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

static void clearBlock(EPaper& e, int col, int row) {
  Rect r = block(col, row);
  e.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
}

static void drawCentered(EPaper& e, const String& text, const Rect& r, int y, int font) {
  e.drawString(text, r.x + (r.w - e.textWidth(text, font)) / 2, y, font);
}

// ---------- blocks ----------

// ---- top band (like Gluco-Monitor's): rounded, light grey, three cells ----
// The E1001's real 4-level gray mode has no partial refresh, so "light grey" is a sparse ordered-dither
// pattern in plain black/white. LIGHT_GREY_DOTS is the number of black pixels per 4x4 cell (0..16).
#ifndef LIGHT_GREY_DOTS
#define LIGHT_GREY_DOTS 2
#endif

static const Rect BAND = {6, 10, 788, 130};
static const Rect TOP_ROW = {0, 0, SCREEN_W, 150};  // refresh window covering the band
static const int BAND_RADIUS = 10;

static void fillLightGrey(EPaper& e, const Rect& r, int radius) {
  static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  for (int y = 0; y < r.h; y++) {
    for (int x = 0; x < r.w; x++) {
      // skip pixels outside the rounded corners
      int cx = x < radius ? radius - x : x >= r.w - radius ? x - (r.w - radius - 1) : 0;
      int cy = y < radius ? radius - y : y >= r.h - radius ? y - (r.h - radius - 1) : 0;
      if (cx * cx + cy * cy > radius * radius) continue;
      if (bayer[(r.y + y) & 3][(r.x + x) & 3] < LIGHT_GREY_DOTS) e.drawPixel(r.x + x, r.y + y, TFT_BLACK);
    }
  }
}

// "+12" / "-3" / "0" in font 6 (48 px, digits only), so the sign is drawn as bars. The caption sits above.
static void drawDeltaCell(EPaper& e, int cx, bool has, int delta, bool mmol, const char* caption) {
  char buf[16];
  int sign = 0;
  if (!has) {
    snprintf(buf, sizeof(buf), "--");
  } else if (mmol) {
    float v = fabsf(delta / 18.016f);
    snprintf(buf, sizeof(buf), "%.1f", v);
    sign = lroundf(v * 10) == 0 ? 0 : (delta > 0 ? 1 : -1);
  } else {
    snprintf(buf, sizeof(buf), "%d", abs(delta));
    sign = delta > 0 ? 1 : delta < 0 ? -1 : 0;
  }
  const int SIGN_W = 24, GAP = 8, BAR = 6;
  const int CAPTION_Y = BAND.y + 26, VALUE_Y = BAND.y + 54, MID = VALUE_Y + 24;
  int dw = e.textWidth(buf, 6);
  int x = cx - (dw + (sign ? SIGN_W + GAP : 0)) / 2;
  if (sign) {
    e.fillRect(x, MID - BAR / 2, SIGN_W, BAR, TFT_BLACK);                                      // minus bar
    if (sign > 0) e.fillRect(x + (SIGN_W - BAR) / 2, MID - SIGN_W / 2, BAR, SIGN_W, TFT_BLACK);  // + vertical bar
    x += SIGN_W + GAP;
  }
  e.drawString(buf, x, VALUE_Y, 6);
  e.drawString(caption, cx - e.textWidth(caption, 2) / 2, CAPTION_Y, 2);
}

static void drawTopBand(EPaper& e, Trend trend, const Deltas& d, bool mmol) {
  e.fillRect(TOP_ROW.x, TOP_ROW.y, TOP_ROW.w, TOP_ROW.h, TFT_WHITE);
  fillLightGrey(e, BAND, BAND_RADIUS);
  const int CW = BAND.w / 3;
  for (int i = 1; i < 3; i++) e.fillRect(BAND.x + i * CW, BAND.y + 14, 1, BAND.h - 28, TFT_BLACK);  // separators
  drawArrow(e, BAND.x + CW / 2, BAND.y + BAND.h / 2, 90, trend);
  drawDeltaCell(e, BAND.x + CW + CW / 2, d.has1, d.d1, mmol, "vs last");
  drawDeltaCell(e, BAND.x + 2 * CW + CW / 2, d.has15, d.d15, mmol, "vs 15 min");
}

// ---- font 8 (75 px digits, only 0-9 . -) drawn at a fractional scale num/den ----
// The library only scales by whole numbers (2x = 150 px is wider than a block), so the glyph data
// is drawn directly: run-length decoded, nearest-neighbour scaled.
static const int F8_H = 75;

static int font8Width(const char* s, int num, int den) {
  int w = 0;
  for (; *s; s++)
    if ((unsigned char)*s >= 32 && (unsigned char)*s < 128) w += widtbl_f72[(unsigned char)*s - 32];
  return w * num / den;
}

static void drawFont8Scaled(EPaper& e, const char* s, int x, int y, int num, int den) {
  int srcX = 0;
  for (; *s; s++) {
    unsigned char c = *s;
    if (c < 32 || c >= 128) continue;
    int w = widtbl_f72[c - 32];
    const unsigned char* d = chrtbl_f72[c - 32];
    int total = w * F8_H, pc = 0;
    while (pc < total) {
      unsigned char b = *d++;
      int run = (b & 0x7F) + 1;
      if (b & 0x80) {  // foreground run: split it at row ends
        int p = pc, left = run;
        while (left > 0) {
          int px = p % w, py = p / w;
          int seg = std::min(left, w - px);
          int x0 = x + (srcX + px) * num / den, x1 = x + (srcX + px + seg) * num / den;
          int y0 = y + py * num / den, y1 = y + (py + 1) * num / den;
          e.fillRect(x0, y0, x1 - x0, y1 - y0, TFT_BLACK);
          p += seg;
          left -= seg;
        }
      }
      pc += run;
    }
    srcX += w;
  }
}

static void drawValueBlock(EPaper& e, int mgdl, bool mmol) {
  Rect r = block(1, 1);
  char buf[16];
  if (mmol) snprintf(buf, sizeof(buf), "%.1f", mgdl / 18.016);
  else snprintf(buf, sizeof(buf), "%d", mgdl);
  // biggest scale that still fits the block: 1.5x, then 1.33x, then 1x
  static const int SCALES[][2] = {{3, 2}, {4, 3}, {1, 1}};
  int num = 1, den = 1;
  for (const auto& sc : SCALES) {
    num = sc[0];
    den = sc[1];
    if (font8Width(buf, num, den) <= r.w - 8) break;
  }
  int w = font8Width(buf, num, den), h = F8_H * num / den;
  const int GAP = 6, LABEL_H = 16;
  int top = r.y + (r.h - h - GAP - LABEL_H) / 2;
  drawFont8Scaled(e, buf, r.x + (r.w - w) / 2, top, num, den);
  drawCentered(e, mmol ? "mmol/L" : "mg/dL", r, top + h + GAP, 2);
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

// Age text under the first column, error text after it.
static void drawStrip(EPaper& e, int mins, const String& error) {
  e.fillRect(STRIP.x, STRIP.y, STRIP.w, STRIP.h, TFT_WHITE);
  e.drawString("Updated " + ageText(mins), 10, 458, 2);
  if (error.length()) e.drawString("! " + error, 276, 458, 2);
}

static void drawReadingScreen(EPaper& e, const Reading& last, const Deltas& d, bool mmol, int mins, const String& error) {
  e.fillScreen(TFT_WHITE);
  drawTopBand(e, last.trend, d, mmol);
  drawValueBlock(e, last.mgdl, mmol);
  drawStrip(e, mins, error);
}

static void partial(EPaper& e, const Rect& r) { e.updataPartial(r.x, r.y, r.w, r.h); }

static void refresh(EPaper& e, const Rect& changed) {
  if (millis() - lastFullMs >= FULL_REFRESH_MS) {
    e.update();
    lastFullMs = millis();
  } else {
    partial(e, changed);
  }
}

// ---------- public ----------

void drawDashboard(EPaper& e, const Reading& last, const Deltas& d, const String& error,
                   bool notConfigured, const String& configUrl) {
  bool mmol = cfgGet("unit") == "mmol";
  Mode mode = notConfigured ? MODE_NOT_CONFIGURED : last.valid ? MODE_READING : MODE_NO_DATA;
  String name = sourceName();
  int mins = last.valid ? ageMinutes(last.when) : -1;

  bool layoutChanged = shown.mode == MODE_NONE || mode != shown.mode;
  bool errorChanged = error != shown.error;

  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);

  if (mode == MODE_READING && !layoutChanged) {
    // A new reading (or unit) redraws every block, captions included, even if a number is unchanged.
    bool all = mmol != shown.mmol || last.when != shown.when;
    Rect dirty = {0, 0, 0, 0};
    if (all || last.trend != shown.trend || d.has1 != shown.deltas.has1 || d.d1 != shown.deltas.d1 ||
        d.has15 != shown.deltas.has15 || d.d15 != shown.deltas.d15) {
      drawTopBand(e, last.trend, d, mmol);  // the band is one piece: redraw it whole
      dirty = unite(dirty, TOP_ROW);
    }
    if (all || last.mgdl != shown.mgdl) {
      clearBlock(e, 1, 1);
      drawValueBlock(e, last.mgdl, mmol);
      dirty = unite(dirty, block(1, 1));
    }
    if (mins != shown.ageMins || errorChanged) {
      drawStrip(e, mins, error);
      dirty = unite(dirty, STRIP);
    }
    if (dirty.w == 0) return;  // panel stays untouched
    refresh(e, dirty);
  } else {
    bool urlChanged = mode == MODE_NOT_CONFIGURED && configUrl != shown.url;
    bool nameChanged = mode != MODE_READING && name != shown.name;
    if (!layoutChanged && !errorChanged && !urlChanged && !nameChanged) return;
    if (mode == MODE_READING) {
      drawReadingScreen(e, last, d, mmol, mins, error);
    } else {
      e.fillScreen(TFT_WHITE);
      e.drawString(name, 20, 20, 4);
      if (mode == MODE_NOT_CONFIGURED) {
        e.drawString("Not configured", 20, 100, 4);
        e.drawString("Open this page to choose your", 20, 160, 2);
        e.drawString("glucose data source:", 20, 190, 2);
        e.drawString(configUrl, 20, 240, 2);
        drawQR(e, 520, 80, 7, configUrl.c_str());
      } else {
        e.drawString("No data yet", 20, 100, 4);
        if (error.length()) e.drawString("! " + error, 20, 440, 2);
      }
    }
    e.update();
    lastFullMs = millis();
  }

  shown.mode = mode;
  shown.name = name;
  shown.url = configUrl;
  shown.error = error;
  shown.mmol = mmol;
  shown.mgdl = last.mgdl;
  shown.trend = last.trend;
  shown.deltas = d;
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
  e.setTextColor(TFT_BLACK);
  e.setTextSize(1);
  drawStrip(e, mins, shown.error);
  refresh(e, STRIP);
}
