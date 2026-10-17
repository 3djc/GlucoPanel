#include "ui.h"
#include <time.h>
#include "config.h"
#include "i18n.h"
#include "lvgl_port.h"
#include "profile.h"
#include "wifi_setup.h"  // AP_NAME

// A black label that wraps at `width`.
static lv_obj_t* wrapLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int x, int y, int width) {
  lv_obj_t* l = uiLabel(parent, text, font, x, y);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, width);
  return l;
}

void uiApplyTimeZone() {
  static String applied;
  String tz = cfgGet("tz");
  if (tz == applied) return;
  setenv("TZ", tz.c_str(), 1);
  tzset();
  applied = tz;
}

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
  const int quiet = size / 23;  // white border around the code (about 2 modules): scanners need it
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

String uiConfigLink(const String& url, const String& code) { return url + "/?k=" + code; }

// All the screens below are laid out in design coordinates (the 800x480 layout, see profile.h) and scaled.
static int qrX() { return M.screenW - M.qr - dx(20); }  // QR code on the right, 20 design px from the edge

int uiShowAddressAndCode(lv_obj_t* parent, int x, int y, const String& url, const String& code) {
  uiLabel(parent, url.c_str(), M.fMid, x, y);
  uiLabel(parent, (String(T("Code:")) + " " + code).c_str(), M.fTitle, x, y + dy(36));
  return y + dy(36) + dy(44);
}

#ifdef UI_PROFILE_LCD
// ---- on-screen buttons (LCD profile) ----
struct Rect { int x, y, w, h; };
static Rect eraseRect = {0, 0, 0, 0}, retryRect = {0, 0, 0, 0};
static lv_obj_t* eraseFill = nullptr;

static bool inside(const Rect& r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

// A rounded outlined button with a centred label; returns the "fill" object behind the label (grows with
// uiConfigEraseProgress on the hold-to-erase button).
static lv_obj_t* button(lv_obj_t* parent, const Rect& r, const char* text, const lv_font_t* font) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_set_pos(b, r.x, r.y);
  lv_obj_set_size(b, r.w, r.h);
  lv_obj_set_style_bg_color(b, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(b, lv_color_black(), 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* fill = lv_obj_create(b);
  lv_obj_set_pos(fill, 0, 0);
  lv_obj_set_size(fill, 0, r.h);
  lv_obj_set_style_bg_color(fill, lv_color_hex(0xB4B4B4), 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(fill, 0, 0);
  lv_obj_set_style_radius(fill, 8, 0);
  lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, r.w - 16);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
  return fill;
}

int uiErasePromptHit(int x, int y) { return inside(eraseRect, x, y) ? 1 : inside(retryRect, x, y) ? 2 : 0; }
bool uiConfigEraseHit(int x, int y) { return inside(eraseRect, x, y); }

void uiConfigEraseProgress(int percent) {
  if (!eraseFill) return;
  lv_obj_set_width(eraseFill, eraseRect.w * LV_CLAMP(0, percent, 100) / 100);
  portRefresh(false);
}

void uiShowErasePrompt(const String& ssid) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("Cannot join WiFi"), M.fTitle, dx(20), dy(20));
  uiLabel(s, (String(T("Saved network:")) + " " + ssid).c_str(), M.fCaption, dx(20), dy(90));
  uiLabel(s, T("Erase it and set up a new WiFi?"), M.fCaption, dx(20), dy(140));
  uiLabel(s, T("(retries automatically in 60 s)"), M.fCaption, dx(20), dy(240));
  eraseRect = {dx(20), dy(330), dx(500), dy(110)};
  retryRect = {dx(540), dy(330), dx(240), dy(110)};
  button(s, eraseRect, T("Erase and set up new WiFi"), M.fMid);
  button(s, retryRect, T("Retry"), M.fMid);
  portRefresh(true);
}
#else
void uiShowErasePrompt(const String& ssid) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("Cannot join WiFi"), M.fTitle, dx(20), dy(20));
  uiLabel(s, (String(T("Saved network:")) + " " + ssid).c_str(), M.fCaption, dx(20), dy(80));
  uiLabel(s, T("Erase it and set up a new WiFi?"), M.fCaption, dx(20), dy(130));
  uiLabel(s, T("KEY0 (green): ERASE and set up new WiFi"), M.fCaption, dx(20), dy(200));
  uiLabel(s, T("KEY1: retry saved WiFi"), M.fCaption, dx(20), dy(240));
  uiLabel(s, T("(retries automatically in 60 s)"), M.fCaption, dx(20), dy(300));
  portRefresh(true);
}
#endif

void uiShowPortal(const String& hotspotPassword) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("WiFi setup"), M.fTitle, dx(20), dy(20));
  char join[96];
  snprintf(join, sizeof(join), T("1. Join WiFi %s"), AP_NAME);
  uiLabel(s, join, M.fCaption, dx(20), dy(70));
  int y = dy(110);
  uiQR(s, dx(56), y, M.qr, (String("WIFI:T:WPA;S:") + AP_NAME + ";P:" + hotspotPassword + ";;").c_str());
  uiLabel(s, (String(T("Password")) + ": " + hotspotPassword).c_str(), M.fMid, dx(56), y + M.qr + dy(6));
  uiLabel(s, T("2. Open setup page"), M.fCaption, dx(420), dy(70));
  uiQR(s, dx(456), y, M.qr, "http://192.168.4.1");
  uiLabel(s, T("(or browse to 192.168.4.1)"), M.fCaption, dx(420), y + M.qr + dy(6));
  portRefresh(true);
}

void uiShowConnected(const String& ssid, const String& ip, const String& url, const String& code) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("WiFi connected"), M.fTitle, dx(20), dy(20));
  uiLabel(s, ("SSID: " + ssid).c_str(), M.fCaption, dx(20), dy(70));
  uiLabel(s, ("IP: " + ip).c_str(), M.fCaption, dx(20), dy(100));
  uiLabel(s, T("Configure the panel at:"), M.fCaption, dx(20), dy(170));
  int next = uiShowAddressAndCode(s, dx(20), dy(200), url, code);
  uiLabel(s, (String(T("or")) + " http://glucopanel.local").c_str(), M.fCaption, dx(20), next);
  uiQR(s, qrX(), dy(60), M.qr, uiConfigLink(url, code).c_str());
  portRefresh(true);
}

void uiShowConfigQr(const String& url, const String& code) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("Settings"), M.fTitle, dx(20), dy(20));
  wrapLabel(s, T("Scan to open the settings page:"), M.fCaption, dx(20), dy(100), dx(440));
  int next = uiShowAddressAndCode(s, dx(20), dy(170), url, code);
  uiLabel(s, (String(T("or")) + " http://glucopanel.local").c_str(), M.fCaption, dx(20), next);
  uiQR(s, qrX(), dy(60), M.qr, uiConfigLink(url, code).c_str());
#ifdef UI_PROFILE_LCD
  eraseRect = {dx(20), dy(370), dx(520), dy(80)};
  eraseFill = button(s, eraseRect, T("Hold 3 s to erase the WiFi"), M.fMid);
  wrapLabel(s, T("Tap anywhere else to close"), M.fCaption, dx(20), dy(320), dx(500));
#else
  wrapLabel(s, T("Hold the green button for 3 s to erase the WiFi"), M.fCaption, dx(20), dy(400), dx(760));
  wrapLabel(s, T("Press any button to close"), M.fCaption, dx(20), dy(432), dx(760));
#endif
  portRefresh(true);
}

void uiShowWifiErased() {
  lv_obj_t* s = uiNewScreen();
  char msg[200];
  snprintf(msg, sizeof(msg), T("WiFi erased. Rebooting: join the %s hotspot to set it up again."), AP_NAME);
  wrapLabel(s, msg, M.fMid, dx(20), dy(100), dx(760));
  portRefresh(true);
}

void uiShowWifiFailed() {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, T("WiFi setup failed"), M.fTitle, dx(20), dy(20));
  portRefresh(true);
}

void uiShowNotConfigured(const String& name, const String& url, const String& code) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, name.c_str(), M.fTitle, dx(20), dy(20));
  uiLabel(s, T("Not configured"), M.fTitle, dx(20), dy(100));
  uiLabel(s, T("Open this page to choose your"), M.fCaption, dx(20), dy(160));
  uiLabel(s, T("glucose data source:"), M.fCaption, dx(20), dy(190));
  uiShowAddressAndCode(s, dx(20), dy(240), url, code);
  uiQR(s, qrX(), dy(60), M.qr, uiConfigLink(url, code).c_str());
  portRefresh(true);
}

void uiShowNoData(const String& name, const String& error) {
  lv_obj_t* s = uiNewScreen();
  uiLabel(s, name.c_str(), M.fTitle, dx(20), dy(20));
  uiLabel(s, T("No data yet"), M.fTitle, dx(20), dy(100));
  if (error.length()) uiLabel(s, ("! " + error).c_str(), M.fCaption, dx(20), dy(440));
  portRefresh(true);
}
