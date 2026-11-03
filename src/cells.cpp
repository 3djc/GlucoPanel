#include "cells.h"
#include <math.h>
#include <algorithm>
#include <string.h>
#include "graph.h"
#include "i18n.h"
#include "profile.h"
#include "ui.h"

namespace {

const int SEGMENTS = 5;

void setText(lv_obj_t* label, const char* text) {
  if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}

int lineHeight(const lv_font_t* f) { return lv_font_get_line_height(f); }

// A centred label across the whole block; its vertical position is set by the layout functions below.
lv_obj_t* label(const Cell& c, const lv_font_t* font) { return uiLabel(c.parent, "", font, c.x, c.y, c.w); }

const lv_font_t* fontOf(lv_obj_t* l) { return lv_obj_get_style_text_font(l, LV_PART_MAIN); }

// ---------- vertical centring ----------
// Every block centres its content in its own rectangle, horizontally (the labels are as wide as the block and
// centred) and vertically (from the real size of the glyphs, not of the lines: a line has empty space above and
// below its ink).

struct Ink {
  int top, bottom;  // first and last row of ink, from the top of the label
};

// Ink extent of the glyphs of `ref` in a font. The reference text is fixed per kind of line (digits, a caption with
// a descender...) so the layout does not move when the text changes.
Ink inkOf(const lv_font_t* f, const char* ref) {
  int baseline = lineHeight(f) - f->base_line;  // y of the baseline inside the label
  Ink k = {1000, -1000};
  for (const char* p = ref; *p; p++) {
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(f, &g, (uint8_t)*p, 0)) continue;
    k.top = std::min(k.top, baseline - (g.ofs_y + g.box_h));
    k.bottom = std::max(k.bottom, baseline - g.ofs_y);
  }
  return k;
}

struct Line {
  lv_obj_t* obj;
  Ink ink;
};

// Stacks the lines with `gap` px between their ink and centres the stack vertically in the block.
void centreStack(const Cell& c, const Line* lines, int n, int gap) {
  int total = (n - 1) * gap;
  for (int i = 0; i < n; i++) total += lines[i].ink.bottom - lines[i].ink.top;
  int y = c.y + (c.h - total) / 2;
  for (int i = 0; i < n; i++) {
    int top = y - lines[i].ink.top;
    if (lv_obj_get_style_y(lines[i].obj, LV_PART_MAIN) != top) lv_obj_set_y(lines[i].obj, top);  // no redraw if unchanged
    y += lines[i].ink.bottom - lines[i].ink.top + gap;
  }
}

int gapOf() { return lineHeight(M.fCaption) / 3; }

// The number of a block: a digit's ink; the HbA1c text may wrap on several lines.
Ink numberInk(const Cell& c) {
  const lv_font_t* f = fontOf(c.a);
  Ink k = inkOf(f, "0");
  if (c.type == CELL_HBA1C) {
    lv_point_t size;
    lv_text_get_size(&size, lv_label_get_text(c.a), f, 0, 0, c.w, LV_TEXT_FLAG_NONE);
    k.bottom += (std::max(1, (int)(size.y / lineHeight(f))) - 1) * lineHeight(f);
  }
  return k;
}

// Caption, number and (when there is one) a small line under the number.
void layoutCaptioned(Cell& c) {
  Line lines[3] = {{c.b, inkOf(fontOf(c.b), "Hg")}, {c.a, numberInk(c)}, {nullptr, {0, 0}}};
  int n = 2;
  if (c.c) lines[n++] = {c.c, inkOf(fontOf(c.c), "Hg")};
  centreStack(c, lines, n, gapOf());
}

// The glucose value and its unit.
void layoutValue(Cell& c) {
  Line lines[2] = {{c.a, inkOf(fontOf(c.a), "0")}, {c.b, inkOf(M.fMid, "mg/dL")}};
  centreStack(c, lines, 2, lineHeight(M.fMid) / 2);
}

// The date over the time.
void layoutClock(Cell& c) {
  Line lines[2] = {{c.b, inkOf(M.fMid, "Hg")}, {c.a, inkOf(M.fNumBig, "0")}};
  centreStack(c, lines, 2, gapOf());
}

// ---------- text helpers ----------

// A block's number: the big font when the text fits the block, the regular one otherwise (a wide text such as
// "62 - 241"). `allowBig` is false for blocks with a small line under the number: there is no room for it.
void setNumber(Cell& c, const char* text, bool allowBig = true) {
  const lv_font_t* f = M.fNumBig;
  lv_point_t size;
  lv_text_get_size(&size, text, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  if (!allowBig || size.x > c.w - 16) f = M.fNum;
  if (fontOf(c.a) != f) {
    lv_obj_set_style_text_font(c.a, f, 0);
    layoutCaptioned(c);
  }
  setText(c.a, text);
}

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

// ---------- trend arrow ----------

// Custom-drawn: a shaft and one or two heads, rotated to the trend's direction.
void arrowDrawCb(lv_event_t* ev) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(ev);
  Trend trend = ((Cell*)lv_obj_get_user_data(obj))->trend;
  lv_layer_t* layer = lv_event_get_layer(ev);
  float deg;
  int heads = 1;
  switch (trend) {
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
  const float len = (float)M.arrowLen;
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

// ---------- block builders ----------

// A caption over a big number (the delta, reading time and age blocks), plus a small line under the number
// (c.c) for the blocks that have one.
void buildCaptioned(Cell& c, const char* caption, bool withSubLine = false) {
  c.b = label(c, M.fCaption);
  lv_label_set_text(c.b, caption);
  c.a = label(c, M.fNum);
  if (withSubLine) c.c = label(c, M.fCaption);
  layoutCaptioned(c);
}

void buildValue(Cell& c) {
  c.a = label(c, c.h >= M.tallH ? M.fBigXL : M.fBigM);
  c.b = label(c, M.fMid);
  layoutValue(c);
}

void buildArrow(Cell& c) {
  const int BOX = M.arrowBox;
  c.a = lv_obj_create(c.parent);
  lv_obj_set_size(c.a, BOX, BOX);
  lv_obj_set_pos(c.a, c.x + (c.w - BOX) / 2, c.y + (c.h - BOX) / 2);
  lv_obj_set_style_bg_opa(c.a, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c.a, 0, 0);
  lv_obj_remove_flag(c.a, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_user_data(c.a, &c);
  lv_obj_add_event_cb(c.a, arrowDrawCb, LV_EVENT_DRAW_MAIN, nullptr);
  c.b = label(c, M.fNum);  // "?" while the trend is unknown
  lv_label_set_text(c.b, "?");
  Ink k = inkOf(M.fNum, "?");
  lv_obj_set_y(c.b, c.y + c.h / 2 - (k.top + k.bottom) / 2);
}

void buildClock(Cell& c) {
  c.a = label(c, M.fNumBig);
  c.b = label(c, M.fMid);
  layoutClock(c);
}

// SEGMENTS stacked segments filling from the bottom, one per minute of age.
void buildFresh(Cell& c) {
  const int barH = std::min(c.h >= M.tallH ? M.barTall : M.barShort, c.h), segW = M.segW, GAP = M.segGap;
  const int segH = (barH - (SEGMENTS - 1) * GAP) / SEGMENTS;
  int top = c.y + (c.h - barH) / 2;
  for (int i = 0; i < SEGMENTS; i++) {  // seg[0] is the bottom one
    lv_obj_t* sg = lv_obj_create(c.parent);
    lv_obj_set_pos(sg, c.x + (c.w - segW) / 2, top + (SEGMENTS - 1 - i) * (segH + GAP));
    lv_obj_set_size(sg, segW, segH);
    lv_obj_set_style_bg_color(sg, lv_color_black(), 0);
    lv_obj_set_style_border_color(sg, lv_color_black(), 0);
    lv_obj_set_style_border_width(sg, M.segBorder, 0);  // empty segments stay visible as outlines
    lv_obj_set_style_radius(sg, M.segRadius, 0);
    lv_obj_remove_flag(sg, LV_OBJ_FLAG_SCROLLABLE);
    c.seg[i] = sg;
  }
}

// ---------- block updaters ----------

void updateValue(Cell& c, const CellView& v) {
  char buf[16];
  if (v.mmol) snprintf(buf, sizeof(buf), "%.1f", v.last.mgdl / 18.016);
  else snprintf(buf, sizeof(buf), "%d", v.last.mgdl);
  // biggest font that still fits the block
  const lv_font_t* candidates[] = {M.fBigXL, M.fBigL, M.fBigM, M.fNum};
  bool tall = c.h >= M.tallH;
  const lv_font_t* f = candidates[tall ? 0 : 2];
  for (int i = tall ? 0 : 2; i < 4; i++) {
    f = candidates[i];
    lv_point_t size;
    lv_text_get_size(&size, buf, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= c.w - 8) break;
  }
  if (fontOf(c.a) != f) {
    lv_obj_set_style_text_font(c.a, f, 0);
    layoutValue(c);
  }
  setText(c.a, buf);
  setText(c.b, v.mmol ? "mmol/L" : "mg/dL");
}

void updateArrow(Cell& c, const CellView& v) {
  if (c.initDone && c.trend == v.last.trend) return;
  c.trend = v.last.trend;
  lv_obj_invalidate(c.a);
  if (c.trend == TREND_UNKNOWN) lv_obj_remove_flag(c.b, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(c.b, LV_OBJ_FLAG_HIDDEN);
}

void updateClock(Cell& c) {
  uiApplyTimeZone();
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  setText(c.a, buf);
  setText(c.b, i18nDate(t).c_str());
}

void updateReadTime(Cell& c, const CellView& v) {
  uiApplyTimeZone();
  struct tm t;
  localtime_r(&v.last.when, &t);
  char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  setNumber(c, buf);
}

void updateAge(Cell& c, const CellView& v) {
  int m = ageMinutes(v.last.when);
  char buf[16];
  if (m < 60) snprintf(buf, sizeof(buf), T("%d min"), m);
  else snprintf(buf, sizeof(buf), "%dh%02d", m / 60, m % 60);
  setNumber(c, buf);
}

void updateFresh(Cell& c, const CellView& v) {
  int mins = ageMinutes(v.last.when), level = mins > SEGMENTS ? SEGMENTS : mins;  // a 3 min old reading shows 3
  for (int i = 0; i < SEGMENTS; i++) {
    lv_opa_t want = i < level ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (lv_obj_get_style_bg_opa(c.seg[i], LV_PART_MAIN) != want) lv_obj_set_style_bg_opa(c.seg[i], want, 0);
  }
}

// ---------- statistics blocks ----------

String mgdlText(float mgdl, bool mmol) {
  char buf[16];
  if (mmol) snprintf(buf, sizeof(buf), "%.1f", mgdl / 18.016f);
  else snprintf(buf, sizeof(buf), "%d", (int)lroundf(mgdl));
  return buf;
}

void updateStat(Cell& c, const CellView& v) {
  Stats st = glucoseStats(v.low, v.high);
  const char* name = T(c.type == CELL_AVG ? "average" : c.type == CELL_TIR ? "in range" : c.type == CELL_RANGE ? "lowest / highest" : "variability");
  String caption = name;
  String number = "--", sub;
  if (st.ok) {
    caption += " " + String(st.hours) + " h";
    char buf[48];
    switch (c.type) {
      case CELL_AVG: number = mgdlText(st.avg, v.mmol); break;
      case CELL_TIR:
        snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(st.inRangePct));
        number = buf;
        snprintf(buf, sizeof(buf), "%s %d%%   %s %d%%", T("low"), (int)lroundf(st.belowPct), T("high"), (int)lroundf(st.abovePct));
        sub = buf;
        break;
      case CELL_RANGE: number = mgdlText(st.minMgdl, v.mmol) + " - " + mgdlText(st.maxMgdl, v.mmol); break;
      default:  // CELL_CV
        snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(100.0f * st.sd / (st.avg > 0 ? st.avg : 1)));
        number = buf;
        sub = String(T("SD")) + " " + mgdlText(st.sd, v.mmol);
    }
  }
  setText(c.b, caption.c_str());
  setNumber(c, number.c_str(), sub.length() == 0);
  if (c.c) setText(c.c, sub.c_str());
}

void updateHba1c(Cell& c, const CellView& v) {
  Hba1c h = glucoseHba1c();
  char buf[64];
  setText(c.b, T("estimated HbA1c"));
  if (h.ok) {
    char over[24];
    snprintf(over, sizeof(over), T("over %d days"), HBA1C_DAYS);
    if (v.hba1cMmolMol) {  // the unit goes in the small line: it does not fit next to the big number
      snprintf(buf, sizeof(buf), "%d", (int)lroundf(h.mmolMol));
      snprintf(buf + 24, 40, "mmol/mol, %s", over);
    } else {
      snprintf(buf, sizeof(buf), "%.1f%%", h.percent);
      snprintf(buf + 24, 40, "%s", over);
    }
  } else {
    snprintf(buf, sizeof(buf), "%s", T("Not available yet"));
    snprintf(buf + 24, 40, T("%.1f / %d days"), h.coveredDays, HBA1C_DAYS);
  }
  // the number is big; the "not available" text is a smaller, wrapping one
  const lv_font_t* f = h.ok ? M.fNum : M.fMid;
  if (fontOf(c.a) != f) lv_obj_set_style_text_font(c.a, f, LV_PART_MAIN);
  setText(c.a, buf);
  setText(c.c, buf + 24);
  layoutCaptioned(c);  // the text may wrap on two lines
}

}  // namespace

CellType cellTypeFromString(const String& s) {
  static const struct { const char* name; CellType type; } table[] = {
      {"value", CELL_VALUE}, {"arrow", CELL_ARROW}, {"delta1", CELL_DELTA1}, {"delta15", CELL_DELTA15}, {"delta30", CELL_DELTA30},
      {"clock", CELL_CLOCK}, {"readtime", CELL_READTIME}, {"age", CELL_AGE}, {"fresh", CELL_FRESH},
      {"avg", CELL_AVG}, {"tir", CELL_TIR}, {"range", CELL_RANGE}, {"cv", CELL_CV}, {"hba1c", CELL_HBA1C}, {"graph", CELL_GRAPH}};
  for (const auto& e : table)
    if (s == e.name) return e.type;
  return CELL_NONE;
}

void cellBuild(Cell& c) {
  c.initDone = false;
  c.stamp = 0;
  c.trend = TREND_UNKNOWN;
  switch (c.type) {
    case CELL_VALUE: buildValue(c); break;
    case CELL_ARROW: buildArrow(c); break;
    case CELL_DELTA1: buildCaptioned(c, T("vs last")); break;
    case CELL_DELTA15: buildCaptioned(c, T("vs 15 min")); break;
    case CELL_DELTA30: buildCaptioned(c, T("vs 30 min")); break;
    case CELL_CLOCK: buildClock(c); break;
    case CELL_READTIME: buildCaptioned(c, T("reading time")); break;
    case CELL_AGE: buildCaptioned(c, T("reading age")); break;
    case CELL_FRESH: buildFresh(c); break;
    case CELL_AVG: buildCaptioned(c, ""); break;
    case CELL_TIR: buildCaptioned(c, "", true); break;
    case CELL_RANGE: buildCaptioned(c, ""); break;
    case CELL_CV: buildCaptioned(c, "", true); break;
    case CELL_HBA1C:
      buildCaptioned(c, "", true);
      lv_label_set_long_mode(c.a, LV_LABEL_LONG_WRAP);  // "Not available yet" may need two lines
      break;
    case CELL_GRAPH: c.a = graphCreate(c.parent, c.x, c.y, c.w, c.h); break;
    default: break;
  }
}

void cellUpdate(Cell& c, const CellView& v) {
  switch (c.type) {
    case CELL_VALUE: updateValue(c, v); break;
    case CELL_ARROW: updateArrow(c, v); break;
    case CELL_DELTA1: setNumber(c, deltaText(v.deltas.has1, v.deltas.d1, v.mmol).c_str()); break;
    case CELL_DELTA15: setNumber(c, deltaText(v.deltas.has15, v.deltas.d15, v.mmol).c_str()); break;
    case CELL_DELTA30: setNumber(c, deltaText(v.deltas.has30, v.deltas.d30, v.mmol).c_str()); break;
    case CELL_CLOCK: updateClock(c); break;
    case CELL_READTIME: updateReadTime(c, v); break;
    case CELL_AGE: updateAge(c, v); break;
    case CELL_FRESH: updateFresh(c, v); break;
    case CELL_AVG:
    case CELL_TIR:
    case CELL_RANGE:
    case CELL_CV: updateStat(c, v); break;
    case CELL_HBA1C: updateHba1c(c, v); break;
    case CELL_GRAPH: {
      long stamp = (long)v.last.when * 2 + (v.mmol ? 1 : 0);  // the graph only changes with a new reading or unit
      if (stamp != c.stamp) {
        c.stamp = stamp;
        graphUpdate(c.a, v.last.when, v.mmol);
      }
      break;
    }
    default: break;
  }
  c.initDone = true;
}
