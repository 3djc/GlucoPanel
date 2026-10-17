#include "lvgl_port.h"
#include <Arduino.h>
#include <lvgl.h>
#include <stdlib.h>
#include "profile.h"
#include "ui.h"
#ifdef UI_PROFILE_LCD
#include "panel.h"
#else
#include "TFT_eSPI.h"
#endif
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

// Union of the areas flushed since the last panel refresh.
static lv_area_t dirty;
static bool hasDirty = false;
static lv_display_t* display;
static uint8_t* drawBuffer;

static void markDirty(const lv_area_t* area) {
  if (!hasDirty) {
    dirty = *area;
    hasDirty = true;
  } else {
    dirty.x1 = LV_MIN(dirty.x1, area->x1);
    dirty.y1 = LV_MIN(dirty.y1, area->y1);
    dirty.x2 = LV_MAX(dirty.x2, area->x2);
    dirty.y2 = LV_MAX(dirty.y2, area->y2);
  }
}

static uint32_t tickCb() { return millis(); }

// LVGL renders `bufferLines` screen lines at a time (partial render mode) into one RGB565 buffer.
static void createDisplay(int bufferLines, bool preferPsram, lv_display_flush_cb_t flush) {
  size_t bytes = (size_t)M.screenW * bufferLines * sizeof(uint16_t);
#ifdef ARDUINO
  if (preferPsram) drawBuffer = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (!drawBuffer) drawBuffer = (uint8_t*)malloc(bytes);
  if (!drawBuffer) {
    Serial.println("LVGL buffer allocation failed.");
    while (true) delay(1000);
  }
  display = lv_display_create(M.screenW, M.screenH);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, drawBuffer, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

#ifdef UI_PROFILE_LCD
// ---------------------------------------------------------------------------------------------------
// Colour LCD: the flush callback copies each rendered area into the panel's frame buffer, and
// portRefresh() pushes the frame buffer to the screen when something was drawn. A full frame takes
// ~50 ms on the JC3248W535, so there is no partial / full refresh policy and no dithering.
// ---------------------------------------------------------------------------------------------------
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  panelDraw(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (const uint16_t*)px);
  markDirty(area);
  lv_display_flush_ready(disp);
}

void portBegin() {
  panelBegin();
  lv_init();
  lv_tick_set_cb(tickCb);
  createDisplay(40, false, flushCb);  // 480 x 40 x 2 = 38 KB in internal RAM
}

void portRefresh(bool forceFull) {
  if (forceFull) lv_obj_invalidate(lv_screen_active());
  hasDirty = false;
  lv_refr_now(display);
  if (forceFull || hasDirty) panelPresent();
}

#else
// ---------------------------------------------------------------------------------------------------
// e-paper (reTerminal E1001)
// ---------------------------------------------------------------------------------------------------
// ePaper policy: a full refresh flashes and wears the panel, so it is only used when the screen
// layout changes and every FULL_REFRESH_MS to clear ghosting. Everything else (new reading, age
// text, error text) is a single partial refresh of the smallest window covering what changed.
#ifndef FULL_REFRESH_MS
#define FULL_REFRESH_MS (30UL * 60UL * 1000UL)
#endif

// The E1001's real 4-level gray mode has no partial refresh, so UI_GREY fills are drawn as a sparse
// ordered-dither pattern in plain black/white. LIGHT_GREY_DOTS is the number of black pixels per
// 4x4 cell (0..16).
#ifndef LIGHT_GREY_DOTS
#define LIGHT_GREY_DOTS 2
#endif

#define BLACK_BELOW 128  // luminance (0..255) under which a pixel is drawn black

static EPaper epaper;
static unsigned long lastFullMs = 0;

static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  static const uint16_t grey = lv_color_to_u16(UI_GREY);
  const int w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
  const uint16_t* p = (const uint16_t*)px;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++, p++) {
      uint16_t c = *p;
      int ax = area->x1 + x, ay = area->y1 + y;
      bool black;
      if (c == grey) {
        black = bayer[ay & 3][ax & 3] < LIGHT_GREY_DOTS;
      } else {
        uint32_t r = ((c >> 11) & 0x1F) * 255 / 31, g = ((c >> 5) & 0x3F) * 255 / 63, b = (c & 0x1F) * 255 / 31;
        black = (r * 30 + g * 59 + b * 11) / 100 < BLACK_BELOW;
      }
      epaper.drawPixel(ax, ay, black ? TFT_BLACK : TFT_WHITE);
    }
  }
  markDirty(area);
  lv_display_flush_ready(disp);
}

void portBegin() {
  epaper.begin();
  epaper.fillScreen(TFT_WHITE);
  lv_init();
  lv_tick_set_cb(tickCb);
  createDisplay(24, true, flushCb);  // 800 x 24 x 2 = 38 KB, PSRAM when there is some
}

void portRefresh(bool forceFull) {
  if (forceFull) lv_obj_invalidate(lv_screen_active());
  hasDirty = false;
  lv_refr_now(display);  // render synchronously: the flush callback fills the panel framebuffer
  if (!forceFull && !hasDirty) return;

  if (forceFull || millis() - lastFullMs >= FULL_REFRESH_MS) {
    epaper.update();
    lastFullMs = millis();
  } else {
    epaper.updataPartial(dirty.x1, dirty.y1, dirty.x2 - dirty.x1 + 1, dirty.y2 - dirty.y1 + 1);
  }
}

#endif  // UI_PROFILE_LCD
