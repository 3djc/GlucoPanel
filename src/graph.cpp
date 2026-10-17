#include "graph.h"
#include <math.h>
#include "config.h"
#include "glucose.h"
#include "ui.h"

namespace {

const int GRAPH_MAX = GRAPH_HOURS * 60 + 60;
const time_t WINDOW = GRAPH_HOURS * 3600;
const time_t MAX_GAP = 20 * 60;  // readings further apart than this are not joined by a line

// Margins inside the widget: y labels on the left, hour labels at the bottom.
const int LEFT = 56, RIGHT = 14, TOP = 10, BOTTOM = 26;

struct State {
  Reading pts[GRAPH_MAX];
  int n = 0;
  time_t end = 0;
  int low = 70, high = 180;
  bool mmol = false;
} g;

int roundDown(int v, int step) { return (int)floorf((float)v / step) * step; }
int roundUp(int v, int step) { return (int)ceilf((float)v / step) * step; }

void drawLabel(lv_layer_t* layer, const char* text, const lv_area_t& area, lv_text_align_t align) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = text;
  d.text_local = 1;  // drawing is deferred: LVGL keeps its own copy of the text
  d.font = &lv_font_montserrat_14;
  d.color = lv_color_black();
  d.align = align;
  lv_draw_label(layer, &d, &area);
}

void drawLine(lv_layer_t* layer, int x1, int y1, int x2, int y2, int width, int dash = 0, bool round = false) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1 = {(lv_value_precise_t)x1, (lv_value_precise_t)y1};
  d.p2 = {(lv_value_precise_t)x2, (lv_value_precise_t)y2};
  d.color = lv_color_black();
  d.width = width;
  d.opa = LV_OPA_COVER;
  d.dash_width = dash;
  d.dash_gap = dash;
  d.round_start = d.round_end = round;
  lv_draw_line(layer, &d);
}

void drawDot(lv_layer_t* layer, int cx, int cy, int r) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_black();
  d.bg_opa = LV_OPA_COVER;
  d.radius = LV_RADIUS_CIRCLE;
  lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
  lv_draw_rect(layer, &d, &a);
}

void valueText(char* buf, size_t n, int mgdl) {
  if (g.mmol) snprintf(buf, n, "%.1f", mgdl / 18.016);
  else snprintf(buf, n, "%d", mgdl);
}

void drawCb(lv_event_t* ev) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(ev);
  lv_layer_t* layer = lv_event_get_layer(ev);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  const int px = a.x1 + LEFT, py = a.y1 + TOP;
  const int pw = lv_area_get_width(&a) - LEFT - RIGHT, ph = lv_area_get_height(&a) - TOP - BOTTOM;
  const time_t t0 = g.end - WINDOW;

  // y range: the data and the limits with some air, never tighter than 60..200 mg/dL
  int lo = g.low, hi = g.high;
  for (int i = 0; i < g.n; i++) {
    if (g.pts[i].when < t0) continue;
    lo = g.pts[i].mgdl < lo ? g.pts[i].mgdl : lo;
    hi = g.pts[i].mgdl > hi ? g.pts[i].mgdl : hi;
  }
  const int ymin = roundDown(lo - 10, 10) < 60 ? roundDown(lo - 10, 10) : 60;
  const int ymax = roundUp(hi + 10, 10) > 200 ? roundUp(hi + 10, 10) : 200;
  auto X = [&](time_t t) { return px + (int)((long)(t - t0) * pw / WINDOW); };
  auto Y = [&](int v) { return py + ph - (v - ymin) * ph / (ymax - ymin); };

  // target range: light-grey band with dashed limits
  lv_draw_rect_dsc_t band;
  lv_draw_rect_dsc_init(&band);
  band.bg_color = UI_GREY;
  band.bg_opa = LV_OPA_COVER;
  lv_area_t ba = {px, Y(g.high), px + pw, Y(g.low)};
  lv_draw_rect(layer, &band, &ba);
  drawLine(layer, px, Y(g.high), px + pw, Y(g.high), 1, 4);
  drawLine(layer, px, Y(g.low), px + pw, Y(g.low), 1, 4);

  // axes
  drawLine(layer, px, py, px, py + ph, 2);
  drawLine(layer, px, py + ph, px + pw, py + ph, 2);

  // limit labels
  char buf[16];
  for (int k = 0; k < 2; k++) {
    int v = k ? g.high : g.low, y = Y(v);
    valueText(buf, sizeof(buf), v);
    lv_area_t la = {a.x1, y - 9, a.x1 + LEFT - 8, y + 9};
    drawLabel(layer, buf, la, LV_TEXT_ALIGN_RIGHT);
  }

  // hour ticks and labels (local time)
  uiApplyTimeZone();
  for (time_t t = (t0 / 60 + 1) * 60; t <= g.end; t += 60) {
    struct tm lt;
    localtime_r(&t, &lt);
    if (lt.tm_min != 0) continue;
    int x = X(t);
    drawLine(layer, x, py + ph, x, py + ph + 6, 2);
    snprintf(buf, sizeof(buf), "%02d:00", lt.tm_hour);
    lv_area_t la = {x - 30, py + ph + 7, x + 30, py + ph + BOTTOM};
    drawLabel(layer, buf, la, LV_TEXT_ALIGN_CENTER);
  }

  // readings: a line between neighbours, a dot where a reading has no neighbour
  const Reading* prev = nullptr;
  bool prevJoined = false;
  for (int i = 0; i < g.n; i++) {
    const Reading& r = g.pts[i];
    if (r.when < t0 || r.when > g.end) continue;
    if (prev && r.when - prev->when < 90) continue;  // Dexcom repeats a measurement a few seconds apart
    bool joined = prev && r.when - prev->when <= MAX_GAP;
    if (joined) drawLine(layer, X(prev->when), Y(prev->mgdl), X(r.when), Y(r.mgdl), 4, 0, true);
    else if (prev && !prevJoined) drawDot(layer, X(prev->when), Y(prev->mgdl), 3);
    prevJoined = joined;
    prev = &r;
  }
  if (prev) drawDot(layer, X(prev->when), Y(prev->mgdl), 7);  // newest reading, emphasised
}

}  // namespace

lv_obj_t* graphCreate(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(o, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  return o;
}

void graphUpdate(lv_obj_t* graph, time_t end, bool mmol) {
  g.n = glucoseHistory(g.pts, GRAPH_MAX);
  g.end = end;
  g.mmol = mmol;
  g.low = atoi(cfgGet("low").c_str());
  g.high = atoi(cfgGet("high").c_str());
  lv_obj_invalidate(graph);
}
