#include "cells.h"
#include <math.h>
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

// Vertical centre of the block's small content.
int centreY(const Cell& c) { return c.y + c.h / 2 + (c.middleRow ? M.valueShift : 0); }

lv_obj_t* label(const Cell& c, const lv_font_t* font, int top) { return uiLabel(c.parent, "", font, c.x, top, c.w); }

// ---------- text helpers ----------

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

// A caption over a big number (the delta, reading time and age blocks).
void buildCaptioned(Cell& c, const char* caption) {
  int cy = centreY(c);
  c.b = label(c, M.fCaption, cy + M.capDy);
  lv_label_set_text(c.b, caption);
  c.a = label(c, M.fNum, cy + M.numDy);
}

// Like buildCaptioned, plus a small line under the number (c.c).
void buildStat(Cell& c, const char* caption) {
  buildCaptioned(c, caption);
  c.c = label(c, M.fCaption, centreY(c) + M.subDy);
}

void buildValue(Cell& c) {
  bool tall = c.h >= M.tallH;
  int mid = c.y + c.h / 2;
  const lv_font_t* big = tall ? M.fBigL : M.fBigM;
  c.a = label(c, big, 0);
  lv_obj_set_y(c.a, mid + (tall ? M.digitsTall : M.digitsShort) - lineHeight(big) / 2);
  c.b = label(c, M.fMid, 0);
  lv_obj_set_y(c.b, mid + (tall ? M.unitTall : M.unitShort) - lineHeight(M.fMid) / 2);
}

void buildArrow(Cell& c) {
  const int BOX = M.arrowBox;
  c.a = lv_obj_create(c.parent);
  lv_obj_set_size(c.a, BOX, BOX);
  lv_obj_set_pos(c.a, c.x + (c.w - BOX) / 2, centreY(c) - BOX / 2);
  lv_obj_set_style_bg_opa(c.a, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c.a, 0, 0);
  lv_obj_remove_flag(c.a, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_user_data(c.a, &c);
  lv_obj_add_event_cb(c.a, arrowDrawCb, LV_EVENT_DRAW_MAIN, nullptr);
  c.b = uiLabel(c.parent, "?", M.fNum, c.x, centreY(c) - lineHeight(M.fNum) / 2, c.w);
}

void buildClock(Cell& c) {
  // Measured: the date + time ink is centred 13 px above the time label's centre.
  int timeTop = centreY(c) + M.clockDy - lineHeight(M.fNum) / 2;
  c.a = label(c, M.fNum, timeTop);
  c.b = label(c, M.fMid, timeTop - lineHeight(M.fMid) + M.dateGap);
}

// SEGMENTS stacked segments filling from the bottom, one per minute of age.
void buildFresh(Cell& c) {
  const int barH = c.h >= M.tallH ? M.barTall : M.barShort, segW = M.segW, GAP = M.segGap;
  const int segH = (barH - (SEGMENTS - 1) * GAP) / SEGMENTS;
  int top = centreY(c) - barH / 2;
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
  const lv_font_t* candidates[] = {M.fBigL, M.fBigM, M.fNum};
  bool tall = c.h >= M.tallH;
  const lv_font_t* f = candidates[tall ? 0 : 1];
  for (int i = tall ? 0 : 1; i < 3; i++) {
    f = candidates[i];
    lv_point_t size;
    lv_text_get_size(&size, buf, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= c.w - 8) break;
  }
  if (lv_obj_get_style_text_font(c.a, LV_PART_MAIN) != f) lv_obj_set_style_text_font(c.a, f, 0);
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
  setText(c.a, buf);
}

void updateAge(Cell& c, const CellView& v) {
  int m = ageMinutes(v.last.when);
  char buf[16];
  if (m < 60) snprintf(buf, sizeof(buf), T("%d min"), m);
  else snprintf(buf, sizeof(buf), "%dh%02d", m / 60, m % 60);
  setText(c.a, buf);
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
  setText(c.a, number.c_str());
  setText(c.c, sub.c_str());
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
  if (lv_obj_get_style_text_font(c.a, LV_PART_MAIN) != f) {
    lv_obj_set_style_text_font(c.a, f, LV_PART_MAIN);
    lv_obj_set_y(c.a, centreY(c) - (h.ok ? M.hbaNum : M.hbaNoData));
  }
  setText(c.a, buf);
  setText(c.c, buf + 24);
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
    case CELL_AVG: buildStat(c, ""); break;
    case CELL_TIR: buildStat(c, ""); break;
    case CELL_RANGE: buildStat(c, ""); break;
    case CELL_CV: buildStat(c, ""); break;
    case CELL_HBA1C:
      buildStat(c, "");
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
    case CELL_DELTA1: setText(c.a, deltaText(v.deltas.has1, v.deltas.d1, v.mmol).c_str()); break;
    case CELL_DELTA15: setText(c.a, deltaText(v.deltas.has15, v.deltas.d15, v.mmol).c_str()); break;
    case CELL_DELTA30: setText(c.a, deltaText(v.deltas.has30, v.deltas.d30, v.mmol).c_str()); break;
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
