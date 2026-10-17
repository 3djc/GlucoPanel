#include "display.h"
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "graph.h"
#include "lvgl_port.h"
#include "ui.h"

namespace {

const int SCREEN_W = 800;
const int COLS = 3;

enum Mode { MODE_NONE, MODE_NOT_CONFIGURED, MODE_NO_DATA, MODE_READING };

// Top band (like Gluco-Monitor's): rounded, light grey, three cells.
const int BAND_X = 6, BAND_Y = 10, BAND_W = 788, BAND_H = 130, BAND_RADIUS = 10;
const int CELL_W = BAND_W / 3;
const int GRAPH_Y = 322, GRAPH_H = 136;  // third row: the graph spans the full width
const int STRIP_Y = 458;                 // error text under the graph

// Freshness bar right of the value: SEGMENTS stacked segments, one more filled per minute of age.
const int SEGMENTS = 5;

// What is currently on the panel.
struct Shown {
  Mode mode = MODE_NONE;
  String name, url, error;
  bool mmol = false;
  int mgdl = -1;
  Trend trend = TREND_UNKNOWN;
  Deltas deltas;
  int ageMins = -1;  // freshness level (see ageLevel)
  time_t when = 0;
  long clockMin = -1;  // minute (since epoch) of the clock label
} shown;

// Widgets of the reading screen, valid while shown.mode == MODE_READING.
struct Widgets {
  lv_obj_t *band, *arrow, *arrowUnknown, *delta1, *delta15, *value, *unit, *date, *time, *graph, *error;
  lv_obj_t* seg[SEGMENTS];  // freshness bar
} w;
Trend arrowTrend = TREND_UNKNOWN;

// ---------- trend arrow ----------

// Custom-drawn: a shaft and one or two heads, rotated to the trend's direction.
void arrowDrawCb(lv_event_t* ev) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(ev);
  lv_layer_t* layer = lv_event_get_layer(ev);
  float deg;
  int heads = 1;
  switch (arrowTrend) {
    case TREND_DOUBLE_UP: heads = 2; deg = -90; break;
    case TREND_SINGLE_UP: deg = -90; break;
    case TREND_45_UP: deg = -45; break;
    case TREND_FLAT: deg = 0; break;
    case TREND_45_DOWN: deg = 45; break;
    case TREND_SINGLE_DOWN: deg = 90; break;
    case TREND_DOUBLE_DOWN: heads = 2; deg = 90; break;
    default: return;  // unknown: a "?" label is shown instead
  }
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  const float len = 90;
  float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f;
  float r = deg * (float)M_PI / 180.0f, dx = cosf(r), dy = sinf(r), px = -dy, py = dx;
  float hl = len * 0.35f, hw = len * 0.28f, sw = len * 0.06f;
  float tx = cx - dx * len / 2, ty = cy - dy * len / 2;  // tail
  float ex = cx + dx * len / 2, ey = cy + dy * len / 2;  // tip
  float bx = ex - dx * hl, by = ey - dy * hl;            // base of the first head

  auto tri = [&](float x0, float y0, float x1, float y1, float x2, float y2) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = lv_color_black();
    d.opa = LV_OPA_COVER;
    // lv_value_precise_t is an int here (LV_USE_FLOAT is off)
    d.p[0] = {(lv_value_precise_t)lroundf(x0), (lv_value_precise_t)lroundf(y0)};
    d.p[1] = {(lv_value_precise_t)lroundf(x1), (lv_value_precise_t)lroundf(y1)};
    d.p[2] = {(lv_value_precise_t)lroundf(x2), (lv_value_precise_t)lroundf(y2)};
    lv_draw_triangle(layer, &d);
  };
  // shaft = two triangles
  tri(tx + px * sw, ty + py * sw, tx - px * sw, ty - py * sw, bx + px * sw, by + py * sw);
  tri(tx - px * sw, ty - py * sw, bx + px * sw, by + py * sw, bx - px * sw, by - py * sw);
  for (int h = 0; h < heads; h++) {
    float sx = ex - dx * hl * 0.9f * h, sy = ey - dy * hl * 0.9f * h;  // second head sits behind the first
    tri(sx, sy, sx - dx * hl + px * hw, sy - dy * hl + py * hw, sx - dx * hl - px * hw, sy - dy * hl - py * hw);
  }
}

void setArrow(Trend t) {
  arrowTrend = t;
  lv_obj_invalidate(w.arrow);
  if (t == TREND_UNKNOWN) lv_obj_remove_flag(w.arrowUnknown, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(w.arrowUnknown, LV_OBJ_FLAG_HIDDEN);
}

// ---------- text ----------

// "+12" / "-3" / "0" ("+0.6" in mmol/L), "--" while the history is too short.
String deltaText(bool has, int delta, bool mmol) {
  if (!has) return "--";
  char buf[16];
  if (mmol) {
    float v = delta / 18.016f;
    if (lroundf(v * 10) == 0) return "0.0";
    snprintf(buf, sizeof(buf), "%+.1f", v);
  } else {
    snprintf(buf, sizeof(buf), delta == 0 ? "%d" : "%+d", delta);
  }
  return buf;
}

int ageMinutes(time_t when) {
  long mins = (long)((time(nullptr) - when) / 60);
  return mins < 0 ? 0 : (int)mins;
}

// One filled segment per minute of age, up to SEGMENTS (a 3 min old reading shows 3).
int ageLevel(int mins) { return mins > SEGMENTS ? SEGMENTS : mins; }

void setStrip(int level, const String& error) {
  for (int i = 0; i < SEGMENTS; i++)
    lv_obj_set_style_bg_opa(w.seg[i], i < level ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  String text = error.length() ? "! " + error : String();
  if (text != lv_label_get_text(w.error)) lv_label_set_text(w.error, text.c_str());  // keep the refresh window small
}

// ---------- reading screen ----------

void setValue(int mgdl, bool mmol) {
  char buf[16];
  if (mmol) snprintf(buf, sizeof(buf), "%.1f", mgdl / 18.016);
  else snprintf(buf, sizeof(buf), "%d", mgdl);
  // biggest font that still fits the middle block
  const lv_font_t* f = &font_big_128;
  lv_point_t size;
  lv_text_get_size(&size, buf, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  if (size.x > SCREEN_W / COLS - 8) f = &font_big_96;
  lv_obj_set_style_text_font(w.value, f, 0);
  lv_label_set_text(w.value, buf);
  lv_label_set_text(w.unit, mmol ? "mmol/L" : "mg/dL");
}

// Current local time (HH:MM) and date (DD Mon), in the time zone chosen in the settings.
void setClock() {
  time_t now = time(nullptr);
  shown.clockMin = now / 60;
  uiApplyTimeZone();
  struct tm t;
  localtime_r(&now, &t);
  // a label is only touched when its text changes, so the date refreshes once a day, not every minute
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  if (strcmp(buf, lv_label_get_text(w.time))) lv_label_set_text(w.time, buf);
  strftime(buf, sizeof(buf), "%d %b", &t);
  if (strcmp(buf, lv_label_get_text(w.date))) lv_label_set_text(w.date, buf);
}

void setDeltas(const Deltas& d, bool mmol) {
  lv_label_set_text(w.delta1, deltaText(d.has1, d.d1, mmol).c_str());
  lv_label_set_text(w.delta15, deltaText(d.has15, d.d15, mmol).c_str());
}

void buildReadingScreen() {
  lv_obj_t* scr = uiNewScreen();

  w.band = lv_obj_create(scr);
  lv_obj_set_pos(w.band, BAND_X, BAND_Y);
  lv_obj_set_size(w.band, BAND_W, BAND_H);
  lv_obj_set_style_bg_color(w.band, UI_GREY, 0);
  lv_obj_set_style_bg_opa(w.band, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(w.band, BAND_RADIUS, 0);
  lv_obj_set_style_border_width(w.band, 0, 0);
  lv_obj_set_style_pad_all(w.band, 0, 0);
  lv_obj_remove_flag(w.band, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 1; i < 3; i++) {  // separators
    lv_obj_t* sep = lv_obj_create(w.band);
    lv_obj_set_pos(sep, i * CELL_W, 14);
    lv_obj_set_size(sep, 1, BAND_H - 28);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
  }

  const int ARROW_BOX = 120;
  w.arrow = lv_obj_create(w.band);
  lv_obj_set_size(w.arrow, ARROW_BOX, ARROW_BOX);
  lv_obj_set_pos(w.arrow, (CELL_W - ARROW_BOX) / 2, (BAND_H - ARROW_BOX) / 2);
  lv_obj_set_style_bg_opa(w.arrow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(w.arrow, 0, 0);
  lv_obj_remove_flag(w.arrow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(w.arrow, arrowDrawCb, LV_EVENT_DRAW_MAIN, nullptr);
  w.arrowUnknown = uiLabel(w.band, "?", &lv_font_montserrat_48, 0, 38, CELL_W);

  uiLabel(w.band, "vs last", &lv_font_montserrat_16, CELL_W, 16, CELL_W);
  uiLabel(w.band, "vs 15 min", &lv_font_montserrat_16, 2 * CELL_W, 16, CELL_W);
  w.delta1 = uiLabel(w.band, "", &lv_font_montserrat_48, CELL_W, 44, CELL_W);
  w.delta15 = uiLabel(w.band, "", &lv_font_montserrat_48, 2 * CELL_W, 44, CELL_W);

  // date and clock, in the block left of the value, vertically centred on the digits
  // Measured on the rendered screen: the value + unit ink spans y=187..327 (centre 257); this offset puts
  // the date + time ink (about 13 px above DIGITS_MID) on that centre.
  const int DIGITS_MID = 270;
  const int timeTop = DIGITS_MID - lv_font_get_line_height(&lv_font_montserrat_48) / 2;
  w.time = uiLabel(scr, "", &lv_font_montserrat_48, 0, timeTop, SCREEN_W / COLS);
  w.date = uiLabel(scr, "", &lv_font_montserrat_24, 0, timeTop - lv_font_get_line_height(&lv_font_montserrat_24) + 6,
                   SCREEN_W / COLS);

  w.value = uiLabel(scr, "", &font_big_128, 0, 0, SCREEN_W);
  lv_obj_align(w.value, LV_ALIGN_CENTER, 0, -8);
  w.unit = uiLabel(scr, "", &lv_font_montserrat_24, 0, 0, SCREEN_W);
  lv_obj_align(w.unit, LV_ALIGN_CENTER, 0, 74);

  // freshness bar: SEGMENTS stacked segments filling from the bottom, one per minute of age, centred
  // in the block right of the value and spanning the same height as the digits plus the unit label
  // (measured on the rendered screen: top of the digits to bottom of the unit text)
  const int BAR_TOP = 187, BAR_H = 140;
  const int segW = 98, GAP = 5, segH = (BAR_H - (SEGMENTS - 1) * GAP) / SEGMENTS;
  for (int i = 0; i < SEGMENTS; i++) {  // seg[0] is the bottom one
    lv_obj_t* sg = lv_obj_create(scr);
    lv_obj_set_pos(sg, SCREEN_W * 5 / 6 - segW / 2, BAR_TOP + (SEGMENTS - 1 - i) * (segH + GAP));
    lv_obj_set_size(sg, segW, segH);
    lv_obj_set_style_bg_color(sg, lv_color_black(), 0);
    lv_obj_set_style_border_color(sg, lv_color_black(), 0);
    lv_obj_set_style_border_width(sg, 3, 0);  // empty segments stay visible as outlines
    lv_obj_set_style_radius(sg, 5, 0);
    lv_obj_remove_flag(sg, LV_OBJ_FLAG_SCROLLABLE);
    w.seg[i] = sg;
  }

  w.graph = graphCreate(scr, 0, GRAPH_Y, SCREEN_W, GRAPH_H);

  w.error = uiLabel(scr, "", &lv_font_montserrat_16, 10, STRIP_Y);
  lv_label_set_long_mode(w.error, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(w.error, SCREEN_W - 20);
}

}  // namespace

// ---------- public ----------

void drawDashboard(const Reading& last, const Deltas& d, const String& error,
                   bool notConfigured, const String& configUrl) {
  bool mmol = cfgGet("unit") == "mmol";
  Mode mode = notConfigured ? MODE_NOT_CONFIGURED : last.valid ? MODE_READING : MODE_NO_DATA;
  String name = sourceName();
  int mins = last.valid ? ageLevel(ageMinutes(last.when)) : -1;  // freshness level, not minutes

  bool layoutChanged = shown.mode == MODE_NONE || mode != shown.mode;
  bool errorChanged = error != shown.error;

  if (mode == MODE_READING && !layoutChanged) {
    // A new reading (or unit) updates every block, even if a number is unchanged.
    bool all = mmol != shown.mmol || last.when != shown.when;
    bool changed = false;
    if (all || last.trend != shown.trend || d.has1 != shown.deltas.has1 || d.d1 != shown.deltas.d1 ||
        d.has15 != shown.deltas.has15 || d.d15 != shown.deltas.d15) {
      setArrow(last.trend);
      setDeltas(d, mmol);
      changed = true;
    }
    if (all || last.mgdl != shown.mgdl) {
      setValue(last.mgdl, mmol);
      changed = true;
    }
    if (all) {
      graphUpdate(w.graph, last.when, mmol);
      changed = true;
    }
    if (mins != shown.ageMins || errorChanged) {
      setStrip(mins, error);
      changed = true;
    }
    if (!changed) return;  // panel stays untouched
    portRefresh(false);
  } else {
    bool urlChanged = mode == MODE_NOT_CONFIGURED && configUrl != shown.url;
    bool nameChanged = mode != MODE_READING && name != shown.name;
    if (!layoutChanged && !errorChanged && !urlChanged && !nameChanged) return;
    if (mode == MODE_READING) {
      buildReadingScreen();
      setArrow(last.trend);
      setDeltas(d, mmol);
      setValue(last.mgdl, mmol);
      setClock();
      setStrip(mins, error);
      graphUpdate(w.graph, last.when, mmol);
    } else {
      lv_obj_t* s = uiNewScreen();
      uiLabel(s, name.c_str(), &lv_font_montserrat_32, 20, 20);
      if (mode == MODE_NOT_CONFIGURED) {
        uiLabel(s, "Not configured", &lv_font_montserrat_32, 20, 100);
        uiLabel(s, "Open this page to choose your", &lv_font_montserrat_16, 20, 160);
        uiLabel(s, "glucose data source:", &lv_font_montserrat_16, 20, 190);
        uiLabel(s, configUrl.c_str(), &lv_font_montserrat_24, 20, 240);
        uiQR(s, 500, 60, 280, configUrl.c_str());
      } else {
        uiLabel(s, "No data yet", &lv_font_montserrat_32, 20, 100);
        if (error.length()) uiLabel(s, ("! " + error).c_str(), &lv_font_montserrat_16, 20, 440);
      }
    }
    portRefresh(true);
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

void dashboardTick() {
  if (shown.mode != MODE_READING) return;
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  bool changed = false;
  int mins = ageLevel(ageMinutes(shown.when));
  if (mins != shown.ageMins) {
    shown.ageMins = mins;
    setStrip(mins, shown.error);
    changed = true;
  }
  if (time(nullptr) / 60 != shown.clockMin) {  // the clock label ticks once a minute
    setClock();
    changed = true;
  }
  if (changed) portRefresh(false);
}
