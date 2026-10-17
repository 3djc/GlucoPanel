#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Light grey used for filled areas. The panel is black/white only, so lvgl_port.cpp renders exactly
// this colour as a sparse dither pattern; every other colour is thresholded to black or white.
#define UI_GREY lv_color_hex(0xE0E0E0)

// UI text: Montserrat Medium, ASCII + Latin-1 (48 px: ASCII only, it only shows numbers).
LV_FONT_DECLARE(font_ui_14);
LV_FONT_DECLARE(font_ui_16);
LV_FONT_DECLARE(font_ui_24);
LV_FONT_DECLARE(font_ui_32);
LV_FONT_DECLARE(font_ui_48);

// Big digits for the glucose value (Montserrat Medium, digits and . - + only).
LV_FONT_DECLARE(font_big_128);
LV_FONT_DECLARE(font_big_96);

// Applies the time zone chosen in the settings to localtime() (cheap when unchanged).
void uiApplyTimeZone();

// Empties the active screen (white background, black text, no scrolling) and returns it.
lv_obj_t* uiNewScreen();

// A black label at (x, y) of `parent`. If `width` > 0 the label is that wide, centred and clipped.
lv_obj_t* uiLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int x, int y, int width = 0);

// A QR code of about `size` px with its top-left corner at (x, y), including a white quiet zone.
lv_obj_t* uiQR(lv_obj_t* parent, int x, int y, int size, const char* text);

// ---- WiFi screens (each one replaces the whole display and does a full refresh) ----
void uiShowErasePrompt(const String& ssid);
void uiShowPortal();  // hotspot + captive portal: QR codes to join and to open the setup page
void uiShowConnected(const String& ssid, const String& ip, const String& url);
void uiShowWifiFailed();

// Settings screen opened by a short press of the green button: QR code and address of the settings page.
void uiShowConfigQr(const String& url);
// Shown after a long press, just before the saved WiFi is erased and the panel restarts in setup mode.
void uiShowWifiErased();
