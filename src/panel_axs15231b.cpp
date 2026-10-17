// Guition JC3248W535: 3.5" 320x480 IPS, AXS15231B controller on a QSPI bus (device only).
// Pins and behaviour confirmed with tools/bringup_jc3248w535: a full frame takes ~50 ms. The controller is
// driven through a full-size canvas (in PSRAM) that is pushed whole: some batches of this panel ignore
// window-address commands, so partial updates cannot be relied on. The canvas also does the rotation to
// landscape (480x320), which the panel has no hardware for.
#if defined(ARDUINO) && defined(UI_PROFILE_LCD)
#include "panel.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

static const int PIN_BACKLIGHT = 1;

static Arduino_Canvas* canvas = nullptr;
static bool shown = false;      // the first frame has been pushed: the backlight may come on
static int backlightPercent = 100;

static void applyBacklight() { ledcWrite(PIN_BACKLIGHT, shown ? backlightPercent * 255 / 100 : 0); }

void panelBegin() {
  ledcAttach(PIN_BACKLIGHT, 5000, 8);  // PWM: 5 kHz, 8 bits
  ledcWrite(PIN_BACKLIGHT, 0);          // dark until the first frame is ready: no flash of garbage
  Arduino_DataBus* bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
  Arduino_GFX* panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
  canvas = new Arduino_Canvas(320, 480, panel, 0, 0, 0);
  if (!canvas->begin()) Serial.println("display begin FAILED");
  canvas->setRotation(1);  // landscape, USB connector on the left (as in the bring-up test)
  canvas->fillScreen(0xFFFF);
}

void panelDraw(int x, int y, int w, int h, const uint16_t* rgb565) {
  canvas->draw16bitRGBBitmap(x, y, const_cast<uint16_t*>(rgb565), w, h);
}

void panelPresent() {
  canvas->flush();
  if (!shown) {
    shown = true;
    applyBacklight();
  }
}

void panelBacklight(int percent) {
  backlightPercent = constrain(percent, 0, 100);
  applyBacklight();
}
#endif
