#include "ui.h"
#include "lvgl_port.h"
#include "wifi_setup.h"  // AP_NAME

lv_obj_t* uiNewScreen() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, lv_color_black(), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_obj_t* uiLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int x, int y, int width) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_pos(l, x, y);
  if (width > 0) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l, width);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  return l;
}

lv_obj_t* uiQR(lv_obj_t* parent, int x, int y, int size, const char* text) {
  const int quiet = 12;
  lv_obj_t* qr = lv_qrcode_create(parent);
  lv_qrcode_set_size(qr, size - 2 * quiet);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, text, strlen(text));
  lv_obj_set_pos(qr, x, y);
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, quiet, 0);
  return qr;
}

void uiShowErasePrompt(const String& ssid) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, "Cannot join WiFi", &lv_font_montserrat_32, 20, 20);
  uiLabel(s, ("Saved network: " + ssid).c_str(), &lv_font_montserrat_16, 20, 80);
  uiLabel(s, "Erase it and set up a new WiFi?", &lv_font_montserrat_16, 20, 130);
  uiLabel(s, "KEY0 (green): ERASE and set up new WiFi", &lv_font_montserrat_16, 20, 200);
  uiLabel(s, "KEY1: retry saved WiFi", &lv_font_montserrat_16, 20, 240);
  uiLabel(s, "(retries automatically in 60 s)", &lv_font_montserrat_16, 20, 300);
  portRefresh(true);
}

void uiShowPortal() {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, "WiFi setup", &lv_font_montserrat_32, 20, 20);
  uiLabel(s, "1. Join WiFi " AP_NAME, &lv_font_montserrat_16, 20, 70);
  uiQR(s, 56, 110, 280, "WIFI:T:nopass;S:" AP_NAME ";;");
  uiLabel(s, "2. Open setup page", &lv_font_montserrat_16, 420, 70);
  uiQR(s, 456, 110, 280, "http://192.168.4.1");
  uiLabel(s, "(or browse to 192.168.4.1)", &lv_font_montserrat_16, 420, 420);
  portRefresh(true);
}

void uiShowConnected(const String& ssid, const String& ip, const String& url) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, "WiFi connected", &lv_font_montserrat_32, 20, 20);
  uiLabel(s, ("SSID: " + ssid).c_str(), &lv_font_montserrat_16, 20, 70);
  uiLabel(s, ("IP: " + ip).c_str(), &lv_font_montserrat_16, 20, 100);
  uiLabel(s, "Configure the panel at:", &lv_font_montserrat_16, 20, 170);
  uiLabel(s, url.c_str(), &lv_font_montserrat_24, 20, 200);
  uiLabel(s, "or http://glucopanel.local", &lv_font_montserrat_16, 20, 240);
  uiQR(s, 500, 60, 280, url.c_str());
  portRefresh(true);
}

void uiShowWifiFailed() {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, "WiFi setup failed", &lv_font_montserrat_32, 20, 20);
  portRefresh(true);
}
