#include "display.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "cells.h"
#include "battery.h"
#include "config.h"
#include "i18n.h"
#include "lvgl_port.h"
#include "night.h"
#include "profile.h"
#include "ui.h"

namespace {

const int SCREEN_W = M.screenW;

enum Mode { MODE_NONE, MODE_NOT_CONFIGURED, MODE_NO_DATA, MODE_READING };

// Top band (like Gluco-Monitor's): three cells, on a light grey card on the LCD. All sizes come from the profile (profile.cpp).

// The blocks, in the order of their settings: top row, middle row, bottom row.
const int N_KEYS = 7;   // the blocks of the settings
const int N_CELLS = 8;  // plus the clock that "Time and glucose graph" puts left of the graph in the bottom row
const char* const CELL_KEYS[N_KEYS] = {"blk_tl", "blk_tm", "blk_tr", "blk_ml", "blk_mc", "blk_mr", "blk_bottom"};
Cell cells[N_CELLS];  // their addresses must stay put (the arrow's draw callback points at one)
lv_obj_t* errorLabel = nullptr;

// What is currently on the panel.
struct Shown {
  Mode mode = MODE_NONE;
  String name, url, code, error, layout;
} shown;
CellView view;

// The arguments of the last drawDashboard(), to redraw when the settings screen closes.
struct LastArgs {
  Reading last;
  Deltas d;
  String error, url, code;
  bool notConfigured = false, have = false;
} lastArgs;
bool overlay = false;  // the settings screen is up

// Left edge of column i (0..3) in a row `w` wide: three columns, the middle one M.centerW wide when that is set.
int colX(int w, int i) {
  if (!M.centerW) return i * w / 3;
  int side = (w - M.centerW) / 2;
  return i == 0 ? 0 : i == 1 ? side : i == 2 ? side + M.centerW : w;
}

#ifndef UI_PROFILE_LCD
// Battery level in the top right corner (e-paper only): an outline with a bar as long as the charge, and the percentage.
struct BatteryIcon {
  lv_obj_t *body = nullptr, *nub = nullptr, *bar = nullptr, *label = nullptr;
  int barMaxW = 0;
} bat;

lv_obj_t* batteryRect(lv_obj_t* parent, int x, int y, int w, int h, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

void buildBattery(lv_obj_t* scr) {
  const int W = 32, H = 16, NUB_W = 3, NUB_H = 7, MARGIN = 8, PAD = 3, LABEL_W = 44;
  int x = SCREEN_W - MARGIN - NUB_W - W, y = MARGIN;
  lv_obj_t* body = bat.body = batteryRect(scr, x, y, W, H, 3);  // the outline
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(body, lv_color_black(), 0);
  lv_obj_set_style_border_width(body, 2, 0);
  lv_obj_t* nub = bat.nub = batteryRect(scr, x + W, y + (H - NUB_H) / 2, NUB_W, NUB_H, 1);
  lv_obj_set_style_bg_color(nub, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(nub, 0, 0);
  bat.barMaxW = W - 2 * PAD;
  bat.bar = batteryRect(scr, x + PAD, y + PAD, bat.barMaxW, H - 2 * PAD, 1);
  lv_obj_set_style_bg_color(bat.bar, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(bat.bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bat.bar, 0, 0);
  int lineH = lv_font_get_line_height(M.fCaption);
  bat.label = uiLabel(scr, "", M.fCaption, x - 5 - LABEL_W, y + (H - lineH) / 2);
  lv_obj_set_width(bat.label, LABEL_W);
  lv_obj_set_style_text_align(bat.label, LV_TEXT_ALIGN_RIGHT, 0);
}

void updateBattery() {
  int pct = batteryUsbPowered() ? -1 : batteryPercent();  // no icon while the USB is plugged in
  lv_obj_t* parts[] = {bat.body, bat.nub, bat.bar, bat.label};
  for (lv_obj_t* o : parts) {
    if (pct < 0) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  if (pct < 0) return;
  lv_obj_set_width(bat.bar, std::max(1, bat.barMaxW * pct / 100));
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  if (strcmp(buf, lv_label_get_text(bat.label))) lv_label_set_text(bat.label, buf);
}
#else
void buildBattery(lv_obj_t*) {}
void updateBattery() {}
#endif

// Everything that needs the screen to be rebuilt when it changes.
String layoutSignature() {
  String sig = cfgGet("tz") + "|" + cfgGet("lang");
  for (int i = 0; i < N_KEYS; i++) sig += "|" + cfgGet(CELL_KEYS[i]);
  return sig;
}

void setError(const String& error) {
  String text = error.length() ? "! " + error : String();
  if (text == lv_label_get_text(errorLabel)) return;  // keep refreshes small
  lv_label_set_text(errorLabel, text.c_str());
  // The text sits over the bottom of the graph on the e-paper: a white background keeps it readable.
  lv_obj_set_style_bg_opa(errorLabel, text.length() ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

bool night = false;

// Dashboard screens (a reading, or no data yet) are inverted at night; screens with QR codes never are.
bool invertWanted(Mode m) { return night && (m == MODE_READING || m == MODE_NO_DATA); }

void updateCells() {
  for (Cell& c : cells) cellUpdate(c, view);
  updateBattery();
}

void buildReadingScreen() {
  lv_obj_t* scr = uiNewScreen();

  lv_obj_t* band = lv_obj_create(scr);
  lv_obj_set_pos(band, M.bandX, M.bandY);
  lv_obj_set_size(band, M.bandW, M.bandH);
#ifdef UI_PROFILE_LCD
  lv_obj_set_style_bg_color(band, UI_GREY, 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
#else
  lv_obj_set_style_bg_opa(band, LV_OPA_TRANSP, 0);  // e-paper: no grey fill, just the separators
#endif
  lv_obj_set_style_radius(band, M.bandRadius, 0);
  lv_obj_set_style_border_width(band, 0, 0);
  lv_obj_set_style_pad_all(band, 0, 0);
  lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 1; i < 3; i++) {  // separators
    lv_obj_t* sep = lv_obj_create(band);
    lv_obj_set_pos(sep, colX(M.bandW, i), M.bandSepInset);
    lv_obj_set_size(sep, 1, M.bandH - 2 * M.bandSepInset);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
  }

  bool split = cfgGet("blk_bottom") == "timegraph";  // bottom row: the clock on the left, the graph on the right
  int splitX = colX(SCREEN_W, 1);                    // the clock is as wide as the first column above it
  for (int i = 0; i < N_KEYS; i++) {
    Cell& c = cells[i];
    c = Cell();
    c.type = cellTypeFromString(cfgGet(CELL_KEYS[i]));
    if (i < 3) {  // top row: inside the band
      c.parent = band;
      c.x = colX(M.bandW, i);
      c.y = 0;
      c.w = colX(M.bandW, i + 1) - c.x;
      c.h = M.bandH;
    } else if (i < 6) {  // middle row
      c.parent = scr;
      c.x = colX(SCREEN_W, i - 3);
      c.w = colX(SCREEN_W, i - 2) - c.x;
      c.y = M.midY;
      c.h = M.midH;
    } else {  // bottom row
      if (split) c.type = CELL_GRAPH;
      c.parent = scr;
      c.x = split ? splitX : 0;
      c.y = M.graphY;
      c.w = SCREEN_W - c.x;
      c.h = M.graphH;
    }
    cellBuild(c);
  }
  Cell& clock = cells[N_KEYS];
  clock = Cell();
  if (split) {
    clock.type = CELL_CLOCK;
    clock.parent = scr;
    clock.x = 0;
    clock.y = M.graphY;
    clock.w = splitX;
    clock.h = M.graphH;
    cellBuild(clock);
  }

  errorLabel = uiLabel(scr, "", M.fCaption, M.edge, M.stripY);  // as wide as its text, up to the screen width
  lv_label_set_long_mode(errorLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_max_width(errorLabel, SCREEN_W - 2 * M.edge, 0);
  lv_obj_set_style_bg_color(errorLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(errorLabel, LV_OPA_TRANSP, 0);
  buildBattery(scr);  // last: on top of the blocks
}

}  // namespace

// ---------- public ----------

void drawDashboard(const Reading& last, const Deltas& d, const String& error,
                   bool notConfigured, const String& configUrl, const String& configCode) {
  lastArgs = {last, d, error, configUrl, configCode, notConfigured, true};
  if (overlay) return;
  Mode mode = notConfigured ? MODE_NOT_CONFIGURED : last.valid ? MODE_READING : MODE_NO_DATA;
  String name = sourceName();
  String layout = layoutSignature();
  view.last = last;
  view.deltas = d;
  view.mmol = cfgGet("unit") == "mmol";
  view.low = atoi(cfgGet("low").c_str());
  view.high = atoi(cfgGet("high").c_str());
  view.hba1cMmolMol = cfgGet("hba1c_unit") == "mmolmol";

  bool layoutChanged = shown.mode == MODE_NONE || mode != shown.mode || layout != shown.layout;
  portSetInvert(invertWanted(mode));  // before the screen is (re)built, so its first frame already has the right colours

  if (mode == MODE_READING && !layoutChanged) {
    updateCells();  // each block only touches what changed
    setError(error);
    portRefresh(false);  // refreshes only if something was redrawn
  } else {
    bool urlChanged = mode == MODE_NOT_CONFIGURED && (configUrl != shown.url || configCode != shown.code);
    bool nameChanged = mode != MODE_READING && name != shown.name;
    bool errorChanged = error != shown.error;
    if (!layoutChanged && !errorChanged && !urlChanged && !nameChanged) return;
    if (mode == MODE_READING) {
      buildReadingScreen();
      updateCells();
      setError(error);
      portRefresh(true);
    } else if (mode == MODE_NOT_CONFIGURED) {
      uiShowNotConfigured(name, configUrl, configCode);  // these draw and refresh the screen themselves
    } else {
      uiShowNoData(name, error);
    }
  }

  shown.mode = mode;
  shown.name = name;
  shown.url = configUrl;
  shown.code = configCode;
  shown.error = error;
  shown.layout = layout;
}

void dashboardShowConfig(const String& url, const String& code) {
  overlay = true;
  portSetInvert(false);  // QR codes stay black on white
  lastArgs.url = url;  // the dashboard is redrawn with the new code when this screen closes
  lastArgs.code = code;
  uiShowConfigQr(url, code);
}

void dashboardShowUpdate(int percent) {
  if (!overlay) {
    overlay = true;
    portSetInvert(false);
  }
  uiShowUpdate(percent);
}

void dashboardHideConfig() {
  if (!overlay) return;
  overlay = false;
  shown.mode = MODE_NONE;  // the dashboard widgets are gone: rebuild
  if (lastArgs.have) drawDashboard(lastArgs.last, lastArgs.d, lastArgs.error, lastArgs.notConfigured, lastArgs.url, lastArgs.code);
}

void dashboardTick() {
  if (overlay || shown.mode != MODE_READING) return;
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

#ifndef UI_PROFILE_LCD
  // E-paper: every redraw is a visible flash, so the clock, the reading age and the freshness bar all move together,
  // once per clock minute (a reading's own minutes would tick at a different second and add redraws).
  static long lastMinute = -1;
  long minute = (long)(time(nullptr) / 60);
  if (minute == lastMinute) return;
  lastMinute = minute;
#endif

  updateCells();  // clock, reading age and freshness bar move with time
  portRefresh(false);
}

void dashboardSetNight(bool n) {
  night = n;
  bool want = !overlay && invertWanted(shown.mode);
  if (want != portInverted()) {
    portSetInvert(want);
    portRefresh(true);  // redraw everything in the new colours (a full refresh on e-paper)
  }
}
