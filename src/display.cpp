#include "display.h"
#include <Arduino.h>
#include "cells.h"
#include "config.h"
#include "i18n.h"
#include "lvgl_port.h"
#include "night.h"
#include "profile.h"
#include "ui.h"

namespace {

const int SCREEN_W = M.screenW;

enum Mode { MODE_NONE, MODE_NOT_CONFIGURED, MODE_NO_DATA, MODE_READING };

// Top band (like Gluco-Monitor's): rounded, light grey, three cells. All sizes come from the profile (profile.cpp).
const int BAND_CELL_W = M.bandW / 3;

// The blocks, in the order of their settings: top row, middle row, bottom row.
const int N_CELLS = 7;
const char* const CELL_KEYS[N_CELLS] = {"blk_tl", "blk_tm", "blk_tr", "blk_ml", "blk_mc", "blk_mr", "blk_bottom"};
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

// Everything that needs the screen to be rebuilt when it changes.
String layoutSignature() {
  String sig = cfgGet("tz") + "|" + cfgGet("lang");
  for (int i = 0; i < N_CELLS; i++) sig += "|" + cfgGet(CELL_KEYS[i]);
  return sig;
}

void setError(const String& error) {
  String text = error.length() ? "! " + error : String();
  if (text != lv_label_get_text(errorLabel)) lv_label_set_text(errorLabel, text.c_str());  // keep refreshes small
}

bool night = false;

// Dashboard screens (a reading, or no data yet) are inverted at night; screens with QR codes never are.
bool invertWanted(Mode m) { return night && (m == MODE_READING || m == MODE_NO_DATA); }

void updateCells() {
  for (Cell& c : cells) cellUpdate(c, view);
}

void buildReadingScreen() {
  lv_obj_t* scr = uiNewScreen();

  lv_obj_t* band = lv_obj_create(scr);
  lv_obj_set_pos(band, M.bandX, M.bandY);
  lv_obj_set_size(band, M.bandW, M.bandH);
  lv_obj_set_style_bg_color(band, UI_GREY, 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(band, M.bandRadius, 0);
  lv_obj_set_style_border_width(band, 0, 0);
  lv_obj_set_style_pad_all(band, 0, 0);
  lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 1; i < 3; i++) {  // separators
    lv_obj_t* sep = lv_obj_create(band);
    lv_obj_set_pos(sep, i * BAND_CELL_W, M.bandSepInset);
    lv_obj_set_size(sep, 1, M.bandH - 2 * M.bandSepInset);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
  }

  for (int i = 0; i < N_CELLS; i++) {
    Cell& c = cells[i];
    c = Cell();
    c.type = cellTypeFromString(cfgGet(CELL_KEYS[i]));
    if (i < 3) {  // top row: inside the band
      c.parent = band;
      c.x = i * BAND_CELL_W;
      c.y = 0;
      c.w = BAND_CELL_W;
      c.h = M.bandH;
    } else if (i < 6) {  // middle row
      c.parent = scr;
      c.x = (i - 3) * SCREEN_W / 3;
      c.w = (i - 2) * SCREEN_W / 3 - c.x;
      c.y = M.midY;
      c.h = M.midH;
      c.middleRow = true;
    } else {  // bottom row
      c.parent = scr;
      c.x = 0;
      c.y = M.graphY;
      c.w = SCREEN_W;
      c.h = M.graphH;
    }
    cellBuild(c);
  }

  errorLabel = uiLabel(scr, "", M.fCaption, M.edge, M.stripY);
  lv_label_set_long_mode(errorLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(errorLabel, SCREEN_W - 2 * M.edge);
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
